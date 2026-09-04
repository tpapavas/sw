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

#ifndef NVDLA_PRIV_EMU_INTERFACE_H
#define NVDLA_PRIV_EMU_INTERFACE_H

#include <stdbool.h>

#include "priv/Type.h"
#include "priv/EMUInterfaceEnums.h"
#include "priv/emu/emu1/A/emu_interface.h"

#include "dlaerror.h"
#include "dlatypes.h"

#define EMU_FORMAT_INT8     0
#define EMU_FORMAT_INT8_8   1
#define EMU_FORMAT_INT16    2
#define EMU_FORMAT_FF16     3
#define EMU_FORMAT_UINT8    4
#define EMU_FORMAT_UINT16   5

namespace nvdla
{

namespace priv
{

//
// struct emu_address
//
class EMUAddress
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual void * hMem(NvU8 *base)  const = 0;
    virtual NvU32 * offset(NvU8 *base)  const = 0;

protected:
    EMUAddress()          { }
    virtual ~EMUAddress() { }
};

class EMUAddressAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    void * hMem()  const;
    NvU32 * offset()  const;

    EMUAddressAccessor(NvU8 *base, const EMUAddress &);

protected:
    NvU8 *_base;
    const EMUAddress &_n;
};


//
// struct emu_task_desc
//
class EMUTaskDesc
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual NvU32 * numAddresses(NvU8 *base)   const = 0;
    virtual size_t maxBuffersPerTask() const = 0;
    virtual EMUAddressAccessor addressList(NvU8 *base, size_t c) const = 0;

protected:
    EMUTaskDesc()          { }
    virtual ~EMUTaskDesc() { }
};

class EMUTaskDescAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    NvU32 * numAddresses()   const;
    size_t maxBuffersPerTask() const;
    EMUAddressAccessor addressList(size_t c) const;

    EMUTaskDescAccessor(NvU8 *base, const EMUTaskDesc &);

protected:
    NvU8 *_base;
    const EMUTaskDesc &_n;
};

//
// struct emu_network_desc
//
class EMUNetworkDesc
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual NvS16 * operationDescIndex(NvU8 *base)   const = 0;
    virtual NvS16 * operationBufferDescIndex(NvU8 *base)     const = 0;
    virtual NvU16 * numOperations(NvU8 *base)        const = 0;

protected:
    EMUNetworkDesc()          { }
    virtual ~EMUNetworkDesc() { }
};

class EMUNetworkDescAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    NvS16 * operationDescIndex()   const;
    NvS16 * operationBufferDescIndex()     const;
    NvU16 * numOperations()        const;

    EMUNetworkDescAccessor(NvU8 *base, const EMUNetworkDesc &);

protected:
    NvU8 *_base;
    const EMUNetworkDesc &_n;
};

//
// struct emu_common_op_desc
//
class EMUCommonOpDesc
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual NvU8 * op_type(NvU8 *base) const = 0;
    virtual NvF32 * input_scale_factor(NvU8 *base) const = 0;
    virtual NvF32 * output_scale_factor(NvU8 *base) const = 0;

protected:
    EMUCommonOpDesc()          { }
    virtual ~EMUCommonOpDesc() { }
};

class EMUCommonOpDescAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    NvU8 * op_type() const;
    NvF32 * input_scale_factor() const;
    NvF32 * output_scale_factor() const;

    EMUCommonOpDescAccessor(NvU8 *base, const EMUCommonOpDesc &);

protected:
    NvU8 *_base;
    const EMUCommonOpDesc &_n;
};


//
// struct emu_power_op_desc
//
class EMUPowerOpDesc
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual EMUCommonOpDescAccessor commonOpDescAccessor(NvU8 *base) const = 0;
    virtual NvF32 * power(NvU8 *base) const = 0;
    virtual NvF32 * scale(NvU8 *base) const = 0;
    virtual NvF32 * shift(NvU8 *base) const = 0;

protected:
    EMUPowerOpDesc()          { }
    virtual ~EMUPowerOpDesc() { }
};

class EMUPowerOpDescAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    EMUCommonOpDescAccessor commonOpDescAccessor() const;
    NvF32 * power() const;
    NvF32 * scale() const;
    NvF32 * shift() const;

    EMUPowerOpDescAccessor(NvU8 *base, const EMUPowerOpDesc &);

protected:
    NvU8 *_base;
    const EMUPowerOpDesc &_n;
};


//
// struct emu_softmax_op_desc
//
class EMUSoftmaxOpDesc
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual EMUCommonOpDescAccessor commonOpDescAccessor(NvU8 *base) const = 0;
    virtual NvU8 * axis(NvU8 *base) const = 0;

protected:
    EMUSoftmaxOpDesc()          { }
    virtual ~EMUSoftmaxOpDesc() { }
};

class EMUSoftmaxOpDescAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    EMUCommonOpDescAccessor commonOpDescAccessor() const;
    NvU8 * axis() const;

    EMUSoftmaxOpDescAccessor(NvU8 *base, const EMUSoftmaxOpDesc &);

protected:
    NvU8 *_base;
    const EMUSoftmaxOpDesc &_n;
};

//
// struct emu_conv_op_desc
//
class EMUConvOpDesc
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual EMUCommonOpDescAccessor commonOpDescAccessor(NvU8 *base) const = 0;

    virtual	NvU8 * conv_mode(NvU8 *base) const = 0;
    virtual	NvU8 * data_reuse(NvU8 *base) const = 0;
    virtual	NvU8 * weight_reuse(NvU8 *base) const = 0;
    virtual	NvU8 * skip_data_rls(NvU8 *base) const = 0;

	virtual NvU8 * skip_weight_rls(NvU8 *base) const = 0;
	virtual NvU8 * reserved0(NvU8 *base) const = 0;
	virtual NvU16 * entry_per_slice(NvU8 *base) const = 0;

	/* dla_data_format */
	virtual NvU8 * data_format(NvU8 *base) const = 0;
	/* dla_pixel_mapping */
	virtual NvU8 * pixel_mapping(NvU8 *base) const = 0;
	/* number of free slices before fetch */
	virtual NvU16 * fetch_grain(NvU8 *base) const = 0;

    // reserved_b[8]
	virtual NvU8 * reserved_b(NvU8 *base) const = 0;

	/* batch_num */
	virtual NvU8 * batch(NvU8 *base) const = 0;
	/* dla_weight_format */
	virtual NvU8 * weight_format(NvU8 *base) const = 0;
	virtual NvU8 * data_bank(NvU8 *base) const = 0;
	virtual NvU8 * weight_bank(NvU8 *base) const = 0;

	/* the offset in bytes of each data cube in a batch */
	virtual NvU32 * batch_stride(NvU8 *base) const = 0;

	virtual NvU8 * post_extension(NvU8 *base) const = 0;
	virtual NvU8 * pixel_override(NvU8 *base) const = 0;
	/* number of slices need to be released */
	virtual NvU16 * release(NvU8 *base) const = 0;

	 /* The input cube dimension for CSC */
	virtual NvU16 * input_width_csc(NvU8 *base) const = 0;
	virtual NvU16 * input_height_csc(NvU8 *base) const = 0;

	virtual NvU16 * input_channel_csc(NvU8 *base) const = 0;
	virtual NvU16 * kernel_width_csc(NvU8 *base) const = 0;

	virtual NvU16 * kernel_height_csc(NvU8 *base) const = 0;
	virtual NvU16 * kernel_channel_csc(NvU8 *base) const = 0;

	/* The input cube dimension for CMAC */
	virtual NvU16 * input_width_cmac(NvU8 *base) const = 0;
	virtual NvU16 * input_height_cmac(NvU8 *base) const = 0;

	/* actual size in bytes */
	virtual NvU32 * bytes_per_kernel(NvU8 *base) const = 0;

	/* Algorithm parameters */
	virtual NvS16 * mean_ry(NvU8 *base) const = 0; /* mean value for red in RGB or Y in YUV */
	virtual NvS16 * mean_gu(NvU8 *base) const = 0; /* mean value for green in RGB or U in YUV */

	virtual NvS16 * mean_bv(NvU8 *base) const = 0; /* mean value for blue in RGB or V in YUV */
	virtual NvS16 * mean_ax(NvU8 *base) const = 0;

	virtual NvU8 * mean_format(NvU8 *base) const = 0; /* dla_mean_format */
	virtual NvU8 * conv_stride_x(NvU8 *base) const = 0;
	virtual NvU8 * conv_stride_y(NvU8 *base) const = 0;
	virtual NvU8 * pad_x_left(NvU8 *base) const = 0;

	virtual NvU8 * pad_x_right(NvU8 *base) const = 0;
	virtual NvU8 * pad_y_top(NvU8 *base) const = 0;
	virtual NvU8 * pad_y_bottom(NvU8 *base) const = 0;
	virtual NvU8 * dilation_x(NvU8 *base) const = 0;

	virtual NvU8 * dilation_y(NvU8 *base) const = 0;
	// reserved2[2]
    virtual NvU8 * reserved2(NvU8 *base) const = 0;

	/* Precision parameters */
	virtual NvU8 * pra_truncate(NvU8 *base) const = 0;

	virtual NvU8 * in_precision(NvU8 *base) const = 0;
	/* The output precision from CONV, it's the MAC processing precison */
	virtual NvU8 * out_precision(NvU8 *base) const = 0;
	virtual NvS16 * pad_val(NvU8 *base) const = 0;

	/* input converter parameters */
	virtual NvS16 * in_cvt_scale(NvU8 *base) const = 0;
	virtual NvU8 * in_cvt_truncate(NvU8 *base) const = 0;
	virtual NvU8 * in_cvt_enable(NvU8 *base) const = 0;
	virtual NvS32 * in_cvt_offset(NvU8 *base) const = 0;
	/* output converter parameters, support truncate only */
	virtual NvS16 * out_cvt_scale(NvU8 *base) const = 0;
	virtual NvU8 * out_cvt_truncate(NvU8 *base) const = 0;
	virtual NvU8 * out_cvt_enable(NvU8 *base) const = 0;
	virtual NvS32 * out_cvt_offset(NvU8 *base) const = 0;


	//////////////////////
	/* BIAS paramteters */
	//////////////////////
	/* dla_precision */
	virtual NvU8 * src_precision(NvU8 *base) const = 0;
	virtual NvU8 * dst_precision(NvU8 *base) const = 0;
	virtual NvS16 * lut_index(NvU8 *base) const = 0;

	/* Performance parameters */
	virtual NvU8 * batch_num(NvU8 *base) const = 0;

	// x1 params
	virtual NvU8 * x1_op_enable(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_alu_type(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_type(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_mode(NvU8 *base) const = 0;

	virtual NvU8 * x1_op_act(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_shift_value(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_truncate(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_precision(NvU8 *base) const = 0;

	virtual NvS32 * x1_op_alu_operand(NvU8 *base) const = 0;
	virtual NvS32 * x1_op_mul_operand(NvU8 *base) const = 0;

	// struct dla_sdp_cvt x1_op_cvt;
	virtual NvS16 * x1_op_cvt_alu_cvt_scale(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_cvt_alu_cvt_truncate(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_cvt_alu_cvt_enable(NvU8 *base) const = 0;
	virtual NvS32 * x1_op_cvt_alu_cvt_offset(NvU8 *base) const = 0;

	virtual NvS16 * x1_op_cvt_mul_cvt_scale(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_cvt_mul_cvt_truncate(NvU8 *base) const = 0;
	virtual NvU8 * x1_op_cvt_mul_cvt_enable(NvU8 *base) const = 0;
	virtual NvS32 * x1_op_cvt_mul_cvt_offset(NvU8 *base) const = 0;

    virtual NvU8 * has_relu(NvU8 *base) const = 0;
protected:
    EMUConvOpDesc()          { }
    virtual ~EMUConvOpDesc() { }
};

class EMUConvOpDescAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    EMUCommonOpDescAccessor commonOpDescAccessor() const;

    NvU8 * conv_mode() const;
    NvU8 * data_reuse() const;
    NvU8 * weight_reuse() const;
    NvU8 * skip_data_rls() const;

	NvU8 * skip_weight_rls() const;
	NvU8 * reserved0() const;
	NvU16 * entry_per_slice() const;

	/* dla_data_format */
	NvU8 * data_format() const;
	/* dla_pixel_mapping */
	NvU8 * pixel_mapping() const;
	/* number of free slices before fetch */
	NvU16 * fetch_grain() const;

    // reserved_b[8]
	NvU8 * reserved_b() const;

	/* batch_num */
	NvU8 * batch() const;
	/* dla_weight_format */
	NvU8 * weight_format() const;
	NvU8 * data_bank() const;
	NvU8 * weight_bank() const;

	/* the offset in bytes of each data cube in a batch */
	NvU32 * batch_stride() const;

	NvU8 * post_extension() const;
	NvU8 * pixel_override() const;
	/* number of slices need to be released */
	NvU16 * release() const;

	 /* The input cube dimension for CSC */
	NvU16 * input_width_csc() const;
	NvU16 * input_height_csc() const;

	NvU16 * input_channel_csc() const;
	NvU16 * kernel_width_csc() const;

	NvU16 * kernel_height_csc() const;
	NvU16 * kernel_channel_csc() const;

	/* The input cube dimension for CMAC */
	NvU16 * input_width_cmac() const;
	NvU16 * input_height_cmac() const;

	/* actual size in bytes */
	NvU32 * bytes_per_kernel() const;

	/* Algorithm parameters */
	NvS16 * mean_ry() const; /* mean value for red in RGB or Y in YUV */
	NvS16 * mean_gu() const; /* mean value for green in RGB or U in YUV */

	NvS16 * mean_bv() const; /* mean value for blue in RGB or V in YUV */
	NvS16 * mean_ax() const;

	NvU8 * mean_format() const; /* dla_mean_format */
	NvU8 * conv_stride_x() const;
	NvU8 * conv_stride_y() const;
	NvU8 * pad_x_left() const;

	NvU8 * pad_x_right() const;
	NvU8 * pad_y_top() const;
	NvU8 * pad_y_bottom() const;
	NvU8 * dilation_x() const;

	NvU8 * dilation_y() const;
	// reserved2[2]
    NvU8 * reserved2() const;

	/* Precision parameters */
	NvU8 * pra_truncate() const;

	NvU8 * in_precision() const;
	/* The output precision from CONV, it's the MAC processing precison */
	NvU8 * out_precision() const;
	NvS16 * pad_val() const;

	/* input converter parameters */
	NvS16 * in_cvt_scale() const;
	NvU8 * in_cvt_truncate() const;
	NvU8 * in_cvt_enable() const;
	NvS32 * in_cvt_offset() const;
	/* output converter parameters, support truncate only */
	NvS16 * out_cvt_scale() const;
	NvU8 * out_cvt_truncate() const;
	NvU8 * out_cvt_enable() const;
	NvS32 * out_cvt_offset() const;


	//////////////////////
	/* BIAS paramteters */
	//////////////////////
	/* dla_precision */
	NvU8 * src_precision() const;
	NvU8 * dst_precision() const;
	NvS16 * lut_index() const;

	/* Performance parameters */
	NvU8 * batch_num() const;

	// x1 params
	NvU8 * x1_op_enable() const;
	NvU8 * x1_op_alu_type() const;
	NvU8 * x1_op_type() const;
	NvU8 * x1_op_mode() const;

	NvU8 * x1_op_act() const;
	NvU8 * x1_op_shift_value() const;
	NvU8 * x1_op_truncate() const;
	NvU8 * x1_op_precision() const;

	NvS32 * x1_op_alu_operand() const;
	NvS32 * x1_op_mul_operand() const;

	// struct dla_sdp_cvt x1_op_cvt;
	NvS16 * x1_op_cvt_alu_cvt_scale() const;
	NvU8 * x1_op_cvt_alu_cvt_truncate() const;
	NvU8 * x1_op_cvt_alu_cvt_enable() const;
	NvS32 * x1_op_cvt_alu_cvt_offset() const;

	NvS16 * x1_op_cvt_mul_cvt_scale() const;
	NvU8 * x1_op_cvt_mul_cvt_truncate() const;
	NvU8 * x1_op_cvt_mul_cvt_enable() const;
	NvS32 * x1_op_cvt_mul_cvt_offset() const;

	NvU8 * has_relu() const;

    EMUConvOpDescAccessor(NvU8 *base, const EMUConvOpDesc &);

protected:
    NvU8 *_base;
    const EMUConvOpDesc &_n;
};


//
// union emu_operation_container
//
class EMUOperationContainer
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual EMUPowerOpDescAccessor powerOpDescAccessor(NvU8 *base, size_t c) const = 0;
    virtual EMUSoftmaxOpDescAccessor softmaxOpDescAccessor(NvU8 *base, size_t c) const = 0;
    virtual EMUConvOpDescAccessor convOpDescAccessor(NvU8 *base, size_t c) const = 0;

protected:
    EMUOperationContainer()          { }
    virtual ~EMUOperationContainer() { }
};

class EMUOperationContainerAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    EMUPowerOpDescAccessor powerOpDescAccessor(size_t c) const;
    EMUSoftmaxOpDescAccessor softmaxOpDescAccessor(size_t c) const;
    EMUConvOpDescAccessor convOpDescAccessor(size_t c) const;

    EMUOperationContainerAccessor(NvU8 *base, const EMUOperationContainer &);

protected:
    NvU8 *_base;
    const EMUOperationContainer &_n;
};


//
// struct emu_buffer_desc
//
class EMUBufferDesc
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual NvS16 * addressIndex(NvU8 *base)   const = 0;
    virtual NvU32 * addressIndexOffset(NvU8 *base)   const = 0;
    virtual NvU32 * size(NvU8 *base)       const = 0;
    virtual NvU16 * format(NvU8 *base)     const = 0;
    virtual NvU16   format_FF16()          const = 0;
    virtual NvU16   format_INT8()          const = 0;
    virtual NvU16   format_INT8_8()        const = 0;
    virtual NvU16   format_UINT8()         const = 0;
    virtual NvU16   format_INT16()         const = 0;
    virtual NvU16   format_UINT16()        const = 0;
    virtual NvU16 * width(NvU8 *base)      const = 0;
    virtual NvU16 * height(NvU8 *base)     const = 0;
    virtual NvU16 * channel(NvU8 *base)    const = 0;
    virtual NvU32 * lineStride(NvU8 *base) const = 0;
    virtual NvU32 * surfStride(NvU8 *base) const = 0;
    virtual NvU32 * planeStride(NvU8 *base) const = 0;

protected:
    EMUBufferDesc()          { }
    virtual ~EMUBufferDesc() { }
};

class EMUBufferDescAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    NvS16 * addressIndex()  const;
    NvU32 * addressIndexOffset() const;
    NvU32 * size()          const;
    NvU16 * format()        const;
    NvU16   format_FF16()   const;
    NvU16   format_INT8()   const;
    NvU16   format_INT8_8() const;
    NvU16   format_UINT8()  const;
    NvU16   format_INT16()  const;
    NvU16   format_UINT16() const;
    NvU16 * width()      const;
    NvU16 * height()     const;
    NvU16 * channel()    const;
    NvU32 * lineStride() const;
    NvU32 * surfStride() const;
    NvU32 * planeStride() const;

    EMUBufferDescAccessor(NvU8 *base, const EMUBufferDesc &);

protected:
    NvU8 *_base;
    const EMUBufferDesc &_n;
};


//
// struct emu_power_buffer_descs
//
class EMUPowerBufferDescs
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual EMUBufferDescAccessor srcDataAccessor(NvU8 *base) const = 0;
    virtual EMUBufferDescAccessor dstDataAccessor(NvU8 *base) const = 0;

protected:
    EMUPowerBufferDescs()          { }
    virtual ~EMUPowerBufferDescs() { }
};

class EMUPowerBufferDescsAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    EMUBufferDescAccessor srcDataAccessor() const;
    EMUBufferDescAccessor dstDataAccessor() const;

    EMUPowerBufferDescsAccessor(NvU8 *base, const EMUPowerBufferDescs &);

protected:
    NvU8 *_base;
    const EMUPowerBufferDescs &_n;
};


//
// struct emu_softmax_buffer_descs
//
class EMUSoftmaxBufferDescs
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual EMUBufferDescAccessor srcDataAccessor(NvU8 *base) const = 0;
    virtual EMUBufferDescAccessor dstDataAccessor(NvU8 *base) const = 0;

protected:
    EMUSoftmaxBufferDescs()          { }
    virtual ~EMUSoftmaxBufferDescs() { }
};

class EMUSoftmaxBufferDescsAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    EMUBufferDescAccessor srcDataAccessor() const;
    EMUBufferDescAccessor dstDataAccessor() const;

    EMUSoftmaxBufferDescsAccessor(NvU8 *base, const EMUSoftmaxBufferDescs &);

protected:
    NvU8 *_base;
    const EMUSoftmaxBufferDescs &_n;
};


//
// struct emu_conv_buffer_descs
//
class EMUConvBufferDescs
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual EMUBufferDescAccessor weightDataAccessor(NvU8 *base) const = 0;
    virtual EMUBufferDescAccessor wmbDataAccessor(NvU8 *base) const = 0;
    virtual EMUBufferDescAccessor wgsDataAccessor(NvU8 *base) const = 0;
    virtual EMUBufferDescAccessor biasDataAccessor(NvU8 *base) const = 0;
    virtual EMUBufferDescAccessor srcDataAccessor(NvU8 *base) const = 0;
    virtual EMUBufferDescAccessor dstDataAccessor(NvU8 *base) const = 0;

protected:
    EMUConvBufferDescs()          { }
    virtual ~EMUConvBufferDescs() { }
};

class EMUConvBufferDescsAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    EMUBufferDescAccessor weightDataAccessor() const;
    EMUBufferDescAccessor wmbDataAccessor() const;
    EMUBufferDescAccessor wgsDataAccessor() const;
    EMUBufferDescAccessor biasDataAccessor() const;
    EMUBufferDescAccessor srcDataAccessor() const;
    EMUBufferDescAccessor dstDataAccessor() const;

    EMUConvBufferDescsAccessor(NvU8 *base, const EMUConvBufferDescs &);

protected:
    NvU8 *_base;
    const EMUConvBufferDescs &_n;
};


//
// union emu_operation_buffer_container
//
class EMUOperationBufferContainer
{
public:
    virtual size_t struct_size()  const = 0;
    virtual size_t struct_align() const = 0;

    virtual EMUPowerBufferDescsAccessor powerBufferDescsAccessor(NvU8 *base, size_t c) const = 0;
    virtual EMUSoftmaxBufferDescsAccessor softmaxBufferDescsAccessor(NvU8 *base, size_t c) const = 0;
    virtual EMUConvBufferDescsAccessor convBufferDescsAccessor(NvU8 *base, size_t c) const = 0;

protected:
    EMUOperationBufferContainer()          { }
    virtual ~EMUOperationBufferContainer() { }
};

class EMUOperationBufferContainerAccessor
{
public:
    NvU8 * struct_base()  const;
    size_t struct_size()  const;
    size_t struct_align() const;

    EMUPowerBufferDescsAccessor powerBufferDescsAccessor(size_t c) const;
    EMUSoftmaxBufferDescsAccessor softmaxBufferDescsAccessor(size_t c) const;
    EMUConvBufferDescsAccessor convBufferDescsAccessor(size_t c) const;

    EMUOperationBufferContainerAccessor(NvU8 *base, const EMUOperationBufferContainer &);

protected:
    NvU8 *_base;
    const EMUOperationBufferContainer &_n;
};


class EMUInterface
{
public:
    virtual ~EMUInterface() { }

    // these are the targeted versions
    virtual NvU8 emulatorTargetVersionMajor()    const = 0;
    virtual NvU8 emulatorTargetVersionMinor()    const = 0;
    virtual NvU8 emulatorTargetVersionSubminor() const = 0;
    virtual NvU32 emulatorTargetVersion()         const = 0;

    virtual const std::string emulatorTargetGerritChange() const = 0;
    virtual const std::string emulatorTargetGerritReview() const = 0;

    // this is what was found to be running
    virtual NvU8 emulatorVersionMajor()    const = 0;
    virtual NvU8 emulatorVersionMinor()    const = 0;
    virtual NvU8 emulatorVersionSubminor() const = 0;
    virtual NvU32 emulatorVersion()         const = 0;

    EMUTaskDescAccessor      taskDescAccessor(NvU8 *base)  const;
    EMUNetworkDescAccessor   networkDescAccessor(NvU8 *base)  const;
    EMUOperationContainerAccessor operationContainerAccessor(NvU8 *base)  const;
    EMUBufferDescAccessor bufferDescAccessor(NvU8 *base)  const;
    EMUOperationBufferContainerAccessor operationBufferContainerAccessor(NvU8 *base)  const;

protected:
    virtual const EMUTaskDesc     & taskDesc()  const = 0;
    virtual const EMUNetworkDesc  & networkDesc()  const = 0;
    virtual const EMUOperationContainer & operationContainer()  const = 0;
    virtual const EMUBufferDesc & bufferDesc()  const = 0;
    virtual const EMUOperationBufferContainer & operationBufferContainer()  const = 0;
};


class EMUInterfaceA : public EMUInterface
{
public:
    virtual ~EMUInterfaceA() { }

    // these are the targeted versions
    NvU8 emulatorTargetVersionMajor()    const;
    NvU8 emulatorTargetVersionMinor()    const;
    NvU8 emulatorTargetVersionSubminor() const;
    NvU32 emulatorTargetVersion()         const;

    const std::string emulatorTargetGerritChange() const;
    const std::string emulatorTargetGerritReview() const;

    // this is what was found to be running
    NvU8 emulatorVersionMajor()    const;
    NvU8 emulatorVersionMinor()    const;
    NvU8 emulatorVersionSubminor() const;
    NvU32 emulatorVersion()         const;

protected:
    const EMUTaskDesc     & taskDesc()  const;
    const EMUNetworkDesc  & networkDesc()  const;
    const EMUOperationContainer & operationContainer()  const;
    const EMUBufferDesc & bufferDesc()  const;
    const EMUOperationBufferContainer & operationBufferContainer()  const;
    const EMUAddress & address()  const;
};


} // nvdla::priv
} // nvdla

#endif
