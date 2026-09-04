/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <queue>
#include <float.h>

#include "half.h"
#include "priv/Emulator.h"
#include "priv/Check.h"
#include "ErrorMacros.h"

using namespace half_float;

namespace nvdla
{
namespace priv
{

#define POOL_MODE_AVG		0
#define POOL_MODE_MAX		1
#define POOL_MODE_MIN		2

#define SDP_OP_NONE		0
#define SDP_OP_MUL		1
#define SDP_OP_ALU		2
#define SDP_OP_BOTH		3

#define SDP_ALU_OP_MAX		0
#define SDP_ALU_OP_MIN		1
#define SDP_ALU_OP_SUM		2
#define SDP_ALU_OP_EQL		3

#define SDP_OP_PER_LAYER	0
#define SDP_OP_PER_KERNEL	1
#define SDP_OP_PER_POINT	2

#define ACTIVATION_NONE		0
#define ACTIVATION_RELU		1
#define ACTIVATION_LUT		2
#define ACTIVATION_PRELU	3

/* rubik mode */
#define RUBIK_MODE_CONTRACT	0
#define RUBIK_MODE_SPLIT	1
#define RUBIK_MODE_MERGE	2

uint32_t WEIGHT_ATOM_CUBE_SIZE = 128;
uint32_t ELEMENT_SIZE = 2;
uint32_t MAC_ATOMIC_K = 16;

Emulator::Emulator() :
        m_thread(),
        m_threadActive(false),
        m_signalShutdown(false)
{

}

Emulator::~Emulator()
{

}

bool Emulator::ping()
{
    return m_threadActive;
}

NvDlaError Emulator::submit(NvU8* task_mem, bool blocking)
{
    m_taskQueue.push(task_mem);

    if (blocking) {
        // wait until queue becomes empty
        while (!m_taskQueue.empty()) {
            NvDlaThreadYield();
        }
    }

    return NvDlaSuccess;
}

NvDlaError Emulator::start()
{
    NvDlaError e = NvDlaSuccess;

    PROPAGATE_ERROR_FAIL(NvDlaThreadCreate(threadFunction, this, &m_thread), "Failed to create thread");

    return NvDlaSuccess;

fail:
    return e;
}

void Emulator::threadFunction(void* arg)
{
    Emulator* engine = static_cast<Emulator*>(arg);
    engine->run();
}

bool Emulator::stop()
{
    bool ok = true;

    if (m_thread)
    {
        m_signalShutdown = true;
        NvDlaThreadJoin(m_thread);
        m_thread = NULL;
    }

    return ok;
}

bool Emulator::run()
{
    bool ok = true;
    m_threadActive = true;

    EMUInterface* emu_if = new EMUInterfaceA();

    NvDlaDebugPrintf("Emulator starting\n");

    while (true)
    {
        if (!m_taskQueue.empty())
        {
            NvU8* task_mem = m_taskQueue.front();
            NvDlaDebugPrintf("Work Found!\n");

            EMUTaskDescAccessor task_desc = emu_if->taskDescAccessor(task_mem);

            NvU32 numAddresses = *task_desc.numAddresses();
            std::vector<NvU8*> mappedAddressList;
            mappedAddressList.resize(numAddresses);

            // Replace all mem handles with mapped addresses
            for (NvU32 ii=0; ii<numAddresses; ii++)
            {
                void* base = *((void **)task_desc.addressList(ii).hMem());
                NvU32 offset = *task_desc.addressList(ii).offset();

                if (base == 0) {
                    mappedAddressList[ii] = NULL;
                }
                else {
                    mappedAddressList[ii] = (NvU8*)base + offset;
                }
            }

            // Process the task
            processTask(task_mem, mappedAddressList);
            NvDlaDebugPrintf("Work Done\n");

            m_taskQueue.pop();
            continue;
        }

        if (m_signalShutdown)
        {
            NvDlaDebugPrintf("Shutdown signal received, exiting\n");
            break;
        }

        if (m_taskQueue.empty())
        {
            NvDlaSleepMS(500);
        }
    }

    // Cleanup
    while (!m_taskQueue.empty())
    {
        m_taskQueue.pop();
    }

    delete emu_if;
    m_threadActive = false;
    m_signalShutdown = false;

    return ok;
}

NvDlaError Emulator::processTask(NvU8* task_mem, std::vector<NvU8*> addressList)
{
    NvDlaError e = NvDlaSuccess;
    EMUInterface* emu_if = new EMUInterfaceA();
    EMUTaskDescAccessor task_desc = emu_if->taskDescAccessor(task_mem);
    NVDLA_UNUSED(task_desc);

    // 0 - network descriptor
    EMUNetworkDescAccessor network_desc = emu_if->networkDescAccessor(addressList[0]);
    NvU16 numOperations                 = *network_desc.numOperations();

    NvU8*  operation_container_0        = addressList[*network_desc.operationDescIndex()];
    NvU8*  operation_buffer_container_0 = addressList[*network_desc.operationBufferDescIndex()];

    for ( NvU16 op = 0; op < numOperations; ++op)
    {
        // follow the same technique to obtain op_container and buffer_container accessors for each op as the compiler side
        // this short-cut assumes that the op and buffer containers for all batches were placed contiguous in memory

        EMUOperationContainerAccessor operation_container              = emu_if->operationContainerAccessor(operation_container_0);
        EMUOperationBufferContainerAccessor operation_buffer_container = emu_if->operationBufferContainerAccessor(operation_buffer_container_0);

        // HACK: Borrow softmax's accessor to get at the common descriptor
        EMUCommonOpDescAccessor common_op_desc = operation_container.softmaxOpDescAccessor(op).commonOpDescAccessor();

        if (*common_op_desc.op_type() == 0 /* POWER */)
        {
            EMUPowerOpDescAccessor power_op_desc = operation_container.powerOpDescAccessor(op);
            EMUPowerBufferDescsAccessor power_op_buffer_descs = operation_buffer_container.powerBufferDescsAccessor(op);

            PROPAGATE_ERROR_FAIL(executePower(power_op_desc, common_op_desc, power_op_buffer_descs, addressList));

        } else if (*common_op_desc.op_type() == 1 /* SOFTMAX */) {
            EMUSoftmaxOpDescAccessor softmax_op_desc = operation_container.softmaxOpDescAccessor(op);
            EMUSoftmaxBufferDescsAccessor softmax_op_buffer_descs = operation_buffer_container.softmaxBufferDescsAccessor(op);

            PROPAGATE_ERROR_FAIL(executeSoftmax(softmax_op_desc, common_op_desc, softmax_op_buffer_descs, addressList));

        }  else if (*common_op_desc.op_type() == NVDLA_EMU_OP_CONV /* CONV */) {
            EMUConvOpDescAccessor conv_op_desc = operation_container.convOpDescAccessor(op);
            EMUConvBufferDescsAccessor conv_op_buffer_descs = operation_buffer_container.convBufferDescsAccessor(op);

            // PROPAGATE_ERROR_FAIL(executeConvolution(conv_op_desc, common_op_desc, conv_op_buffer_descs, addressList));
            PROPAGATE_ERROR_FAIL(executeConvolutionNaive(conv_op_desc, common_op_desc, conv_op_buffer_descs, addressList));

        } else if (*common_op_desc.op_type() == NVDLA_EMU_OP_POOL /* POOL */) {
            EMUPoolOpDescAccessor pool_op_desc = operation_container.poolOpDescAccessor(op);
            EMUPoolBufferDescsAccessor pool_op_buffer_descs = operation_buffer_container.poolBufferDescsAccessor(op);

            PROPAGATE_ERROR_FAIL(executePool(pool_op_desc, common_op_desc, pool_op_buffer_descs, addressList));

        } else if (*common_op_desc.op_type() == NVDLA_EMU_OP_SDP /* SDP */) {
            EMUSdpOpDescAccessor sdp_op_desc = operation_container.sdpOpDescAccessor(op);
            EMUSdpBufferDescsAccessor sdp_op_buffer_descs = operation_buffer_container.sdpBufferDescsAccessor(op);

            PROPAGATE_ERROR_FAIL(executeSdp(sdp_op_desc, common_op_desc, sdp_op_buffer_descs, addressList));
        } else {
            NvDlaDebugPrintf("Unknown op type %u\n", *common_op_desc.op_type());
        }
    }
fail:
    return e;
}

NvS8 Emulator::getBpe(EMUBufferDescAccessor buffer)
{
    NvS8 bpe = -1;
    switch(*buffer.format())
    {
        case EMU_FORMAT_FF16:
        case EMU_FORMAT_INT16:
        case EMU_FORMAT_UINT16:
            bpe = 2; break;
        case EMU_FORMAT_INT8:
        case EMU_FORMAT_INT8_8:
        case EMU_FORMAT_UINT8:
            bpe = 1; break;
        default:
            bpe = -1;
    }
    return bpe;
}

NvDlaError Emulator::getAddrOffset(EMUBufferDescAccessor in, NvU32 w, NvU32 h, NvU32 c, NvU32* offset)
{
    NvDlaError e = NvDlaSuccess;

    NvU32 x = 0;
    NvU32 xStride = 0;
    NvU32 cquotient = 0;
    NvU32 cremainder = 0;

    NvS8 bpe = getBpe(in);
    if (bpe < 0)
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter);
    }

    switch(*in.format())
    {
        case EMU_FORMAT_FF16:
        case EMU_FORMAT_INT8:
            x = 32 / bpe;
            xStride = x * bpe;
            cquotient = c / x;
            cremainder = c % x;
            *offset = (cquotient * (*in.surfStride())) + (h * (*in.lineStride())) + (w * xStride) + (cremainder * bpe);
            break;
        case EMU_FORMAT_INT8_8:
            x = 8 / bpe;
            xStride = x * bpe;
            cquotient = c / x;
            cremainder = c % x;
            *offset = (cquotient * (*in.surfStride())) + (h * (*in.lineStride())) + (w * xStride) + (cremainder * bpe);
            break;
        default:
            *offset = 0;
            ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Unsupported input format: %d\n", *in.format());
    }

fail:
    return e;
}

NvDlaError Emulator::executePower
(
    EMUPowerOpDescAccessor opDesc,
    EMUCommonOpDescAccessor commonOpDesc,
    EMUPowerBufferDescsAccessor bufDescs,
    std::vector<NvU8*> addressList
)
{
    NvDlaError e = NvDlaSuccess;
    EMUBufferDescAccessor src = bufDescs.srcDataAccessor();
    EMUBufferDescAccessor dst = bufDescs.dstDataAccessor();

    if ( debugOps() )
    {
        NvDlaDebugPrintf("Processing power [power=%f scale=%f shift=%f]\n", *opDesc.power(), *opDesc.scale(), *opDesc.shift());
        NvDlaDebugPrintf("src format %u\n", *src.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *src.addressIndex(), *src.addressIndexOffset(),
                addressList[*src.addressIndex()], *src.width(), *src.height(), *src.channel(), *src.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *src.lineStride(), *src.surfStride());
        NvDlaDebugPrintf("\tinput scale factor: %f, output scale factor: %f\n", *commonOpDesc.input_scale_factor(), *commonOpDesc.output_scale_factor());

        NvDlaDebugPrintf("dst format %u\n", *dst.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *dst.addressIndex(), *dst.addressIndexOffset(),
                addressList[*dst.addressIndex()], *dst.width(), *dst.height(), *dst.channel(), *dst.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *dst.lineStride(), *dst.surfStride());
    }

    if ( *src.format() != *dst.format() )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU Scale operation with different "
            " src (%d) and dst (%d) formats\n", static_cast<NvU32>(*src.format()),
            static_cast<NvU32>(*dst.format()));
    }

    // Execute
    {
        NvU8* pSrc = addressList[*src.addressIndex()] + *src.addressIndexOffset();
        NvU8* pDst = addressList[*dst.addressIndex()] + *dst.addressIndexOffset();

        NvU32 srcoffset = 0;
        NvU32 dstoffset = 0;

        for (NvU32 channel=0; channel<*src.channel(); channel++)
        {
            for (NvU32 height=0; height<*src.height(); height++)
            {
                for (NvU32 width=0; width<*src.width(); width++)
                {
                    PROPAGATE_ERROR_FAIL(getAddrOffset(src, width, height, channel, &srcoffset));
                    PROPAGATE_ERROR_FAIL(getAddrOffset(dst, width, height, channel, &dstoffset));

                    if (*src.format() == EMU_FORMAT_FF16)
                    {
                        NvF32 x = 0;
                        NvF32 y = 0;

                        half_float::half* srchalfp = reinterpret_cast<half_float::half*>(pSrc + srcoffset);
                        half_float::half* dsthalfp = reinterpret_cast<half_float::half*>(pDst + dstoffset);

                        x = float(*srchalfp);
                        y = powf((*opDesc.shift() + (*opDesc.scale() * x)), *opDesc.power());
                        *dsthalfp = half(y);
                    }
                    else if ((*src.format() == EMU_FORMAT_INT8) || (*src.format() == EMU_FORMAT_INT8_8))
                    {
                        NvF32 x = 0;
                        NvF32 y = 0;

                        NvS8* srcint8p = reinterpret_cast<NvS8*>(pSrc + srcoffset);
                        NvS8* dstint8p = reinterpret_cast<NvS8*>(pDst + dstoffset);

                        x = static_cast<NvF32>(*srcint8p);
                        // scale input for executing in FLOAT land
                        x *= *commonOpDesc.input_scale_factor();
                        y = powf((*opDesc.shift() + (*opDesc.scale() * x)), *opDesc.power());
                        // rescale output to write out in INT8 land
                        y /= *commonOpDesc.output_scale_factor();

                        *dstint8p = saturate<NvF32, NvS8>(y);
                    }
                    else
                    {
                        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU scale operation for format: %d\n",
                            static_cast<NvU32>(*src.format()));
                    }
                }
            }
        }
    }

fail:
    return e;
}


NvDlaError Emulator::executeSoftmax
(
    EMUSoftmaxOpDescAccessor opDesc,
    EMUCommonOpDescAccessor commonOpDesc,
    EMUSoftmaxBufferDescsAccessor bufDescs,
    std::vector<NvU8*> addressList
)
{
    NvDlaError e = NvDlaSuccess;

    EMUBufferDescAccessor src = bufDescs.srcDataAccessor();
    EMUBufferDescAccessor dst = bufDescs.dstDataAccessor();

    if ( debugOps() )
    {
        NvDlaDebugPrintf("Processing softmax [axis=%u]\n", *opDesc.axis());
        NvDlaDebugPrintf("src format %u\n", *src.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *src.addressIndex(), *src.addressIndexOffset(),
                addressList[*src.addressIndex()], *src.width(), *src.height(), *src.channel(), *src.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *src.lineStride(), *src.surfStride());
        NvDlaDebugPrintf("\tinput scale factor: %f, output scale factor: %f\n", *commonOpDesc.input_scale_factor(), *commonOpDesc.output_scale_factor());

        NvDlaDebugPrintf("dst format %u\n", *dst.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *dst.addressIndex(),  *dst.addressIndexOffset(),
                addressList[*dst.addressIndex()], *dst.width(), *dst.height(), *dst.channel(), *dst.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *dst.lineStride(), *dst.surfStride());
    }

    if ( *src.format() != *dst.format() )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU Scale operation with different "
            " src (%d) and dst (%d) formats\n", static_cast<NvU32>(*src.format()),
            static_cast<NvU32>(*dst.format()));
    }

    // Execute
    if (*src.format() == EMU_FORMAT_FF16)
    {
        half* pSrc = reinterpret_cast<half*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
        half* pDst = reinterpret_cast<half*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());

        NvF32 maxval = -INFINITY;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            if (float(pSrc[ii]) > maxval)
            {
                maxval = float(pSrc[ii]);
            }
        }
        NvF32 sumexp = 0.0f;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            sumexp += expf(float(pSrc[ii])-maxval);
        }
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            pDst[ii] = expf(float(pSrc[ii])-maxval) / sumexp;
        }
    }
    else if ((*src.format() == EMU_FORMAT_INT8) || (*src.format() == EMU_FORMAT_INT8_8))
    {
        NvS8* pSrc = reinterpret_cast<NvS8*>( addressList[*src.addressIndex()] + *src.addressIndexOffset() );
        NvS8* pDst = reinterpret_cast<NvS8*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset() );

        half* pHalfSrc = reinterpret_cast<half*>(malloc(*src.channel() * sizeof(half)));
        half* pHalfDst = reinterpret_cast<half*>(malloc(*dst.channel() * sizeof(half)));

        // scale input for processing in FLOAT land
        for (NvU32 ii = 0; ii < *src.channel(); ii++)
        {
            pHalfSrc[ii] = pSrc[ii] * (*commonOpDesc.input_scale_factor());
        }

        NvF32 maxval = -INFINITY;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            if (float(pHalfSrc[ii]) > maxval)
            {
                maxval = float(pHalfSrc[ii]);
            }
        }

        NvF32 sumexp = 0.0f;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            sumexp += expf(float(pHalfSrc[ii])-maxval);
        }
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            pHalfDst[ii] = static_cast<half>(expf(float(pHalfSrc[ii])-maxval) / sumexp);
        }

        // rescale output to write out in INT8 land
        for (NvU32 ii = 0; ii < *dst.channel(); ii++)
        {
            pDst[ii] = saturate<NvF32, NvS8>(pHalfDst[ii] / (*commonOpDesc.output_scale_factor()));
        }

        if (debugPrint())
        {
            NvF32 maxHalfDst = -INFINITY;
            NvU32 maxHalfIndex = -1;
            NvF32 maxIntDst = std::numeric_limits<NvS8>::lowest();
            NvU32 maxIntIndex = -1;

            for (NvU32 ii = 0; ii < *dst.channel(); ii++) {
                if (pHalfDst[ii] > maxHalfDst) {
                    maxHalfDst = pHalfDst[ii];
                    maxHalfIndex = ii;
                }
                if (pDst[ii] > maxIntDst) {
                    maxIntDst = pDst[ii];
                    maxIntIndex = ii;
                }
            }

            NvDlaDebugPrintf("Post-softmax max value: (half) %f, (int) %f\n", maxHalfDst, maxIntDst);
            NvDlaDebugPrintf("at indices (half) %d, (int) %d\n", maxHalfIndex, maxIntIndex);
        }


    }
    else
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU softmax operation for format: %d\n",
            static_cast<NvU32>(*src.format()));
    }

fail:
    return e;
}

typedef half fp16_t;

// Convert a 32-bit float to a 16-bit half using round-to-nearest-even.
fp16_t fp16_from_float(float f) {
    union { float f; uint32_t u; } bits;
    bits.f = f;
    uint32_t u = bits.u;

    uint32_t sign = (u >> 16) & 0x8000u;
    int      exp  = (int)((u >> 23) & 0xFFu) - 127;
    uint32_t mant = u & 0x007FFFFFu;

    // Inf / NaN
    if (exp == 128) {
        if (mant != 0) {
            // NaN - keep some payload bits, ensure quiet NaN
            return (fp16_t)(sign | 0x7E00u | (mant >> 13));
        }
        return (fp16_t)(sign | 0x7C00u); // +/-Inf
    }

    // Exponent + mantissa in half-precision range
    int h_exp  = exp + 15;
    uint32_t h_mant = mant;

    if (h_exp >= 0x1F) {
        // Overflow -> +/-Inf
        return (fp16_t)(sign | 0x7C00u);
    }

    if (h_exp <= 0) {
        // Subnormal or underflow
        if (h_exp < -10) {
            return (fp16_t)sign; // +/-0
        }
        // Subnormal: shift mantissa right and add implicit leading 1
        h_mant = (mant | 0x00800000u) >> (1 - h_exp);

        // Round-to-nearest-even
        uint32_t round_bit = 1u << 12;
        if ((h_mant & round_bit) && (h_mant & ((round_bit << 1) - 1) & ~round_bit
                                      ? 1 : (h_mant & (round_bit << 1)))) {
            h_mant += round_bit;
        }
        return (fp16_t)(sign | (h_mant >> 13));
    }

    // Round-to-nearest-even for normals
    uint32_t round_bit = 1u << 12;
    if ((h_mant & round_bit) && ((h_mant & (round_bit - 1)) || (h_mant & (round_bit << 1)))) {
        h_mant += round_bit;
        if (h_mant & 0x00800000u) {
            h_mant = 0;
            h_exp++;
            if (h_exp >= 0x1F) {
                return (fp16_t)(sign | 0x7C00u); // Overflow after rounding
            }
        }
    }
    return (fp16_t)(sign | ((uint32_t)h_exp << 10) | (h_mant >> 13));
}

// Convert a 16-bit half to a 32-bit float.
float fp16_to_float(fp16_t h) {
    uint32_t sign = ((uint32_t)h & 0x8000u) << 16;
    uint32_t exp  = ((uint32_t)h >> 10) & 0x1Fu;
    uint32_t mant = (uint32_t)h & 0x03FFu;

    union { uint32_t u; float f; } bits;

    if (exp == 0x1F) {
        // Inf / NaN
        bits.u = sign | 0x7F800000u | (mant << 13);
        return bits.f;
    }

    if (exp == 0) {
        if (mant == 0) {
            // +/-0
            bits.u = sign;
            return bits.f;
        }
        // Subnormal -> normalise
        while (!(mant & 0x0400u)) {
            mant <<= 1;
            exp--;
        }
        exp++;
        mant &= ~0x0400u;
    }

    bits.u = sign | (((exp - 15 + 127) & 0xFFu) << 23) | (mant << 13);
    return bits.f;
}

void unpack_nvdla_feature_map(uint8_t *src,
                               uint8_t *dst,
                               int C, int H, int W,
                               int Wp, int Wu,
                               int ATOM_C)
{
    int groups = (C + ATOM_C - 1) / ATOM_C;

    for (int c = 0; c < C; c++) {
        /**
         * TODO: lane points to element in 32-byte feature data cube.
         * I think lane should shift by element size (that is: lane = (c*ELEMENT_SIZE) % ATOM_C)
         * Correspondingly, g should shift by element size (that is: g = (c*ELEMENT_SIZE) / ATOM_C)
         */
        // int g = c / ATOM_C;
        // int lane = c % ATOM_C;
        int g = (c*ELEMENT_SIZE) / ATOM_C;
        int lane = (c*ELEMENT_SIZE) % ATOM_C;

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int src_idx = (((g * H + y) * Wp + x) * ATOM_C) + lane;
                int dst_idx = (c * H + y) * Wu + x;

                dst[dst_idx*2] = src[src_idx];
                dst[dst_idx*2+1] = src[src_idx+1];
                // NvDlaDebugPrintf("0x%02x 0x%02x ", src[src_idx], src[src_idx+1]);
                // NvDlaDebugPrintf("%.0f ", (float) src[src_idx]);
            }
            // NvDlaDebugPrintf("\n");
        }
    }
}

void pack_nvdla_feature_map(uint8_t *src,
                            uint8_t *dst,
                            int C, int H, int W,
                            int Wu, int Wp,
                            int ATOM_C)
{
    for (int c = 0; c < C; c++) {
        // int g = c / ATOM_C;
        // int lane = c % ATOM_C;
        int g = (c*ELEMENT_SIZE) / ATOM_C;
        int lane = (c*ELEMENT_SIZE) % ATOM_C;

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int src_idx = (c * H + y) * Wu + x;
                int dst_idx = (((g * H + y) * Wp + x) * ATOM_C) + lane;

                dst[dst_idx] = src[src_idx*2];
                dst[dst_idx+1] = src[src_idx*2+1];
            }
        }
    }
}

void unpack_nvdla_weights(uint8_t *src,
                         uint8_t *dst,
                         int Co, int Ci, int Kh, int Kw,
                         int ATOM_C, int ATOM_K)
{
    int Co_groups = (Co + ATOM_K - 1) / ATOM_K;
    int Ci_groups = (Ci + ATOM_C - 1) / ATOM_C;

    for (int co = 0; co < Co; co++) {
        int gok = co / ATOM_K;
        int lok = co % ATOM_K;

        for (int ci = 0; ci < Ci; ci++) {
            int gic = ci / ATOM_C;
            int lic = ci % ATOM_C;

            for (int kh = 0; kh < Kh; kh++) {
                for (int kw = 0; kw < Kw; kw++) {

                    int src_idx =
                        (((((gok * Ci_groups + gic) * Kh + kh) * Kw + kw)
                           * ATOM_C + lic) * ATOM_K + lok);

                    int dst_idx =
                        (((co * Ci + ci) * Kh + kh) * Kw + kw);

                    NvDlaDebugPrintf("%d ", src_idx);

                    // dst[dst_idx] = src[src_idx];
                    dst[dst_idx*2] = src[src_idx];
                    dst[dst_idx*2+1] = src[src_idx+1];
                }
                NvDlaDebugPrintf("\n");
            }
        }
    }
}

void unpackWeightImpl(
    // float* destData,                      // [N][C][H][W]
    // const Type* srcData,                  // packed weights
    // NvDlaDims packedDims,
    uint8_t *srcData, uint8_t *destData,
    int N, int C, int H, int W,
    int64_t numFrontPaddingChannels,
    int64_t outputChannelOffset)
{
    // const int N = packedDims.n;
    // const int C = packedDims.c;
    // const int H = packedDims.h;
    // const int W = packedDims.w;
    int logicalDims_c = C;
    int logicalDims_h = H;
    int logicalDims_w = W;

    const int channel_per_cube = WEIGHT_ATOM_CUBE_SIZE / ELEMENT_SIZE;
    const int w_stride_kgrp = MAC_ATOMIC_K * C * H * W;

    for (int n = 0; n < (N + MAC_ATOMIC_K - 1) / MAC_ATOMIC_K; n++)
    {
        int n_size =
            (N - n * MAC_ATOMIC_K >= MAC_ATOMIC_K) ?
            MAC_ATOMIC_K :
            (N - n * MAC_ATOMIC_K);

        int w_stride_surf = W * H * n_size * channel_per_cube;

        for (int h = 0; h < H; h++)
        {
            for (int w = 0; w < W; w++)
            {
                for (int n_ofs = 0; n_ofs < n_size; n_ofs++)
                {
                    for (int c = 0; c < C; c++)
                    {
                        int surf_ofs = c / channel_per_cube;
                        int ch_ofs   = c % channel_per_cube;

                        int cube_size =
                            ((C - surf_ofs * channel_per_cube) >= channel_per_cube) ?
                            channel_per_cube :
                            (C - surf_ofs * channel_per_cube);

                        int w_stride_line =
                            W * n_size * cube_size;

                        int src_ofs = (n * w_stride_kgrp) + (surf_ofs * w_stride_surf) +
                            (h * w_stride_line) + 
                            (w * n_size * cube_size) +
                            (n_ofs * cube_size) +
                            ch_ofs;

                        int dstChannel = c - numFrontPaddingChannels;

                        if (c < numFrontPaddingChannels)
                            continue;

                        int dst_ofs =
                            ((n * MAC_ATOMIC_K + n_ofs + outputChannelOffset)* logicalDims_c * logicalDims_h * logicalDims_w) +
                            (dstChannel * logicalDims_h * logicalDims_w) +
                            (h * logicalDims_w) + w;

                        // destData[dst_ofs] = float16ToFloat(srcData[src_ofs]);
                        destData[dst_ofs*2] = srcData[src_ofs*2];
                        destData[dst_ofs*2+1] = srcData[src_ofs*2+1];
                    }
                }
            }
        }
    }
}

NvDlaError Emulator::executeConvolutionNaive
(
    EMUConvOpDescAccessor opDesc,
    EMUCommonOpDescAccessor commonOpDesc,
    EMUConvBufferDescsAccessor bufDescs,
    std::vector<NvU8*> addressList
)
{
    NvDlaError e = NvDlaSuccess;

    EMUBufferDescAccessor weights = bufDescs.weightDataAccessor();
    EMUBufferDescAccessor biases = bufDescs.biasDataAccessor();
    EMUBufferDescAccessor src = bufDescs.srcDataAccessor();
    EMUBufferDescAccessor dst = bufDescs.dstDataAccessor();

    if ( debugOps() )
    {
        NvDlaDebugPrintf("Processing convolution (naive) %s relu\n", *opDesc.has_relu() ? "with" : "without");
        NvDlaDebugPrintf("src format %u\n", *src.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *src.addressIndex(), *src.addressIndexOffset(),
                addressList[*src.addressIndex()], *src.width(), *src.height(), *src.channel(), *src.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *src.lineStride(), *src.surfStride());
        NvDlaDebugPrintf("\tinput scale factor: %f, output scale factor: %f\n", *commonOpDesc.input_scale_factor(), *commonOpDesc.output_scale_factor());

        NvDlaDebugPrintf("dst format %u\n", *dst.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *dst.addressIndex(),  *dst.addressIndexOffset(),
                addressList[*dst.addressIndex()], *dst.width(), *dst.height(), *dst.channel(), *dst.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *dst.lineStride(), *dst.surfStride());
        
        NvDlaDebugPrintf("weights format %u\n", *weights.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *weights.addressIndex(),  *weights.addressIndexOffset(),
                addressList[*weights.addressIndex()], *weights.width(), *weights.height(), *weights.channel(), *weights.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *weights.lineStride(), *weights.surfStride());
    }


    fp16_t *in = reinterpret_cast<half*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
    fp16_t *w_hwim = reinterpret_cast<half*>( addressList[*weights.addressIndex()] + *weights.addressIndexOffset());
    fp16_t *bias = reinterpret_cast<half*>( addressList[*biases.addressIndex()] + *biases.addressIndexOffset());
    fp16_t *out = reinterpret_cast<half*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());
    
    // for (int sss = 0; sss < *src.size()/2; sss++) {
    //     NvDlaDebugPrintf("0x%08x (%f) ", in[sss], float(in[sss]));
    //     if (!((sss+1) % *src.width())) {
    //         NvDlaDebugPrintf("\n");
    //     }
    // }
    // NvDlaDebugPrintf("\n");

    // for (int sss = 0; sss < *dst.size()/2; sss++) {
    //     out[sss] = 0.02;
    // }
    // return e;

    // general
    int S = 1;
    // int pad = 0;

    // Query the opDesc for padding parameters
    // The exact field names depend on EMUConvOpDescAccessor interface
    // Common naming patterns:
    int pad_top    = *opDesc.pad_y_top();      // or similar field
    int pad_bottom = *opDesc.pad_y_bottom();
    int pad_left   = *opDesc.pad_x_left();
    int pad_right  = *opDesc.pad_x_right();
    int Sx         = *opDesc.conv_stride_x();
    int Sy         = *opDesc.conv_stride_y();

    // int pad_top = 0, pad_left = 0;
    // /* if (*opDesc.padType() == 1) */ {  // SAME padding
    //     int total_pad_h = ((Ho - 1) * S + Fh - Hi);
    //     int total_pad_w = ((Wo - 1) * S + Fw - Wi);
    //     if (total_pad_h < 0) total_pad_h = 0;
    //     if (total_pad_w < 0) total_pad_w = 0;
    //     pad_top  = total_pad_h / 2;
    //     pad_left = total_pad_w / 2;
    // }

    // input
    int Ci = *src.channel(), Hi = *src.height(), Wi = *src.width();

    ////////////////////////////////////////////////
    //////////////     TESTING      ////////////////
    ////////////////////////////////////////////////
    uint8_t *in_bytes = reinterpret_cast<uint8_t*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
    uint8_t * in_unpacked = new uint8_t[Ci*Hi*Wi*2];
    unpack_nvdla_feature_map(in_bytes, in_unpacked, Ci, Hi, Wi, Wi, Wi, 32);
    half *in_unpacked_half = reinterpret_cast<half*>(in_unpacked);
    // return e;
    uint32_t in_ind;
    
    // for (int c0 = 0; c0 < Ci; c0++) {
    //     for (int y0 = 0; y0 < Hi; y0++) {
    //         for (int x0 = 0; x0 < Wi; x0++) {
    //             in_ind = (c0 * Hi + y0) * Wi + x0;
    //             // NvDlaDebugPrintf("0x%08x (%f) (%f) ", in_unpacked[in_ind*2], (half)in_unpacked[in_ind*2]);
    //             // NvDlaDebugPrintf("%3.0f ", (float)in_unpacked_half[in_ind]*255.0);
    //             NvDlaDebugPrintf("%f ", (float)in_unpacked_half[in_ind]);
    //             // NvDlaDebugPrintf("0x%08x (%f) ", in[in_ind], (float)in[in_ind]);
    //         }
    //         NvDlaDebugPrintf("\n");
    //     }
    // }

    // return e;
    ///////////////////////////////////////////////

    in = in_unpacked_half;

    // filters
    int Fh = *weights.height();
    int Fw = *weights.width();

    // output
    int Co = *dst.channel(), Ho = *dst.height(), Wo = *dst.width();

    uint8_t *weights_bytes = reinterpret_cast<uint8_t*>( addressList[*weights.addressIndex()] + *weights.addressIndexOffset());
    uint8_t *weights_unpacked = new uint8_t[Co*Ci*Fh*Fw*2];
    // unpack_nvdla_weights(weights_bytes, weights_unpacked, Co, Ci, Fh, Fw, 8, 8);

    unpackWeightImpl(weights_bytes, weights_unpacked, Co, Ci, Fh, Fw, 0, 0);
    half *weights_unpacked_half = reinterpret_cast<half*>(weights_unpacked);
    w_hwim = weights_unpacked_half;

    // for (int sss = 0; sss < *weights.size()/2; sss++) {
    //     NvDlaDebugPrintf("%f ", w_hwim[sss], float(w_hwim[sss]));
    //     if (!((sss+1) % *weights.height())) {
    //         NvDlaDebugPrintf("\n");
    //     }
    // }
    // return e;

    // for (int c0 = 0; c0 < Co; c0++) {
    //     for (int y0 = 0; y0 < Ho; y0++) {
    //         for (int x0 = 0; x0 < Wo; x0++) {

    //             float sum = (float)bias[c0];

    //             for (int ci = 0; ci < Ci; ci++) {
    //                 for (int fh = 0; fh < Fh; fh++) {
    //                     for (int fw = 0; fw < Fw; fw++) {

    //                         int in_y = y0 * S + fh - pad_top;
    //                         int in_x = x0 * S + fw - pad_left;
                            
    //                         float input_val = 0.0f;
    //                         if (in_y >= 0 && in_y < Hi && in_x >= 0 && in_x < Wi) {
    //                             input_val = in[(ci * Hi + in_y) * Wi + in_x]*255.0;
    //                         }
                        
    //                         NvDlaDebugPrintf("% 4.0f ", (float)input_val);
    //                     }
    //                     NvDlaDebugPrintf("  ");
                        
    //                     for (int fw = 0; fw < Fw; fw++) {
    //                         uint32_t w_ind = ((c0 * Ci + ci) * Fh + fh) * Fw + fw;
                            
    //                         NvDlaDebugPrintf("% 2.4f ", (float)w_hwim[((c0 * Ci + ci) * Fh + fh) * Fw + fw]);
    //                     }
    //                     NvDlaDebugPrintf("\n");
    //                 }
    //                 NvDlaDebugPrintf("\n");
    //             }

    //             // out index: [Co][H_out][W_out]
    //             // out[(c0 * Ho + y0) * Wo + x0] = sum;
    //         }
    //     }
    // }

    // return e;

    half *out_unpacked_half = new half[Co * Ho * Wo];

    for (int c0 = 0; c0 < Co; c0++) {
        for (int y0 = 0; y0 < Ho; y0++) {
            for (int x0 = 0; x0 < Wo; x0++) {

                float sum = (float)bias[c0];

                for (int ci = 0; ci < Ci; ci++) {
                    for (int fh = 0; fh < Fh; fh++) {
                        for (int fw = 0; fw < Fw; fw++) {

                            int in_y = y0 * Sy + fh - pad_top;
                            int in_x = x0 * Sx + fw - pad_left;
                            
                            float input_val = 0.0f;
                            if (in_y >= 0 && in_y < Hi && in_x >= 0 && in_x < Wi) {
                                input_val = in[(ci * Hi + in_y) * Wi + in_x];
                            }

                            uint32_t w_ind = ((c0 * Ci + ci) * Fh + fh) * Fw + fw;
                            float weight_val = (float)w_hwim[((c0 * Ci + ci) * Fh + fh) * Fw + fw];
                            // uint32_t w_ind = ((ci * Fh + fh) * Fw + fw) * Co
                            // NvDlaDebugPrintf("in[%d][%d]*w[%d][%d][%d] (%0.2f*%0.2f) ",
                            //     in_y, in_x, c0,fh,fw, float(in[(ci * Hi + in_y) * Wi + in_x]),
                            //     float(w_hwim[((c0 * Ci + ci) * Fh + fh) * Fw + fw]));
                            // NvDlaDebugPrintf("0x%08x (%f) ", w_hwim[w_ind], float(w_hwim[w_ind]));

                            // sum += (float)in[(ci * Hi + in_y) * Wi + in_x] *
                            //     (float)w_hwim[((c0 * Ci + ci) * Fh + fh) * Fw + fw];
                            sum += input_val*weight_val;
                        }
                    }
                    // NvDlaDebugPrintf("%f\n", sum);
                }
                // NvDlaDebugPrintf("\n");

                // out index: [Co][H_out][W_out]
                // out[(c0 * Ho + y0) * Wo + x0] = sum;

                out_unpacked_half[(c0 * Ho + y0) * Wo + x0] = sum;
            }
        }
    }

    // ReLU
    if (*opDesc.has_relu() == 1) {
        for (int c0 = 0; c0 < Co; c0++) {
            for (int y0 = 0; y0 < Ho; y0++) {
                for (int x0 = 0; x0 < Wo; x0++) {
                    uint32_t ind = (c0 * Ho + y0) * Wo + x0;
                    if (out_unpacked_half[ind] < 0.0f) out_unpacked_half[ind] = 0.0f;
                }
            }
        }
    }

    // print Conv + ReLU result
    // NvDlaDebugPrintf("=== Conv RESULT ===\n");
    // for (int c0 = 0; c0 < Co; c0++) {
    //     for (int y0 = 0; y0 < Ho; y0++) {
    //         for (int x0 = 0; x0 < Wo; x0++) {
    //             NvDlaDebugPrintf("%f ", float(out_unpacked_half[(c0 * Ho + y0) * Wo + x0]));
    //         }
    //         NvDlaDebugPrintf("\n");
    //     }
    // }
    // NvDlaDebugPrintf("=== EOF Conv RESULT ===\n");

    for (int sss = 0; sss < *dst.size()/2; sss++) {
        out[sss] = 0.0;
    }

    uint8_t *dst_bytes = reinterpret_cast<uint8_t*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());
    uint8_t *out_unpacked_bytes = reinterpret_cast<uint8_t*>(out_unpacked_half);
    pack_nvdla_feature_map(out_unpacked_bytes, dst_bytes, Co, Ho, Wo, Wo, Wo, 32);

    delete[] in_unpacked;
    delete[] weights_unpacked;
    delete[] out_unpacked_half;

    // for (int sss = 0; sss < Co*Ho*Wo; sss++) {
    //     NvDlaDebugPrintf("0x%08x (%f) ", out[sss], float(out[sss]));
    //     // if (!(sss % 20)) {
    //         NvDlaDebugPrintf("\n");
    //     // }
    // }
    // for (int sss = 0; sss < *dst.size()/2; sss++) {
    //     out[sss] = 0.02;
    // }


fail:
    return e;
}


NvDlaError Emulator::executeConvolution
(
    EMUConvOpDescAccessor opDesc,
    EMUCommonOpDescAccessor commonOpDesc,
    EMUConvBufferDescsAccessor bufDescs,
    std::vector<NvU8*> addressList
)
{
    NvDlaError e = NvDlaSuccess;

    EMUBufferDescAccessor weights = bufDescs.weightDataAccessor();
    EMUBufferDescAccessor biases = bufDescs.biasDataAccessor();
    EMUBufferDescAccessor src = bufDescs.srcDataAccessor();
    EMUBufferDescAccessor dst = bufDescs.dstDataAccessor();

    if ( debugOps() )
    {
        // NvDlaDebugPrintf("Processing convolution [axis=%u]\n", *opDesc.axis());
        NvDlaDebugPrintf("src format %u\n", *src.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *src.addressIndex(), *src.addressIndexOffset(),
                addressList[*src.addressIndex()], *src.width(), *src.height(), *src.channel(), *src.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *src.lineStride(), *src.surfStride());
        NvDlaDebugPrintf("\tinput scale factor: %f, output scale factor: %f\n", *commonOpDesc.input_scale_factor(), *commonOpDesc.output_scale_factor());

        NvDlaDebugPrintf("dst format %u\n", *dst.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *dst.addressIndex(),  *dst.addressIndexOffset(),
                addressList[*dst.addressIndex()], *dst.width(), *dst.height(), *dst.channel(), *dst.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *dst.lineStride(), *dst.surfStride());
        
        NvDlaDebugPrintf("weights format %u\n", *weights.format());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *weights.addressIndex(),  *weights.addressIndexOffset(),
                addressList[*weights.addressIndex()], *weights.width(), *weights.height(), *weights.channel(), *weights.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *weights.lineStride(), *weights.surfStride());
    }


    fp16_t *in = reinterpret_cast<half*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
    // ShapeNHWC si,
    fp16_t *w_hwim = reinterpret_cast<half*>( addressList[*weights.addressIndex()] + *weights.addressIndexOffset());
    fp16_t *bias = reinterpret_cast<half*>( addressList[*biases.addressIndex()] + *biases.addressIndexOffset());
    // ShapeNHWC so,
    int kh = *weights.height();
    int kw = *weights.width(); 
    int sh = 1; int sw = 1; 
    int pad = 0; int dm = 1;
    fp16_t *out = reinterpret_cast<half*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());

    // for (int sss = 0; sss < *src.size(); sss++) {
    //     NvDlaDebugPrintf("0x%08x (%f) ", in[sss], float(in[sss]));
    //     if (!(sss % 20)) {
    //         NvDlaDebugPrintf("\n");
    //     }
    // }
    // 
    // return e;

    for (int sss = 0; sss < *weights.size(); sss++) {
        NvDlaDebugPrintf("0x%08x (%f) ", w_hwim[sss], float(w_hwim[sss]));
        if (!(sss % 20)) {
            NvDlaDebugPrintf("\n");
        }
    }
    return e;

    // NvU16 dc, dh, dw;
    // dc = *dst.channel();
    // dh = *dst.height();
    // dw = *dst.width();
    // for (int c = 0; c < dc; c++) {
    //     for (int h = 0; h < dh; h++) {
    //         for (int w = 0; w < dw; w++) {
    //             out[c*(dh*dw) + h*dw + w] = 25.0;
    //         }
    //     }
    // }


    // for (int sss = 0; sss < *dst.size(); sss++)
    //     out[sss] = 25.0;

    // NvU16 Sc, Sh, Sw;
    // Sc = *src.channel();
    // Sh = *src.height();
    // Sw = *src.width();

    // NvU8* pSrc = reinterpret_cast<NvU8*>(addressList[*src.addressIndex()] + *src.addressIndexOffset());
    // for (NvU16 c = 0; c < Sc; ++c) {
    //     for (NvU16 h = 0; h < Sh; ++h) {
    //         for (NvU16 w = 0; w < Sw; ++w) {
    //             NvU32 offset = 0;
    //             if (getAddrOffset(src, w, h, c, &offset) == NvDlaSuccess) {
    //                 fp16_t value = *reinterpret_cast<fp16_t*>(pSrc + offset);
    //                 NvDlaDebugPrintf("in(%u,%u,%u):%f ", c, h, w, fp16_to_float(value));
    //             } else {
    //                 NvDlaDebugPrintf("in(%u,%u,%u):<invalid> ", c, h, w);
    //             }
    //         }
    //         NvDlaDebugPrintf("\n");
    //     }
    // }

    // NvU16 Wc, Wh, Ww;
    // Wc = *weights.channel();
    // Wh = *weights.height();
    // Ww = *weights.width();

    // NvU8* pWeights = reinterpret_cast<NvU8*>(addressList[*weights.addressIndex()] + *weights.addressIndexOffset());
    // for (NvU16 c = 0; c < Wc; ++c) {
    //     for (NvU16 h = 0; h < Wh; ++h) {
    //         for (NvU16 w = 0; w < Ww; ++w) {
    //             NvU32 offset = 0;
    //             if (getAddrOffset(weights, w, h, c, &offset) == NvDlaSuccess) {
    //                 fp16_t value = *reinterpret_cast<fp16_t*>(pWeights + offset);
    //                 NvDlaDebugPrintf("w(%u,%u,%u):%f ", c, h, w, fp16_to_float(value));
    //             } else {
    //                 NvDlaDebugPrintf("w(%u,%u,%u):<invalid> ", c, h, w);
    //             }
    //         }
    //         NvDlaDebugPrintf("\n");
    //     }
    // }

    // return e;

    // int H = si.h, W = si.w, Ci = si.c;
    // int Ho = so.h, Wo = so.w, Co = so.c;
    int H = *src.height(), W = *src.width(), Ci = *src.channel();
    int Ho = *dst.height(), Wo = *dst.width(), Co = *dst.channel();
    int pad_top = 0, pad_left = 0;
    if (pad != 0) {
        int total_pad_h = ((Ho - 1) * sh + kh - H);
        int total_pad_w = ((Wo - 1) * sw + kw - W);
        if (total_pad_h < 0) total_pad_h = 0;
        if (total_pad_w < 0) total_pad_w = 0;
        pad_top  = total_pad_h / 2;
        pad_left = total_pad_w / 2;
    }

    for (int oh = 0; oh < Ho; oh++) {
        int ih0 = oh * sh - pad_top;
        for (int ow = 0; ow < Wo; ow++) {
            int iw0 = ow * sw - pad_left;
            for (int c = 0; c < Ci; c++) {
                for (int m = 0; m < dm; m++) {
                    int co = c * dm + m;
                    float acc = bias ? fp16_to_float(bias[co]) : 0.0f;
                    for (int r = 0; r < kh; r++) {
                        int ih = ih0 + r;
                        if (ih < 0 || ih >= H) continue;
                        for (int s = 0; s < kw; s++) {
                            int iw = iw0 + s;
                            if (iw < 0 || iw >= W) continue;
                            float x = fp16_to_float(in[((ih * W + iw) * Ci) + c]);
                            float k = fp16_to_float(w_hwim[((r * kw + s) * Ci + c) * dm + m]);
                            acc += x * k;
                            // NvDlaDebugPrintf("(%f,%f) %f ", x, k, x*k);
                        }
                    }
                    // NvDlaDebugPrintf("\n");
                    out[((oh * Wo + ow) * Co) + co] = fp16_from_float(acc);
                }
            }
        }
    }

    /*
    if ( *src.format() != *dst.format() )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU Scale operation with different "
            " src (%d) and dst (%d) formats\n", static_cast<NvU32>(*src.format()),
            static_cast<NvU32>(*dst.format()));
    }

    // Execute
    if (*src.format() == EMU_FORMAT_FF16)
    {
        half* pSrc = reinterpret_cast<half*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
        half* pDst = reinterpret_cast<half*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());

        NvF32 maxval = -INFINITY;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            if (float(pSrc[ii]) > maxval)
            {
                maxval = float(pSrc[ii]);
            }
        }
        NvF32 sumexp = 0.0f;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            sumexp += expf(float(pSrc[ii])-maxval);
        }
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            pDst[ii] = expf(float(pSrc[ii])-maxval) / sumexp;
        }
    }
    else if ((*src.format() == EMU_FORMAT_INT8) || (*src.format() == EMU_FORMAT_INT8_8))
    {
        NvS8* pSrc = reinterpret_cast<NvS8*>( addressList[*src.addressIndex()] + *src.addressIndexOffset() );
        NvS8* pDst = reinterpret_cast<NvS8*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset() );

        half* pHalfSrc = reinterpret_cast<half*>(malloc(*src.channel() * sizeof(half)));
        half* pHalfDst = reinterpret_cast<half*>(malloc(*dst.channel() * sizeof(half)));

        // scale input for processing in FLOAT land
        for (NvU32 ii = 0; ii < *src.channel(); ii++)
        {
            pHalfSrc[ii] = pSrc[ii] * (*commonOpDesc.input_scale_factor());
        }

        NvF32 maxval = -INFINITY;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            if (float(pHalfSrc[ii]) > maxval)
            {
                maxval = float(pHalfSrc[ii]);
            }
        }

        NvF32 sumexp = 0.0f;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            sumexp += expf(float(pHalfSrc[ii])-maxval);
        }
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            pHalfDst[ii] = static_cast<half>(expf(float(pHalfSrc[ii])-maxval) / sumexp);
        }

        // rescale output to write out in INT8 land
        for (NvU32 ii = 0; ii < *dst.channel(); ii++)
        {
            pDst[ii] = saturate<NvF32, NvS8>(pHalfDst[ii] / (*commonOpDesc.output_scale_factor()));
        }

        if (debugPrint())
        {
            NvF32 maxHalfDst = -INFINITY;
            NvU32 maxHalfIndex = -1;
            NvF32 maxIntDst = std::numeric_limits<NvS8>::lowest();
            NvU32 maxIntIndex = -1;

            for (NvU32 ii = 0; ii < *dst.channel(); ii++) {
                if (pHalfDst[ii] > maxHalfDst) {
                    maxHalfDst = pHalfDst[ii];
                    maxHalfIndex = ii;
                }
                if (pDst[ii] > maxIntDst) {
                    maxIntDst = pDst[ii];
                    maxIntIndex = ii;
                }
            }

            NvDlaDebugPrintf("Post-softmax max value: (half) %f, (int) %f\n", maxHalfDst, maxIntDst);
            NvDlaDebugPrintf("at indices (half) %d, (int) %d\n", maxHalfIndex, maxIntIndex);
        }


    }
    else
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU softmax operation for format: %d\n",
            static_cast<NvU32>(*src.format()));
    }
    */

fail:
    return e;
}


NvDlaError Emulator::executePool
(
    EMUPoolOpDescAccessor opDesc,
    EMUCommonOpDescAccessor commonOpDesc,
    EMUPoolBufferDescsAccessor bufDescs,
    std::vector<NvU8*> addressList
)
{
    NvDlaError e = NvDlaSuccess;

    EMUBufferDescAccessor src = bufDescs.srcDataAccessor();
    EMUBufferDescAccessor dst = bufDescs.dstDataAccessor();

    fp16_t *in = reinterpret_cast<half*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
    fp16_t *out = reinterpret_cast<half*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());

    if ( debugOps() )
    {
        NvDlaDebugPrintf("Processing %s pool\n", *opDesc.pool_mode() == POOL_MODE_AVG ? "avg" : *opDesc.pool_mode() == POOL_MODE_MAX ? "max" : "min");
        NvDlaDebugPrintf("precision %u\n", *opDesc.precision());
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *src.addressIndex(), *src.addressIndexOffset(),
                addressList[*src.addressIndex()], *src.width(), *src.height(), *src.channel(), *src.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *src.lineStride(), *src.surfStride());
        NvDlaDebugPrintf("\tinput scale factor: %f, output scale factor: %f\n", *commonOpDesc.input_scale_factor(), *commonOpDesc.output_scale_factor());

        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *dst.addressIndex(),  *dst.addressIndexOffset(),
                addressList[*dst.addressIndex()], *dst.width(), *dst.height(), *dst.channel(), *dst.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *dst.lineStride(), *dst.surfStride());
    }
    
    // general
    // int S = 1;
    // int pad = 0;

    // Query the opDesc for padding parameters
    // The exact field names depend on EMUConvOpDescAccessor interface
    // Common naming patterns:
    int pad_top    = *opDesc.pad_top();      // or similar field
    int pad_bottom = *opDesc.pad_bottom();
    int pad_left   = *opDesc.pad_left();
    int pad_right  = *opDesc.pad_right();

    int sw         = *opDesc.stride_x();
    int sh         = *opDesc.stride_y();
    int kh         = *opDesc.pool_height()+1;
    int kw         = *opDesc.pool_width()+1;

    // input
    int Ci = *src.channel(), Hi = *src.height(), Wi = *src.width();
    // output
    int Co = *dst.channel(), Ho = *dst.height(), Wo = *dst.width();

    ////////////////////////////////////////////////
    //////////////     TESTING      ////////////////
    ////////////////////////////////////////////////
    uint8_t *in_bytes = reinterpret_cast<uint8_t*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
    uint8_t * in_unpacked = new uint8_t[Ci*Hi*Wi*ELEMENT_SIZE];
    unpack_nvdla_feature_map(in_bytes, in_unpacked, Ci, Hi, Wi, Wi, Wi, 32);
    half *in_unpacked_half = reinterpret_cast<half*>(in_unpacked);
    // return e;
    uint32_t in_ind;
    
    // for (int c0 = 0; c0 < Ci; c0++) {
    //     for (int y0 = 0; y0 < Hi; y0++) {
    //         for (int x0 = 0; x0 < Wi; x0++) {
    //             in_ind = (c0 * Hi + y0) * Wi + x0;
    //             // NvDlaDebugPrintf("0x%08x (%f) (%f) ", in_unpacked[in_ind*2], (half)in_unpacked[in_ind*2]);
    //             // NvDlaDebugPrintf("%3.0f ", (float)in_unpacked_half[in_ind]*255.0);
    //             NvDlaDebugPrintf("%f ", (float)in_unpacked_half[in_ind]);
    //             // NvDlaDebugPrintf("0x%08x (%f) ", in[in_ind], (float)in[in_ind]);
    //         }
    //         NvDlaDebugPrintf("\n");
    //     }
    // }

    // return e;
    ///////////////////////////////////////////////

    in = in_unpacked_half;

    if ( *src.format() != *dst.format() )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU Scale operation with different "
            " src (%d) and dst (%d) formats\n", static_cast<NvU32>(*src.format()),
            static_cast<NvU32>(*dst.format()));
    }

    // Execute
    if (*opDesc.precision() == EMU_FORMAT_FF16)
    {
        half *out_unpacked_half = new half[Co * Ho * Wo];

        if (*opDesc.pool_mode() == POOL_MODE_AVG) {
            // int Ho = so.h;
            // int Wo = so.w;
            if (Ho <= 0) Ho = 1;
            if (Wo <= 0) Wo = 1;

            /**
             * TODO: Check this padding calculation method
             */
            // int pad_top = 0, pad_left = 0;
            // if (pad != 0) {
            //     int total_pad_h = ((Ho - 1) * sh + kh - Hi);
            //     int total_pad_w = ((Wo - 1) * sw + kw - Wi);
            //     if (total_pad_h < 0) total_pad_h = 0;
            //     if (total_pad_w < 0) total_pad_w = 0;
            //     pad_top = total_pad_h / 2;
            //     pad_left = total_pad_w / 2;
            // }

            for (int c = 0; c < Ci; c++) {
                for (int oh = 0; oh < Ho; oh++) {
                    for (int ow = 0; ow < Wo; ow++) {
                        float sum = 0.f; int cnt = 0;
                        for (int r = 0; r < kh; r++) {
                            int ih = oh * sh - pad_top + r;
                            if (ih < 0 || ih >= Hi) continue;
                            for (int s = 0; s < kw; s++) {
                                int iw = ow * sw - pad_left + s;
                                if (iw < 0 || iw >= Wi) continue;
                                // sum += float(in[((ih * Wi + iw) * Ci) + c]);
                                sum += float(in[((c * Hi + ih) * Wi) + iw]);
                                cnt++;
                            }
                        }
                        // out[((oh * Wo + ow) * Ci) + c] = (cnt > 0) ? (sum / (float)cnt) : 0.f;
                        out_unpacked_half[(c * Ho + oh) * Wo + ow] = (cnt > 0) ? (sum / (float)cnt) : 0.f;
                    }
                }
            }
        } else if (*opDesc.pool_mode() == POOL_MODE_MAX) {
            if (kh <= 0) kh = 1;
            if (kw <= 0) kw = 1;
            if (sh <= 0) sh = kh;
            if (sw <= 0) sw = kw;

            // int H = si.h, W = si.w, C = si.c;
            // int Ho = so.h;
            // int Wo = so.w;
            if (Ho <= 0) Ho = 1;
            if (Wo <= 0) Wo = 1;

            // int pad_top = 0, pad_left = 0;
            // if (pad != 0) {
            //     int total_pad_h = ((Ho - 1) * sh + kh - H);
            //     int total_pad_w = ((Wo - 1) * sw + kw - W);
            //     if (total_pad_h < 0) total_pad_h = 0;
            //     if (total_pad_w < 0) total_pad_w = 0;
            //     pad_top = total_pad_h / 2;
            //     pad_left = total_pad_w / 2;
            // }

            for (int c = 0; c < Ci; c++) {
                for (int oh = 0; oh < Ho; oh++) {
                    for (int ow = 0; ow < Wo; ow++) {
                        float m = -FLT_MAX;
                        bool seen = false;
                        for (int r = 0; r < kh; r++) {
                            int ih = oh * sh - pad_top + r;
                            if (ih < 0 || ih >= Hi) continue;
                            for (int s = 0; s < kw; s++) {
                                int iw = ow * sw - pad_left + s;
                                if (iw < 0 || iw >= Wi) continue;
                                // float v = in[((ih * Wi + iw) * Ci) + c];
                                float v = float(in[((c * Hi + ih) * Wi) + iw]);
                                if (!seen || v > m) m = v;
                                seen = true;
                            }
                        }
                        // out[((oh * Wo + ow) * Ci) + c] = seen ? m : 0.f;
                        out_unpacked_half[(c * Ho + oh) * Wo + ow] = seen ? m : 0.f;
                    }
                }
            }
        }

        // print Pool result
        // NvDlaDebugPrintf("=== Pool RESULT ===\n");
        // for (int c0 = 0; c0 < Co; c0++) {
        //     for (int y0 = 0; y0 < Ho; y0++) {
        //         for (int x0 = 0; x0 < Wo; x0++) {
        //             NvDlaDebugPrintf("%f ", float(out_unpacked_half[(c0 * Ho + y0) * Wo + x0]));
        //         }
        //         NvDlaDebugPrintf("\n");
        //     }
        // }
        // NvDlaDebugPrintf("=== EOF Pool RESULT ===\n");

        uint8_t *dst_bytes = reinterpret_cast<uint8_t*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());
        uint8_t *out_unpacked_bytes = reinterpret_cast<uint8_t*>(out_unpacked_half);
        pack_nvdla_feature_map(out_unpacked_bytes, dst_bytes, Co, Ho, Wo, Wo, Wo, 32);

        delete[] in_unpacked;
        delete[] out_unpacked_half;
    }
    else if ((*opDesc.precision() == EMU_FORMAT_INT8) || (*opDesc.precision() == EMU_FORMAT_INT8_8))
    {
        NvS8* pSrc = reinterpret_cast<NvS8*>( addressList[*src.addressIndex()] + *src.addressIndexOffset() );
        NvS8* pDst = reinterpret_cast<NvS8*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset() );

        half* pHalfSrc = reinterpret_cast<half*>(malloc(*src.channel() * sizeof(half)));
        half* pHalfDst = reinterpret_cast<half*>(malloc(*dst.channel() * sizeof(half)));

        // scale input for processing in FLOAT land
        for (NvU32 ii = 0; ii < *src.channel(); ii++)
        {
            pHalfSrc[ii] = pSrc[ii] * (*commonOpDesc.input_scale_factor());
        }

        NvF32 maxval = -INFINITY;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            if (float(pHalfSrc[ii]) > maxval)
            {
                maxval = float(pHalfSrc[ii]);
            }
        }

        NvF32 sumexp = 0.0f;
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            sumexp += expf(float(pHalfSrc[ii])-maxval);
        }
        for (NvU32 ii=0; ii<*src.channel(); ii++)
        {
            pHalfDst[ii] = static_cast<half>(expf(float(pHalfSrc[ii])-maxval) / sumexp);
        }

        // rescale output to write out in INT8 land
        for (NvU32 ii = 0; ii < *dst.channel(); ii++)
        {
            pDst[ii] = saturate<NvF32, NvS8>(pHalfDst[ii] / (*commonOpDesc.output_scale_factor()));
        }

        if (debugPrint())
        {
            NvF32 maxHalfDst = -INFINITY;
            NvU32 maxHalfIndex = -1;
            NvF32 maxIntDst = std::numeric_limits<NvS8>::lowest();
            NvU32 maxIntIndex = -1;

            for (NvU32 ii = 0; ii < *dst.channel(); ii++) {
                if (pHalfDst[ii] > maxHalfDst) {
                    maxHalfDst = pHalfDst[ii];
                    maxHalfIndex = ii;
                }
                if (pDst[ii] > maxIntDst) {
                    maxIntDst = pDst[ii];
                    maxIntIndex = ii;
                }
            }

            NvDlaDebugPrintf("Post-softmax max value: (half) %f, (int) %f\n", maxHalfDst, maxIntDst);
            NvDlaDebugPrintf("at indices (half) %d, (int) %d\n", maxHalfIndex, maxIntIndex);
        }


    }
    else
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU softmax operation for format: %d\n",
            static_cast<NvU32>(*src.format()));
    }

fail:
    return e;
}

NvDlaError Emulator::executeSdp
(
    EMUSdpOpDescAccessor opDesc,
    EMUCommonOpDescAccessor commonOpDesc,
    EMUSdpBufferDescsAccessor bufDescs,
    std::vector<NvU8*> addressList
)
{
    NvDlaError e = NvDlaSuccess;

    EMUBufferDescAccessor src = bufDescs.srcDataAccessor();
    EMUBufferDescAccessor x1_data = bufDescs.x1DataAccessor();
    EMUBufferDescAccessor x2_data = bufDescs.x2DataAccessor();
    EMUBufferDescAccessor y_data = bufDescs.yDataAccessor();
    EMUBufferDescAccessor dst = bufDescs.dstDataAccessor();

    fp16_t *in = reinterpret_cast<half*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
    fp16_t *x1 = reinterpret_cast<half*>( addressList[*x1_data.addressIndex()] + *x1_data.addressIndexOffset());
    fp16_t *out = reinterpret_cast<half*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());

    if ( debugOps() )
    {
        NvDlaDebugPrintf("Processing sdp (%s)\n",
            (*opDesc.x1_op()).type == SDP_OP_NONE ? "none" :
            (*opDesc.x1_op()).type == SDP_OP_MUL ? "mul" :
            (*opDesc.x1_op()).type == SDP_OP_ALU ? "alu (add)" : "mul+alu");
        // NvDlaDebugPrintf("precision %u\n", *opDesc.precision());
        NvDlaDebugPrintf("\t SRC\n");
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *src.addressIndex(), *src.addressIndexOffset(),
                addressList[*src.addressIndex()], *src.width(), *src.height(), *src.channel(), *src.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *src.lineStride(), *src.surfStride());
        NvDlaDebugPrintf("\tinput scale factor: %f, output scale factor: %f\n", *commonOpDesc.input_scale_factor(), *commonOpDesc.output_scale_factor());

        NvDlaDebugPrintf("\t X1\n");
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *x1_data.addressIndex(), *x1_data.addressIndexOffset(),
                addressList[*x1_data.addressIndex()], *x1_data.width(), *x1_data.height(), *x1_data.channel(), *x1_data.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *x1_data.lineStride(), *x1_data.surfStride());
        NvDlaDebugPrintf("\tinput scale factor: %f, output scale factor: %f\n", *commonOpDesc.input_scale_factor(), *commonOpDesc.output_scale_factor());

        NvDlaDebugPrintf("\t DST\n");
        NvDlaDebugPrintf("\taddress[%u][%u] 0x%llx (%ux%ux%u) %uB\n", *dst.addressIndex(),  *dst.addressIndexOffset(),
                addressList[*dst.addressIndex()], *dst.width(), *dst.height(), *dst.channel(), *dst.size());
        NvDlaDebugPrintf("\tline_stride %uB surface_stride %uB\n", *dst.lineStride(), *dst.surfStride());
        NvDlaDebugPrintf("\tprecision: %d\n", *opDesc.src_precision());
    }

    // input
    int Ci = *src.channel(), Hi = *src.height(), Wi = *src.width();
    // output
    int Co = *dst.channel(), Ho = *dst.height(), Wo = *dst.width();

    ////////////////////////////////////////////////
    //////////////     TESTING      ////////////////
    ////////////////////////////////////////////////
    uint8_t *in_bytes = reinterpret_cast<uint8_t*>( addressList[*src.addressIndex()] + *src.addressIndexOffset());
    uint8_t * in_unpacked = new uint8_t[Ci*Hi*Wi*ELEMENT_SIZE];
    unpack_nvdla_feature_map(in_bytes, in_unpacked, Ci, Hi, Wi, Wi, Wi, 32);
    half *in_unpacked_half = reinterpret_cast<half*>(in_unpacked);

    in = in_unpacked_half;

    if ( *src.format() != *dst.format() )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU sdp operation with different "
            " src (%d) and dst (%d) formats\n", static_cast<NvU32>(*src.format()),
            static_cast<NvU32>(*dst.format()));
    }

    // Execute
    if (*opDesc.src_precision() == EMU_FORMAT_FF16)
    {
        half *out_unpacked_half = new half[Co * Ho * Wo];

        if ((*opDesc.x1_op()).type == SDP_OP_MUL) {
            if ((*opDesc.x1_op()).mode == SDP_OP_PER_KERNEL) {
                for (int c = 0; c < Ci; c++) {
                    float x1_c = float(x1[c]);
                    for (int ih = 0; ih < Hi; ih++) {
                        for (int iw = 0; iw < Wi; iw++) {
                            float in_c_h_w = float(in[((c * Hi + ih) * Wi) + iw]);
                            // out[((oh * Wo + ow) * Ci) + c] = (cnt > 0) ? (sum / (float)cnt) : 0.f;
                            out_unpacked_half[(c * Hi + ih) * Wi + iw] = in_c_h_w * x1_c;
                        }
                    }
                }
            }
        } else if ((*opDesc.x1_op()).type == SDP_OP_ALU) {
            if ((*opDesc.x1_op()).mode == SDP_OP_PER_KERNEL) {
                // OP: OUT(c,h,w) = IN(c,h,w) + X1(c)

                for (int c = 0; c < Ci; c++) {
                    float x1_c = float(x1[c]);
                    for (int ih = 0; ih < Hi; ih++) {
                        for (int iw = 0; iw < Wi; iw++) {
                            float in_c_h_w = float(in[((c * Hi + ih) * Wi) + iw]);
                            // out[((oh * Wo + ow) * Ci) + c] = (cnt > 0) ? (sum / (float)cnt) : 0.f;
                            out_unpacked_half[(c * Hi + ih) * Wi + iw] = in_c_h_w + x1_c;
                        }
                    }
                }
            } else if ((*opDesc.x1_op()).mode == SDP_OP_PER_POINT) {
                // OP: OUT(c,h,w) = IN(c,h,w) + X1(c,h,w)
                NvDlaDebugPrintf("\t op: per-point add\n");

                // unpack x1
                uint8_t *x1_bytes = reinterpret_cast<uint8_t*>( addressList[*x1_data.addressIndex()] + *x1_data.addressIndexOffset());
                uint8_t * x1_unpacked = new uint8_t[Ci*Hi*Wi*ELEMENT_SIZE];
                unpack_nvdla_feature_map(x1_bytes, x1_unpacked, Ci, Hi, Wi, Wi, Wi, 32);
                half *x1_unpacked_half = reinterpret_cast<half*>(x1_unpacked);

                x1 = x1_unpacked_half;

                for (int c = 0; c < Ci; c++) {
                    for (int ih = 0; ih < Hi; ih++) {
                        for (int iw = 0; iw < Wi; iw++) {
                            float x1_c_h_w = float(x1[((c * Hi + ih) * Wi) + iw]);
                            float in_c_h_w = float(in[((c * Hi + ih) * Wi) + iw]);
                            // out[((oh * Wo + ow) * Ci) + c] = (cnt > 0) ? (sum / (float)cnt) : 0.f;
                            out_unpacked_half[((c * Hi + ih) * Wi) + iw] = in_c_h_w + x1_c_h_w;
                        }
                    }
                }
                delete[] x1_unpacked;
            }
        } else if ((*opDesc.x1_op()).type == SDP_OP_NONE & (*opDesc.x1_op()).act == ACTIVATION_RELU) {
            // relu
            NvDlaDebugPrintf("Executing relu...\n");
            for (int c0 = 0; c0 < Co; c0++) {
                for (int y0 = 0; y0 < Ho; y0++) {
                    for (int x0 = 0; x0 < Wo; x0++) {
                        uint32_t ind = (c0 * Ho + y0) * Wo + x0;
                        if (in_unpacked_half[ind] < 0.0f) {
                            out_unpacked_half[ind] = 0.0f;
                        }
                        else {
                            out_unpacked_half[ind] = in_unpacked_half[ind];
                        } 
                    }
                }
            }
        }

        // print Pool result
        // NvDlaDebugPrintf("=== Pool RESULT ===\n");
        // for (int c0 = 0; c0 < Co; c0++) {
        //     for (int y0 = 0; y0 < Ho; y0++) {
        //         for (int x0 = 0; x0 < Wo; x0++) {
        //             NvDlaDebugPrintf("%f ", float(out_unpacked_half[(c0 * Ho + y0) * Wo + x0]));
        //         }
        //         NvDlaDebugPrintf("\n");
        //     }
        // }
        // NvDlaDebugPrintf("=== EOF Pool RESULT ===\n");

        uint8_t *dst_bytes = reinterpret_cast<uint8_t*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset());
        uint8_t *out_unpacked_bytes = reinterpret_cast<uint8_t*>(out_unpacked_half);
        pack_nvdla_feature_map(out_unpacked_bytes, dst_bytes, Co, Ho, Wo, Wo, Wo, 32);
        
        // print sdp result
        NvDlaDebugPrintf("=== Sdp RESULT ===\n");
        for (int c0 = 0; c0 < Co; c0++) {
            for (int y0 = 0; y0 < Ho; y0++) {
                for (int x0 = 0; x0 < Wo; x0++) {
                    NvDlaDebugPrintf("%02x %02x ", dst_bytes[((c0 * Ho + y0) * Wo + x0)*2], dst_bytes[((c0 * Ho + y0) * Wo + x0)*2+1]);
                }
                NvDlaDebugPrintf("\n");
            }
            break;
        }
        NvDlaDebugPrintf("=== EOF Sdp RESULT ===\n");

        delete[] in_unpacked;
        delete[] out_unpacked_half;
    }
    else if ((*opDesc.src_precision() == EMU_FORMAT_INT8) || (*opDesc.src_precision() == EMU_FORMAT_INT8_8))
    {
        NvS8* pSrc = reinterpret_cast<NvS8*>( addressList[*src.addressIndex()] + *src.addressIndexOffset() );
        NvS8* pDst = reinterpret_cast<NvS8*>( addressList[*dst.addressIndex()] + *dst.addressIndexOffset() );

        half* pHalfSrc = reinterpret_cast<half*>(malloc(*src.channel() * sizeof(half)));
        half* pHalfDst = reinterpret_cast<half*>(malloc(*dst.channel() * sizeof(half)));
    }
    else
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "Don't support EMU sdp operation for format: %d\n",
            static_cast<NvU32>(*src.format()));
    }

fail:
    return e;
}



} // nvdla::priv
} // nvdla
