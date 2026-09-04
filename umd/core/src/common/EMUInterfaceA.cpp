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

#include "priv/EMUInterface.h"

#include "priv/emu/emu1/A/emu_version.h"
#include "priv/emu/emu1/A/emu_interface.h"

namespace nvdla
{
namespace priv
{

//
// struct emu_address
//
class EMUAddressA : public EMUAddress
{
public:
    virtual ~EMUAddressA() { }

    virtual size_t struct_size()  const { return sizeof(emu_address);    }
    virtual size_t struct_align() const { return 0; }

    virtual void * hMem(NvU8 *base)   const { return &ric(base)->hMem; }
    virtual NvU32 * offset(NvU8 *base)   const { return &ric(base)->offset; }

protected:
    static inline emu_address *ric(NvU8 *base) { return reinterpret_cast<emu_address *>(base); }
};
static EMUAddressA g_emu_address;
const EMUAddress & EMUInterfaceA::address() const { return g_emu_address; }


//
// struct emu_task_desc
//
class EMUTaskDescA : public EMUTaskDesc
{
public:
    virtual ~EMUTaskDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_task_desc);    }
    virtual size_t struct_align() const { return 256; }

    virtual NvU32 * numAddresses(NvU8 *base)   const { return &ric(base)->num_addresses; }
    virtual size_t maxBuffersPerTask() const { return NVDLA_EMU_MAX_BUFFERS_PER_TASK; }
    virtual EMUAddressAccessor addressList(NvU8 *base, size_t c)   const { return EMUAddressAccessor(cir(&ric(base)->address_list[c]), g_emu_address); }

protected:
    static inline NvU8          *cir(emu_address *c) { return reinterpret_cast<NvU8 *>(c);           }
    static inline emu_task_desc *ric(NvU8 *base) { return reinterpret_cast<emu_task_desc *>(base); }
};
static EMUTaskDescA g_emu_task_desc;
const EMUTaskDesc & EMUInterfaceA::taskDesc() const { return g_emu_task_desc; }


//
// struct emu_network_desc
//
class EMUNetworkDescA : public EMUNetworkDesc
{
public:
    virtual ~EMUNetworkDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_network_desc);    }
    virtual size_t struct_align() const { return 256; }

    virtual int16_t  * operationDescIndex(NvU8 *base)   const { return &ric(base)->operation_desc_index;   }
    virtual int16_t  * operationBufferDescIndex(NvU8 *base)  const { return &ric(base)->operation_buffer_desc_index; }
    virtual uint16_t * numOperations(NvU8 *base)        const { return &ric(base)->num_operations;         }

protected:
    static inline emu_network_desc *ric(NvU8 *base) { return reinterpret_cast<emu_network_desc *>(base); }
};
static EMUNetworkDescA g_emu_network_desc;
const EMUNetworkDesc & EMUInterfaceA::networkDesc() const { return g_emu_network_desc; }


//
// struct emu_common_op_desc
//
class EMUCommonOpDescA : public EMUCommonOpDesc
{
public:
    virtual ~EMUCommonOpDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_common_op_desc);    }
    virtual size_t struct_align() const { return 0; }

    virtual NvU8 * op_type(NvU8 *base) const { return &ric(base)->op_type; }
    virtual NvF32 * input_scale_factor(NvU8 *base) const { return &ric(base)->input_scale_factor; }
    virtual NvF32 * output_scale_factor(NvU8 *base) const { return &ric(base)->output_scale_factor; }

protected:
    static inline emu_common_op_desc *ric(NvU8 *base) { return reinterpret_cast<emu_common_op_desc *>(base); }
};
static EMUCommonOpDescA g_emu_common_op_desc;


//
// struct emu_power_op_desc
//
class EMUPowerOpDescA : public EMUPowerOpDesc
{
public:
    virtual ~EMUPowerOpDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_power_op_desc);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUCommonOpDescAccessor commonOpDescAccessor(NvU8 *base) const { return EMUCommonOpDescAccessor(cir(&(ric(base)->common)), g_emu_common_op_desc); }
    virtual NvF32 * power(NvU8 *base) const { return &ric(base)->power; }
    virtual NvF32 * scale(NvU8 *base) const { return &ric(base)->scale; }
    virtual NvF32 * shift(NvU8 *base) const { return &ric(base)->shift; }

protected:
    static inline NvU8          *cir(emu_common_op_desc *c)     { return reinterpret_cast<NvU8 *>(c);             }
    static inline emu_power_op_desc *ric(NvU8 *base) { return reinterpret_cast<emu_power_op_desc *>(base); }
};
static EMUPowerOpDescA g_emu_power_op_desc;


//
// struct emu_softmax_op_desc
//
class EMUSoftmaxOpDescA : public EMUSoftmaxOpDesc
{
public:
    virtual ~EMUSoftmaxOpDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_softmax_op_desc);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUCommonOpDescAccessor commonOpDescAccessor(NvU8 *base) const { return EMUCommonOpDescAccessor(cir(&(ric(base)->common)), g_emu_common_op_desc); }
    virtual NvU8 * axis(NvU8 *base) const { return &ric(base)->axis; }

protected:
    static inline NvU8          *cir(emu_common_op_desc *c)     { return reinterpret_cast<NvU8 *>(c);             }
    static inline emu_softmax_op_desc *ric(NvU8 *base) { return reinterpret_cast<emu_softmax_op_desc *>(base); }
};
static EMUSoftmaxOpDescA g_emu_softmax_op_desc;


//
// struct emu_conv_op_desc
//
class EMUConvOpDescA : public EMUConvOpDesc
{
public:
    virtual ~EMUConvOpDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_conv_op_desc);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUCommonOpDescAccessor commonOpDescAccessor(NvU8 *base) const { return EMUCommonOpDescAccessor(cir(&(ric(base)->common)), g_emu_common_op_desc); }
    virtual	NvU8 * conv_mode(NvU8 *base) const { return &ric(base)->conv_mode; }
    virtual	NvU8 * data_reuse(NvU8 *base) const { return &ric(base)->data_reuse; }
    virtual	NvU8 * weight_reuse(NvU8 *base) const { return &ric(base)->weight_reuse; }
    virtual	NvU8 * skip_data_rls(NvU8 *base) const { return &ric(base)->skip_data_rls; }
	virtual NvU8 * skip_weight_rls(NvU8 *base) const { return &ric(base)->skip_weight_rls; }
	virtual NvU8 * reserved0(NvU8 *base) const { return &ric(base)->reserved0; }
	virtual NvU16 * entry_per_slice(NvU8 *base) const { return &ric(base)->entry_per_slice; }
	virtual NvU8 * data_format(NvU8 *base) const { return &ric(base)->data_format; }
	virtual NvU8 * pixel_mapping(NvU8 *base) const { return &ric(base)->pixel_mapping; }
	virtual NvU16 * fetch_grain(NvU8 *base) const { return &ric(base)->fetch_grain; }
	virtual NvU8 * reserved_b(NvU8 *base) const { return &ric(base)->reserved_b[0]; }

	/* batch_num */
	virtual NvU8 * batch(NvU8 *base) const { return &ric(base)->batch; }
	/* dla_weight_format */
	virtual NvU8 * weight_format(NvU8 *base) const { return &ric(base)->weight_format; }
	virtual NvU8 * data_bank(NvU8 *base) const { return &ric(base)->data_bank; }
	virtual NvU8 * weight_bank(NvU8 *base) const { return &ric(base)->weight_bank; }
	virtual NvU32 * batch_stride(NvU8 *base) const { return &ric(base)->batch_stride; }
	virtual NvU8 * post_extension(NvU8 *base) const { return &ric(base)->post_extension; }
	virtual NvU8 * pixel_override(NvU8 *base) const { return &ric(base)->pixel_override; }
	virtual NvU16 * release(NvU8 *base) const { return &ric(base)->release; }
	virtual NvU16 * input_width_csc(NvU8 *base) const { return &ric(base)->input_width_csc; }
	virtual NvU16 * input_height_csc(NvU8 *base) const { return &ric(base)->input_height_csc; }
	virtual NvU16 * input_channel_csc(NvU8 *base) const { return &ric(base)->input_channel_csc; }
	virtual NvU16 * kernel_width_csc(NvU8 *base) const { return &ric(base)->kernel_width_csc; }
	virtual NvU16 * kernel_height_csc(NvU8 *base) const { return &ric(base)->kernel_height_csc; }
	virtual NvU16 * kernel_channel_csc(NvU8 *base) const { return &ric(base)->kernel_channel_csc; }
	virtual NvU16 * input_width_cmac(NvU8 *base) const { return &ric(base)->input_width_cmac; }
	virtual NvU16 * input_height_cmac(NvU8 *base) const { return &ric(base)->input_height_cmac; }

	/* actual size in bytes */
	virtual NvU32 * bytes_per_kernel(NvU8 *base) const { return &ric(base)->bytes_per_kernel; }

	/* Algorithm parameters */
	virtual NvS16 * mean_ry(NvU8 *base) const { return &ric(base)->mean_ry; } /* mean value for red in RGB or Y in YUV */
	virtual NvS16 * mean_gu(NvU8 *base) const { return &ric(base)->mean_gu; } /* mean value for green in RGB or U in YUV */
	virtual NvS16 * mean_bv(NvU8 *base) const { return &ric(base)->mean_bv; } /* mean value for blue in RGB or V in YUV */
	virtual NvS16 * mean_ax(NvU8 *base) const { return &ric(base)->mean_ax; }
	virtual NvU8 * mean_format(NvU8 *base) const { return &ric(base)->mean_format; } /* dla_mean_format */
	virtual NvU8 * conv_stride_x(NvU8 *base) const { return &ric(base)->conv_stride_x; }
	virtual NvU8 * conv_stride_y(NvU8 *base) const { return &ric(base)->conv_stride_y; }
	virtual NvU8 * pad_x_left(NvU8 *base) const { return &ric(base)->pad_x_left; }
	virtual NvU8 * pad_x_right(NvU8 *base) const { return &ric(base)->pad_x_right; }
	virtual NvU8 * pad_y_top(NvU8 *base) const { return &ric(base)->pad_y_top; }
	virtual NvU8 * pad_y_bottom(NvU8 *base) const { return &ric(base)->pad_y_bottom; }
	virtual NvU8 * dilation_x(NvU8 *base) const { return &ric(base)->dilation_x; }
	virtual NvU8 * dilation_y(NvU8 *base) const { return &ric(base)->dilation_y; }
    virtual NvU8 * reserved2(NvU8 *base) const { return &ric(base)->reserved2[0]; }
	virtual NvU8 * pra_truncate(NvU8 *base) const { return &ric(base)->pra_truncate; }

	virtual NvU8 * in_precision(NvU8 *base) const { return &ric(base)->in_precision; }
	/* The output precision from CONV, it's the MAC processing precison */
	virtual NvU8 * out_precision(NvU8 *base) const { return &ric(base)->out_precision; }
	virtual NvS16 * pad_val(NvU8 *base) const { return &ric(base)->pad_val; }

	/* input converter parameters */
	virtual NvS16 * in_cvt_scale(NvU8 *base) const { return &ric(base)->in_cvt_scale; }
	virtual NvU8 * in_cvt_truncate(NvU8 *base) const { return &ric(base)->in_cvt_truncate; }
	virtual NvU8 * in_cvt_enable(NvU8 *base) const { return &ric(base)->in_cvt_enable; }
	virtual NvS32 * in_cvt_offset(NvU8 *base) const { return &ric(base)->in_cvt_offset; }
	/* output converter parameters, support truncate only */
	virtual NvS16 * out_cvt_scale(NvU8 *base) const { return &ric(base)->out_cvt_scale; }
	virtual NvU8 * out_cvt_truncate(NvU8 *base) const { return &ric(base)->out_cvt_truncate; }
	virtual NvU8 * out_cvt_enable(NvU8 *base) const { return &ric(base)->out_cvt_enable; }
	virtual NvS32 * out_cvt_offset(NvU8 *base) const { return &ric(base)->out_cvt_offset; }


	//////////////////////
	/* BIAS paramteters */
	//////////////////////
	/* dla_precision */
	virtual NvU8 * src_precision(NvU8 *base) const { return &ric(base)->src_precision; }
	virtual NvU8 * dst_precision(NvU8 *base) const { return &ric(base)->dst_precision; }
	virtual NvS16 * lut_index(NvU8 *base) const { return &ric(base)->lut_index; }
	virtual NvU8 * batch_num(NvU8 *base) const { return &ric(base)->batch_num; }

	// x1 params
	virtual NvU8 * x1_op_enable(NvU8 *base) const { return &ric(base)->x1_op_enable; }
	virtual NvU8 * x1_op_alu_type(NvU8 *base) const { return &ric(base)->x1_op_alu_type; }
	virtual NvU8 * x1_op_type(NvU8 *base) const { return &ric(base)->x1_op_type; }
	virtual NvU8 * x1_op_mode(NvU8 *base) const { return &ric(base)->x1_op_mode; }
	virtual NvU8 * x1_op_act(NvU8 *base) const { return &ric(base)->x1_op_act; }
	virtual NvU8 * x1_op_shift_value(NvU8 *base) const { return &ric(base)->x1_op_shift_value; }
	virtual NvU8 * x1_op_truncate(NvU8 *base) const { return &ric(base)->x1_op_truncate; }
	virtual NvU8 * x1_op_precision(NvU8 *base) const { return &ric(base)->x1_op_precision; }
	virtual NvS32 * x1_op_alu_operand(NvU8 *base) const { return &ric(base)->x1_op_alu_operand; }
	virtual NvS32 * x1_op_mul_operand(NvU8 *base) const { return &ric(base)->x1_op_mul_operand; }
	virtual NvS16 * x1_op_cvt_alu_cvt_scale(NvU8 *base) const { return &ric(base)->x1_op_cvt_alu_cvt_scale; }
	virtual NvU8 * x1_op_cvt_alu_cvt_truncate(NvU8 *base) const { return &ric(base)->x1_op_cvt_alu_cvt_truncate; }
	virtual NvU8 * x1_op_cvt_alu_cvt_enable(NvU8 *base) const { return &ric(base)->x1_op_cvt_alu_cvt_enable; }
	virtual NvS32 * x1_op_cvt_alu_cvt_offset(NvU8 *base) const { return &ric(base)->x1_op_cvt_alu_cvt_offset; }
	virtual NvS16 * x1_op_cvt_mul_cvt_scale(NvU8 *base) const { return &ric(base)->x1_op_cvt_mul_cvt_scale; }
	virtual NvU8 * x1_op_cvt_mul_cvt_truncate(NvU8 *base) const { return &ric(base)->x1_op_cvt_mul_cvt_truncate; }
	virtual NvU8 * x1_op_cvt_mul_cvt_enable(NvU8 *base) const { return &ric(base)->x1_op_cvt_mul_cvt_enable; }
	virtual NvS32 * x1_op_cvt_mul_cvt_offset(NvU8 *base) const { return &ric(base)->x1_op_cvt_mul_cvt_offset; }
	virtual NvU8 * has_relu(NvU8 *base) const { return &ric(base)->has_relu; }

protected:
    static inline NvU8          *cir(emu_common_op_desc *c)     { return reinterpret_cast<NvU8 *>(c);             }
    static inline emu_conv_op_desc *ric(NvU8 *base) { return reinterpret_cast<emu_conv_op_desc *>(base); }
};
static EMUConvOpDescA g_emu_conv_op_desc;

//
// struct emu_pool_op_desc
//
class EMUPoolOpDescA : public EMUPoolOpDesc
{
public:
    virtual ~EMUPoolOpDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_pool_op_desc);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUCommonOpDescAccessor commonOpDescAccessor(NvU8 *base) const { return EMUCommonOpDescAccessor(cir(&(ric(base)->common)), g_emu_common_op_desc); }
    virtual NvU16 * partial_in_width_first(NvU8 *base) const { return &ric(base)->partial_in_width_first; }
	virtual NvU16 * partial_in_width_mid(NvU8 *base) const { return &ric(base)->partial_in_width_mid; }
	virtual NvU16 * partial_in_width_last(NvU8 *base) const { return &ric(base)->partial_in_width_last; }
	virtual NvU16 * partial_width_first(NvU8 *base) const { return &ric(base)->partial_width_first; }
	virtual NvU16 * partial_width_mid(NvU8 *base) const { return &ric(base)->partial_width_mid; }
	virtual NvU16 * partial_width_last(NvU8 *base) const { return &ric(base)->partial_width_last; }
	virtual NvU8 * split_num(NvU8 *base) const { return &ric(base)->split_num; }
	virtual NvU8 * pool_mode(NvU8 *base) const { return &ric(base)->pool_mode; }
	virtual NvU8 * pool_width(NvU8 *base) const { return &ric(base)->pool_width; }
	virtual NvU8 * pool_height(NvU8 *base) const { return &ric(base)->pool_height; }
	virtual NvU8 * stride_x(NvU8 *base) const { return &ric(base)->stride_x; }
	virtual NvU8 * stride_y(NvU8 *base) const { return &ric(base)->stride_y; }
	virtual NvU8 * pad_left(NvU8 *base) const { return &ric(base)->pad_left; }
	virtual NvU8 * pad_right(NvU8 *base) const { return &ric(base)->pad_right; }
	virtual NvU8 * pad_top(NvU8 *base) const { return &ric(base)->pad_top; }
	virtual NvU8 * pad_bottom(NvU8 *base) const { return &ric(base)->pad_bottom; }
	virtual NvU8 * precision(NvU8 *base) const { return &ric(base)->precision; }
	virtual NvU8 * reserved0(NvU8 *base) const { return &ric(base)->reserved0; }
	virtual NvS32 * padding_value(NvU8 *base) const { return &ric(base)->padding_value[0]; }

protected:
    static inline NvU8          *cir(emu_common_op_desc *c)     { return reinterpret_cast<NvU8 *>(c);             }
    static inline emu_pool_op_desc *ric(NvU8 *base) { return reinterpret_cast<emu_pool_op_desc *>(base); }
};
static EMUPoolOpDescA g_emu_pool_op_desc;

//
// struct emu_sdp_op_desc
//
class EMUSdpOpDescA : public EMUSdpOpDesc
{
public:
    virtual ~EMUSdpOpDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_sdp_op_desc);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUCommonOpDescAccessor commonOpDescAccessor(NvU8 *base) const { return EMUCommonOpDescAccessor(cir(&(ric(base)->common)), g_emu_common_op_desc); }
    virtual uint8_t * src_precision(NvU8 *base) const { return &ric(base)->src_precision; }
	virtual uint8_t * dst_precision(NvU8 *base) const { return &ric(base)->dst_precision; }
	virtual int16_t * lut_index(NvU8 *base) const { return &ric(base)->lut_index; }
	virtual struct emu_cvt_param * out_cvt(NvU8 *base) const { return &ric(base)->out_cvt; }
	virtual uint8_t * conv_mode(NvU8 *base) const { return &ric(base)->conv_mode; }
	virtual uint8_t * batch_num(NvU8 *base) const { return &ric(base)->batch_num; }
	virtual uint16_t * reserved0(NvU8 *base) const { return &ric(base)->reserved0; }
	virtual uint32_t * batch_stride(NvU8 *base) const { return &ric(base)->batch_stride; }
	virtual struct emu_sdp_op * x1_op(NvU8 *base) const { return &ric(base)->x1_op; }
	virtual struct emu_sdp_op * x2_op(NvU8 *base) const { return &ric(base)->x2_op; }
	virtual struct emu_sdp_op * y_op(NvU8 *base) const { return &ric(base)->y_op; }

protected:
    static inline NvU8          *cir(emu_common_op_desc *c)     { return reinterpret_cast<NvU8 *>(c);             }
    static inline emu_sdp_op_desc *ric(NvU8 *base) { return reinterpret_cast<emu_sdp_op_desc *>(base); }
};
static EMUSdpOpDescA g_emu_sdp_op_desc;


//
// struct emu_operation_container
//
class EMUOperationContainerA : public EMUOperationContainer
{
public:
    virtual ~EMUOperationContainerA() { }

    virtual size_t struct_size()  const { return sizeof(emu_operation_container);    }
    virtual size_t struct_align() const { return 0; }

    virtual EMUPowerOpDescAccessor powerOpDescAccessor(NvU8 *base, size_t c) const { return EMUPowerOpDescAccessor(sir(&(ric(base)[c].power_op)), g_emu_power_op_desc); }
    virtual EMUSoftmaxOpDescAccessor softmaxOpDescAccessor(NvU8 *base, size_t c) const { return EMUSoftmaxOpDescAccessor(sir(&(ric(base)[c].softmax_op)), g_emu_softmax_op_desc); }
    virtual EMUConvOpDescAccessor convOpDescAccessor(NvU8 *base, size_t c) const { return EMUConvOpDescAccessor(sir(&(ric(base)[c].conv_op)), g_emu_conv_op_desc); }
    virtual EMUPoolOpDescAccessor poolOpDescAccessor(NvU8 *base, size_t c) const { return EMUPoolOpDescAccessor(sir(&(ric(base)[c].pool_op)), g_emu_pool_op_desc); }
    virtual EMUSdpOpDescAccessor sdpOpDescAccessor(NvU8 *base, size_t c) const { return EMUSdpOpDescAccessor(sir(&(ric(base)[c].sdp_op)), g_emu_sdp_op_desc); }

protected:
    static inline NvU8          *sir(emu_power_op_desc *c)       { return reinterpret_cast<NvU8 *>(c);             }
    static inline NvU8          *sir(emu_softmax_op_desc *c)     { return reinterpret_cast<NvU8 *>(c);             }
    static inline NvU8          *sir(emu_conv_op_desc *c)        { return reinterpret_cast<NvU8 *>(c);             }
    static inline NvU8          *sir(emu_pool_op_desc *c)        { return reinterpret_cast<NvU8 *>(c);             }
    static inline NvU8          *sir(emu_sdp_op_desc *c)         { return reinterpret_cast<NvU8 *>(c);             }
    static inline emu_operation_container *ric(NvU8 *base)       { return reinterpret_cast<emu_operation_container *>(base); }
};
static EMUOperationContainerA g_emu_operation_container;
const EMUOperationContainer & EMUInterfaceA::operationContainer() const { return g_emu_operation_container; }


//
// struct emu_buffer_desc
//
class EMUBufferDescA : public EMUBufferDesc
{
public:
    virtual ~EMUBufferDescA() { }

    virtual size_t struct_size()  const { return sizeof(emu_buffer_desc);    }
    virtual size_t struct_align() const { return 256; }

    virtual NvS16 * addressIndex(NvU8 *base)    const { return &ric(base)->addressIndex; }
    virtual NvU32 * addressIndexOffset(NvU8 *base)    const { return &ric(base)->addressIndexOffset; }
    virtual NvU32 * size(NvU8 *base)       const { return &ric(base)->size; }
    virtual NvU16 * format(NvU8 *base)     const { return &ric(base)->format; }
    virtual NvU16   format_FF16()          const { return EMU_FORMAT_FF16; }
    virtual NvU16   format_INT8()          const { return EMU_FORMAT_INT8; }
    virtual NvU16   format_INT8_8()        const { return EMU_FORMAT_INT8_8; }
    virtual NvU16   format_UINT8()         const { return EMU_FORMAT_UINT8; }
    virtual NvU16   format_INT16()         const { return EMU_FORMAT_INT16; }
    virtual NvU16   format_UINT16()        const { return EMU_FORMAT_UINT16; }
    virtual NvU16 * width(NvU8 *base)      const { return &ric(base)->width; }
    virtual NvU16 * height(NvU8 *base)     const { return &ric(base)->height; }
    virtual NvU16 * channel(NvU8 *base)    const { return &ric(base)->channel; }
    virtual NvU32 * lineStride(NvU8 *base) const { return &ric(base)->line_stride; }
    virtual NvU32 * surfStride(NvU8 *base) const { return &ric(base)->surf_stride; }
    virtual NvU32 * planeStride(NvU8 *base) const { return &ric(base)->plane_stride; }

protected:
    static inline emu_buffer_desc *ric(NvU8 *base)       { return reinterpret_cast<emu_buffer_desc *>(base); }
};
static EMUBufferDescA g_emu_buffer_desc;
const EMUBufferDesc & EMUInterfaceA::bufferDesc() const { return g_emu_buffer_desc; }


//
// struct emu_power_buffer_descs
//

class EMUPowerBufferDescsA : public EMUPowerBufferDescs
{
public:
    virtual ~EMUPowerBufferDescsA() { }

    virtual size_t struct_size()  const { return sizeof(emu_power_buffer_descs);    }
    virtual size_t struct_align() const { return 4; /* see __attribute__ */ }

    virtual EMUBufferDescAccessor srcDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->src_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor dstDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->dst_data), g_emu_buffer_desc); }

protected:
    static inline NvU8          *dir(emu_buffer_desc *d)       { return reinterpret_cast<NvU8 *>(d);             }
    static inline emu_power_buffer_descs *ric(NvU8 *base)    { return reinterpret_cast<emu_power_buffer_descs *>(base); }
};
static EMUPowerBufferDescsA g_emu_power_buffer_descs;


//
// struct emu_softmax_buffer_descs
//

class EMUSoftmaxBufferDescsA : public EMUSoftmaxBufferDescs
{
public:
    virtual ~EMUSoftmaxBufferDescsA() { }

    virtual size_t struct_size()  const { return sizeof(emu_softmax_buffer_descs);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUBufferDescAccessor srcDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->src_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor dstDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->dst_data), g_emu_buffer_desc); }

protected:
    static inline NvU8          *dir(emu_buffer_desc *d)       { return reinterpret_cast<NvU8 *>(d);             }
    static inline emu_softmax_buffer_descs *ric(NvU8 *base)    { return reinterpret_cast<emu_softmax_buffer_descs *>(base); }
};
static EMUSoftmaxBufferDescsA g_emu_softmax_buffer_descs;


//
// struct emu_conv_buffer_descs
//

class EMUConvBufferDescsA : public EMUConvBufferDescs
{
public:
    virtual ~EMUConvBufferDescsA() { }

    virtual size_t struct_size()  const { return sizeof(emu_conv_buffer_descs);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUBufferDescAccessor weightDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->weight_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor wmbDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->wmb_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor wgsDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->wgs_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor biasDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->bias_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor srcDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->src_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor dstDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->dst_data), g_emu_buffer_desc); }

protected:
    static inline NvU8          *dir(emu_buffer_desc *d)       { return reinterpret_cast<NvU8 *>(d);             }
    static inline emu_conv_buffer_descs *ric(NvU8 *base)    { return reinterpret_cast<emu_conv_buffer_descs *>(base); }
};
static EMUConvBufferDescsA g_emu_conv_buffer_descs;

//
// struct emu_pool_buffer_descs
//

class EMUPoolBufferDescsA : public EMUPoolBufferDescs
{
public:
    virtual ~EMUPoolBufferDescsA() { }

    virtual size_t struct_size()  const { return sizeof(emu_pool_buffer_descs);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUBufferDescAccessor srcDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->src_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor dstDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->dst_data), g_emu_buffer_desc); }

protected:
    static inline NvU8          *dir(emu_buffer_desc *d)       { return reinterpret_cast<NvU8 *>(d);             }
    static inline emu_pool_buffer_descs *ric(NvU8 *base)    { return reinterpret_cast<emu_pool_buffer_descs *>(base); }
};
static EMUPoolBufferDescsA g_emu_pool_buffer_descs;

//
// struct emu_sdp_buffer_descs
//

class EMUSdpBufferDescsA : public EMUSdpBufferDescs
{
public:
    virtual ~EMUSdpBufferDescsA() { }

    virtual size_t struct_size()  const { return sizeof(emu_sdp_buffer_descs);    }
    virtual size_t struct_align() const { return 4; }

    virtual EMUBufferDescAccessor srcDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->src_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor x1DataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->x1_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor x2DataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->x2_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor yDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->y_data), g_emu_buffer_desc); }
    virtual EMUBufferDescAccessor dstDataAccessor(NvU8 *base) const { return EMUBufferDescAccessor(dir(&ric(base)->dst_data), g_emu_buffer_desc); }

protected:
    static inline NvU8          *dir(emu_buffer_desc *d)       { return reinterpret_cast<NvU8 *>(d);             }
    static inline emu_sdp_buffer_descs *ric(NvU8 *base)    { return reinterpret_cast<emu_sdp_buffer_descs *>(base); }
};
static EMUSdpBufferDescsA g_emu_sdp_buffer_descs;


//
// struct emu_operation_buffer_container
//
class EMUOperationBufferContainerA : public EMUOperationBufferContainer
{
public:
    virtual ~EMUOperationBufferContainerA() { }

    virtual size_t struct_size()  const { return sizeof(emu_operation_buffer_container);    }
    virtual size_t struct_align() const { return 0; }

    virtual EMUPowerBufferDescsAccessor powerBufferDescsAccessor(NvU8 *base, size_t c) const
    {
        return EMUPowerBufferDescsAccessor(sir( &(ric(base)[c]).power_buffers), g_emu_power_buffer_descs);
    }

    virtual EMUSoftmaxBufferDescsAccessor softmaxBufferDescsAccessor(NvU8 *base, size_t c) const
    {
        return EMUSoftmaxBufferDescsAccessor(sir( &(ric(base)[c]).softmax_buffers), g_emu_softmax_buffer_descs);
    }

    virtual EMUConvBufferDescsAccessor convBufferDescsAccessor(NvU8 *base, size_t c) const
    {
        return EMUConvBufferDescsAccessor(sir( &(ric(base)[c]).conv_buffers), g_emu_conv_buffer_descs);
    }

    virtual EMUPoolBufferDescsAccessor poolBufferDescsAccessor(NvU8 *base, size_t c) const
    {
        return EMUPoolBufferDescsAccessor(sir( &(ric(base)[c]).pool_buffers), g_emu_pool_buffer_descs);
    }

    virtual EMUSdpBufferDescsAccessor sdpBufferDescsAccessor(NvU8 *base, size_t c) const
    {
        return EMUSdpBufferDescsAccessor(sir( &(ric(base)[c]).sdp_buffers), g_emu_sdp_buffer_descs);
    }
protected:
    static inline NvU8 *sir(emu_power_buffer_descs *c) { return reinterpret_cast<NvU8 *>(c); }
    static inline NvU8 *sir(emu_softmax_buffer_descs *c) { return reinterpret_cast<NvU8 *>(c); }
    static inline NvU8 *sir(emu_conv_buffer_descs *c) { return reinterpret_cast<NvU8 *>(c); }
    static inline NvU8 *sir(emu_pool_buffer_descs *c) { return reinterpret_cast<NvU8 *>(c); }
    static inline NvU8 *sir(emu_sdp_buffer_descs *c) { return reinterpret_cast<NvU8 *>(c); }
    static inline emu_operation_buffer_container *ric(NvU8 *base)  { return reinterpret_cast<emu_operation_buffer_container *>(base); }
};
static EMUOperationBufferContainerA g_emu_operation_buffer_container;
const EMUOperationBufferContainer & EMUInterfaceA::operationBufferContainer() const { return g_emu_operation_buffer_container; }


//
// interface
//

NvU8 EMUInterfaceA::emulatorTargetVersionMajor()    const { return EMULATOR_VERSION_MAJOR;    }
NvU8 EMUInterfaceA::emulatorTargetVersionMinor()    const { return EMULATOR_VERSION_MINOR;    }
NvU8 EMUInterfaceA::emulatorTargetVersionSubminor() const { return EMULATOR_VERSION_SUBMINOR; }

NvU32 EMUInterfaceA::emulatorTargetVersion() const { return emu_version(); }

const std::string EMUInterfaceA::emulatorTargetGerritChange() const { return emu_gerrit_change(); }
const std::string EMUInterfaceA::emulatorTargetGerritReview() const { return emu_gerrit_review(); }

NvU8 EMUInterfaceA::emulatorVersionMajor() const
{
    return EMULATOR_VERSION_MAJOR;
}

NvU8 EMUInterfaceA::emulatorVersionMinor() const
{
    return EMULATOR_VERSION_MINOR;
}

NvU8 EMUInterfaceA::emulatorVersionSubminor() const
{
    return EMULATOR_VERSION_SUBMINOR;
}

NvU32 EMUInterfaceA::emulatorVersion() const
{
    return emu_version();
}



} // nvdla::priv
} // nvdla
