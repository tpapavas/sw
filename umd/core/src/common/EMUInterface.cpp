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

#include <stdbool.h>
#include "dlaerror.h"
#include "dlatypes.h"

#include "priv/EMUInterface.h"

namespace nvdla
{
namespace priv
{

//
// emu_address
//
EMUAddressAccessor::EMUAddressAccessor(NvU8 *base, const EMUAddress &n) : _base(base), _n(n) { }

NvU8 * EMUAddressAccessor::struct_base()  const { return _base; }
size_t EMUAddressAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUAddressAccessor::struct_align() const { return _n.struct_align(); }

void * EMUAddressAccessor::hMem()  const { return _n.hMem(_base); }
NvU32 * EMUAddressAccessor::offset()  const { return _n.offset(_base); }


//
// emu_task_desc
//
EMUTaskDescAccessor::EMUTaskDescAccessor(NvU8 *base, const EMUTaskDesc &n) : _base(base), _n(n) { }

NvU8 * EMUTaskDescAccessor::struct_base()  const { return _base; }
size_t EMUTaskDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUTaskDescAccessor::struct_align() const { return _n.struct_align(); }

NvU32 * EMUTaskDescAccessor::numAddresses()  const { return _n.numAddresses(_base); }
size_t EMUTaskDescAccessor::maxBuffersPerTask() const { return _n.maxBuffersPerTask(); }
EMUAddressAccessor EMUTaskDescAccessor::addressList(size_t c) const { return _n.addressList(_base, c); }


//
// emu_network_desc
//
EMUNetworkDescAccessor::EMUNetworkDescAccessor(NvU8 *base, const EMUNetworkDesc &n) : _base(base), _n(n) { }

NvU8 * EMUNetworkDescAccessor::struct_base()  const { return _base;      }
size_t EMUNetworkDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUNetworkDescAccessor::struct_align() const { return _n.struct_align(); }

int16_t  * EMUNetworkDescAccessor::operationDescIndex()   const { return _n.operationDescIndex(_base); }
int16_t  * EMUNetworkDescAccessor::operationBufferDescIndex()     const { return _n.operationBufferDescIndex(_base); }
uint16_t * EMUNetworkDescAccessor::numOperations()        const { return _n.numOperations(_base); }


//
// emu_common_op_desc
//
EMUCommonOpDescAccessor::EMUCommonOpDescAccessor(NvU8 *base, const EMUCommonOpDesc &n) : _base(base), _n(n) { }

NvU8 * EMUCommonOpDescAccessor::struct_base()  const { return _base;      }
size_t EMUCommonOpDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUCommonOpDescAccessor::struct_align() const { return _n.struct_align(); }

NvU8 * EMUCommonOpDescAccessor::op_type()   const { return _n.op_type(_base); }
NvF32 * EMUCommonOpDescAccessor::input_scale_factor() const { return _n.input_scale_factor(_base); }
NvF32 * EMUCommonOpDescAccessor::output_scale_factor() const { return _n.output_scale_factor(_base); }

//
// emu_power_op_desc
//
EMUPowerOpDescAccessor::EMUPowerOpDescAccessor(NvU8 *base, const EMUPowerOpDesc &n) : _base(base), _n(n) { }

NvU8 * EMUPowerOpDescAccessor::struct_base()  const { return _base;      }
size_t EMUPowerOpDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUPowerOpDescAccessor::struct_align() const { return _n.struct_align(); }

EMUCommonOpDescAccessor EMUPowerOpDescAccessor::commonOpDescAccessor() const { return _n.commonOpDescAccessor(_base); }
NvF32 * EMUPowerOpDescAccessor::power()   const { return _n.power(_base); }
NvF32 * EMUPowerOpDescAccessor::scale()   const { return _n.scale(_base); }
NvF32 * EMUPowerOpDescAccessor::shift()   const { return _n.shift(_base); }


//
// emu_softmax_op_desc
//
EMUSoftmaxOpDescAccessor::EMUSoftmaxOpDescAccessor(NvU8 *base, const EMUSoftmaxOpDesc &n) : _base(base), _n(n) { }

NvU8 * EMUSoftmaxOpDescAccessor::struct_base()  const { return _base;      }
size_t EMUSoftmaxOpDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUSoftmaxOpDescAccessor::struct_align() const { return _n.struct_align(); }

EMUCommonOpDescAccessor EMUSoftmaxOpDescAccessor::commonOpDescAccessor() const { return _n.commonOpDescAccessor(_base); }
NvU8 * EMUSoftmaxOpDescAccessor::axis()   const { return _n.axis(_base); }

//
// emu_conv_op_desc
//
EMUConvOpDescAccessor::EMUConvOpDescAccessor(NvU8 *base, const EMUConvOpDesc &n) : _base(base), _n(n) { }

NvU8 * EMUConvOpDescAccessor::struct_base()  const { return _base;      }
size_t EMUConvOpDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUConvOpDescAccessor::struct_align() const { return _n.struct_align(); }

EMUCommonOpDescAccessor EMUConvOpDescAccessor::commonOpDescAccessor() const { return _n.commonOpDescAccessor(_base); }

NvU8 * EMUConvOpDescAccessor::conv_mode() const { return _n.conv_mode(_base); }
NvU8 * EMUConvOpDescAccessor::data_reuse() const { return _n.data_reuse(_base); }
NvU8 * EMUConvOpDescAccessor::weight_reuse() const { return _n.weight_reuse(_base); }
NvU8 * EMUConvOpDescAccessor::skip_data_rls() const { return _n.skip_data_rls(_base); }

NvU8 * EMUConvOpDescAccessor::skip_weight_rls() const { return _n.skip_weight_rls(_base); }
NvU8 * EMUConvOpDescAccessor::reserved0() const { return _n.reserved0(_base); }
NvU16 * EMUConvOpDescAccessor::entry_per_slice() const { return _n.entry_per_slice(_base); }

NvU8 * EMUConvOpDescAccessor::data_format() const { return _n.data_format(_base); }
NvU8 * EMUConvOpDescAccessor::pixel_mapping() const { return _n.pixel_mapping(_base); }
NvU16 * EMUConvOpDescAccessor::fetch_grain() const { return _n.fetch_grain(_base); }
NvU8 * EMUConvOpDescAccessor::reserved_b() const { return _n.reserved_b(_base); }
NvU8 * EMUConvOpDescAccessor::batch() const { return _n.batch(_base); }
NvU8 * EMUConvOpDescAccessor::weight_format() const { return _n.weight_format(_base); }
NvU8 * EMUConvOpDescAccessor::data_bank() const { return _n.data_bank(_base); }
NvU8 * EMUConvOpDescAccessor::weight_bank() const { return _n.weight_bank(_base); }
NvU32 * EMUConvOpDescAccessor::batch_stride() const { return _n.batch_stride(_base); }
NvU8 * EMUConvOpDescAccessor::post_extension() const { return _n.post_extension(_base); }
NvU8 * EMUConvOpDescAccessor::pixel_override() const { return _n.pixel_override(_base); }
NvU16 * EMUConvOpDescAccessor::release() const { return _n.release(_base); }

NvU16 * EMUConvOpDescAccessor::input_width_csc() const { return _n.input_width_csc(_base); }
NvU16 * EMUConvOpDescAccessor::input_height_csc() const { return _n.input_height_csc(_base); }
NvU16 * EMUConvOpDescAccessor::input_channel_csc() const { return _n.input_channel_csc(_base); }
NvU16 * EMUConvOpDescAccessor::kernel_width_csc() const { return _n.kernel_width_csc(_base); }
NvU16 * EMUConvOpDescAccessor::kernel_height_csc() const { return _n.kernel_height_csc(_base); }
NvU16 * EMUConvOpDescAccessor::kernel_channel_csc() const { return _n.kernel_channel_csc(_base); }
NvU16 * EMUConvOpDescAccessor::input_width_cmac() const { return _n.input_width_cmac(_base); }
NvU16 * EMUConvOpDescAccessor::input_height_cmac() const { return _n.input_height_cmac(_base); }
NvU32 * EMUConvOpDescAccessor::bytes_per_kernel() const { return _n.bytes_per_kernel(_base); }

/* Algorithm parameters */
NvS16 * EMUConvOpDescAccessor::mean_ry() const { return _n.mean_ry(_base); } /* mean value for red in RGB or Y in YUV */
NvS16 * EMUConvOpDescAccessor::mean_gu() const { return _n.mean_gu(_base); } /* mean value for green in RGB or U in YUV */
NvS16 * EMUConvOpDescAccessor::mean_bv() const { return _n.mean_bv(_base); } /* mean value for blue in RGB or V in YUV */
NvS16 * EMUConvOpDescAccessor::mean_ax() const { return _n.mean_ax(_base); }
NvU8 * EMUConvOpDescAccessor::mean_format() const { return _n.mean_format(_base); } /* dla_mean_format */
NvU8 * EMUConvOpDescAccessor::conv_stride_x() const { return _n.conv_stride_x(_base); }
NvU8 * EMUConvOpDescAccessor::conv_stride_y() const { return _n.conv_stride_y(_base); }
NvU8 * EMUConvOpDescAccessor::pad_x_left() const { return _n.pad_x_left(_base); }
NvU8 * EMUConvOpDescAccessor::pad_x_right() const { return _n.pad_x_right(_base); }
NvU8 * EMUConvOpDescAccessor::pad_y_top() const { return _n.pad_y_top(_base); }
NvU8 * EMUConvOpDescAccessor::pad_y_bottom() const { return _n.pad_y_bottom(_base); }
NvU8 * EMUConvOpDescAccessor::dilation_x() const { return _n.dilation_x(_base); }
NvU8 * EMUConvOpDescAccessor::dilation_y() const { return _n.dilation_y(_base); }
NvU8 * EMUConvOpDescAccessor::reserved2() const { return _n.reserved2(_base); }

NvU8 * EMUConvOpDescAccessor::pra_truncate() const { return _n.pra_truncate(_base); }
NvU8 * EMUConvOpDescAccessor::in_precision() const { return _n.in_precision(_base); }
NvU8 * EMUConvOpDescAccessor::out_precision() const { return _n.out_precision(_base); }
NvS16 * EMUConvOpDescAccessor::pad_val() const { return _n.pad_val(_base); }

NvS16 * EMUConvOpDescAccessor::in_cvt_scale() const { return _n.in_cvt_scale(_base); }
NvU8 * EMUConvOpDescAccessor::in_cvt_truncate() const { return _n.in_cvt_truncate(_base); }
NvU8 * EMUConvOpDescAccessor::in_cvt_enable() const { return _n.in_cvt_enable(_base); }
NvS32 * EMUConvOpDescAccessor::in_cvt_offset() const { return _n.in_cvt_offset(_base); }
NvS16 * EMUConvOpDescAccessor::out_cvt_scale() const { return _n.out_cvt_scale(_base); }
NvU8 * EMUConvOpDescAccessor::out_cvt_truncate() const { return _n.out_cvt_truncate(_base); }
NvU8 * EMUConvOpDescAccessor::out_cvt_enable() const { return _n.out_cvt_enable(_base); }
NvS32 * EMUConvOpDescAccessor::out_cvt_offset() const { return _n.out_cvt_offset(_base); }

//////////////////////
/* BIAS paramteters */
//////////////////////
NvU8 * EMUConvOpDescAccessor::src_precision() const { return _n.src_precision(_base); }
NvU8 * EMUConvOpDescAccessor::dst_precision() const { return _n.dst_precision(_base); }
NvS16 * EMUConvOpDescAccessor::lut_index() const { return _n.lut_index(_base); }
NvU8 * EMUConvOpDescAccessor::batch_num() const { return _n.batch_num(_base); }

// x1 params
NvU8 * EMUConvOpDescAccessor::x1_op_enable() const { return _n.x1_op_enable(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_alu_type() const { return _n.x1_op_alu_type(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_type() const { return _n.x1_op_type(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_mode() const { return _n.x1_op_mode(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_act() const { return _n.x1_op_act(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_shift_value() const { return _n.x1_op_shift_value(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_truncate() const { return _n.x1_op_truncate(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_precision() const { return _n.x1_op_precision(_base); }
NvS32 * EMUConvOpDescAccessor::x1_op_alu_operand() const { return _n.x1_op_alu_operand(_base); }
NvS32 * EMUConvOpDescAccessor::x1_op_mul_operand() const { return _n.x1_op_mul_operand(_base); }
NvS16 * EMUConvOpDescAccessor::x1_op_cvt_alu_cvt_scale() const { return _n.x1_op_cvt_alu_cvt_scale(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_cvt_alu_cvt_truncate() const { return _n.x1_op_cvt_alu_cvt_truncate(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_cvt_alu_cvt_enable() const { return _n.x1_op_cvt_alu_cvt_enable(_base); }
NvS32 * EMUConvOpDescAccessor::x1_op_cvt_alu_cvt_offset() const { return _n.x1_op_cvt_alu_cvt_offset(_base); }
NvS16 * EMUConvOpDescAccessor::x1_op_cvt_mul_cvt_scale() const { return _n.x1_op_cvt_mul_cvt_scale(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_cvt_mul_cvt_truncate() const { return _n.x1_op_cvt_mul_cvt_truncate(_base); }
NvU8 * EMUConvOpDescAccessor::x1_op_cvt_mul_cvt_enable() const { return _n.x1_op_cvt_mul_cvt_enable(_base); }
NvS32 * EMUConvOpDescAccessor::x1_op_cvt_mul_cvt_offset() const { return _n.x1_op_cvt_mul_cvt_offset(_base); }
NvU8 * EMUConvOpDescAccessor::has_relu() const { return _n.has_relu(_base); }


//
// emu_pool_op_desc
//
EMUPoolOpDescAccessor::EMUPoolOpDescAccessor(NvU8 *base, const EMUPoolOpDesc &n) : _base(base), _n(n) { }

NvU8 * EMUPoolOpDescAccessor::struct_base()  const { return _base;      }
size_t EMUPoolOpDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUPoolOpDescAccessor::struct_align() const { return _n.struct_align(); }

EMUCommonOpDescAccessor EMUPoolOpDescAccessor::commonOpDescAccessor() const { return _n.commonOpDescAccessor(_base); }
NvU16 * EMUPoolOpDescAccessor::partial_in_width_first() const { return _n.partial_in_width_first(_base); }
NvU16 * EMUPoolOpDescAccessor::partial_in_width_mid() const { return _n.partial_in_width_mid(_base); }
NvU16 * EMUPoolOpDescAccessor::partial_in_width_last() const { return _n.partial_in_width_last(_base); }
NvU16 * EMUPoolOpDescAccessor::partial_width_first() const { return _n.partial_width_first(_base); }
NvU16 * EMUPoolOpDescAccessor::partial_width_mid() const { return _n.partial_width_mid(_base); }
NvU16 * EMUPoolOpDescAccessor::partial_width_last() const { return _n.partial_width_last(_base); }
NvU8 * EMUPoolOpDescAccessor::split_num() const { return _n.split_num(_base); }
NvU8 * EMUPoolOpDescAccessor::pool_mode() const { return _n.pool_mode(_base); }
NvU8 * EMUPoolOpDescAccessor::pool_width() const { return _n.pool_width(_base); }
NvU8 * EMUPoolOpDescAccessor::pool_height() const { return _n.pool_height(_base); }
NvU8 * EMUPoolOpDescAccessor::stride_x() const { return _n.stride_x(_base); }
NvU8 * EMUPoolOpDescAccessor::stride_y() const { return _n.stride_y(_base); }
NvU8 * EMUPoolOpDescAccessor::pad_left() const { return _n.pad_left(_base); }
NvU8 * EMUPoolOpDescAccessor::pad_right() const { return _n.pad_right(_base); }
NvU8 * EMUPoolOpDescAccessor::pad_top() const { return _n.pad_top(_base); }
NvU8 * EMUPoolOpDescAccessor::pad_bottom() const { return _n.pad_bottom(_base); }
NvU8 * EMUPoolOpDescAccessor::precision() const { return _n.precision(_base); }
NvU8 * EMUPoolOpDescAccessor::reserved0() const { return _n.reserved0(_base); }
NvS32 * EMUPoolOpDescAccessor::padding_value() const { return _n.padding_value(_base); }

//
// emu_sdp_op_desc
//
EMUSdpOpDescAccessor::EMUSdpOpDescAccessor(NvU8 *base, const EMUSdpOpDesc &n) : _base(base), _n(n) { }

NvU8 * EMUSdpOpDescAccessor::struct_base()  const { return _base;      }
size_t EMUSdpOpDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUSdpOpDescAccessor::struct_align() const { return _n.struct_align(); }

EMUCommonOpDescAccessor EMUSdpOpDescAccessor::commonOpDescAccessor() const { return _n.commonOpDescAccessor(_base); }
uint8_t * EMUSdpOpDescAccessor::src_precision() const { return _n.src_precision(_base); }
uint8_t * EMUSdpOpDescAccessor::dst_precision() const { return _n.dst_precision(_base); }
int16_t * EMUSdpOpDescAccessor::lut_index() const { return _n.lut_index(_base); }
struct emu_cvt_param * EMUSdpOpDescAccessor::out_cvt() const { return _n.out_cvt(_base); }
uint8_t * EMUSdpOpDescAccessor::conv_mode() const { return _n.conv_mode(_base); }
uint8_t * EMUSdpOpDescAccessor::batch_num() const { return _n.batch_num(_base); }
uint16_t * EMUSdpOpDescAccessor::reserved0() const { return _n.reserved0(_base); }
uint32_t * EMUSdpOpDescAccessor::batch_stride() const { return _n.batch_stride(_base); }
struct emu_sdp_op * EMUSdpOpDescAccessor::x1_op() const { return _n.x1_op(_base); }
struct emu_sdp_op * EMUSdpOpDescAccessor::x2_op() const { return _n.x2_op(_base); }
struct emu_sdp_op * EMUSdpOpDescAccessor::y_op() const { return _n.y_op(_base); }


//
// emu_rubik_op_desc
//
EMURubikOpDescAccessor::EMURubikOpDescAccessor(NvU8 *base, const EMURubikOpDesc &n) : _base(base), _n(n) { }

NvU8 * EMURubikOpDescAccessor::struct_base()  const { return _base;      }
size_t EMURubikOpDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMURubikOpDescAccessor::struct_align() const { return _n.struct_align(); }

EMUCommonOpDescAccessor EMURubikOpDescAccessor::commonOpDescAccessor() const { return _n.commonOpDescAccessor(_base); }
uint8_t * EMURubikOpDescAccessor::mode() const { return _n.mode(_base); }
uint8_t * EMURubikOpDescAccessor::precision() const { return _n.precision(_base); }
uint8_t * EMURubikOpDescAccessor::stride_x() const { return _n.stride_x(_base); }
uint8_t * EMURubikOpDescAccessor::stride_y() const { return _n.stride_y(_base); }

//
// emu_operation_container
//
EMUOperationContainerAccessor::EMUOperationContainerAccessor(NvU8 *base, const EMUOperationContainer &n) : _base(base), _n(n) { }

NvU8 * EMUOperationContainerAccessor::struct_base()  const { return _base;      }
size_t EMUOperationContainerAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUOperationContainerAccessor::struct_align() const { return _n.struct_align(); }

EMUPowerOpDescAccessor EMUOperationContainerAccessor::powerOpDescAccessor(size_t c) const { return _n.powerOpDescAccessor(_base, c); }
EMUSoftmaxOpDescAccessor EMUOperationContainerAccessor::softmaxOpDescAccessor(size_t c) const { return _n.softmaxOpDescAccessor(_base, c); }
EMUConvOpDescAccessor EMUOperationContainerAccessor::convOpDescAccessor(size_t c) const { return _n.convOpDescAccessor(_base, c); }
EMUPoolOpDescAccessor EMUOperationContainerAccessor::poolOpDescAccessor(size_t c) const { return _n.poolOpDescAccessor(_base, c); }
EMUSdpOpDescAccessor EMUOperationContainerAccessor::sdpOpDescAccessor(size_t c) const { return _n.sdpOpDescAccessor(_base, c); }
EMURubikOpDescAccessor EMUOperationContainerAccessor::rubikOpDescAccessor(size_t c) const { return _n.rubikOpDescAccessor(_base, c); }


//
// emu_buffer_desc
//
EMUBufferDescAccessor::EMUBufferDescAccessor(NvU8 *base, const EMUBufferDesc &n) : _base(base), _n(n) { }

NvU8 * EMUBufferDescAccessor::struct_base()  const { return _base;      }
size_t EMUBufferDescAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUBufferDescAccessor::struct_align() const { return _n.struct_align(); }

NvS16 * EMUBufferDescAccessor::addressIndex()    const { return _n.addressIndex(_base); }
NvU32 * EMUBufferDescAccessor::addressIndexOffset()    const { return _n.addressIndexOffset(_base); }
NvU32 * EMUBufferDescAccessor::size()       const { return _n.size(_base); }
NvU16 * EMUBufferDescAccessor::format()     const { return _n.format(_base); }
NvU16   EMUBufferDescAccessor::format_FF16()    const { return _n.format_FF16(); }
NvU16   EMUBufferDescAccessor::format_INT8()    const { return _n.format_INT8(); }
NvU16   EMUBufferDescAccessor::format_INT8_8()  const { return _n.format_INT8_8(); }
NvU16   EMUBufferDescAccessor::format_UINT8()   const { return _n.format_UINT8(); }
NvU16   EMUBufferDescAccessor::format_INT16()   const { return _n.format_INT16(); }
NvU16   EMUBufferDescAccessor::format_UINT16()  const { return _n.format_UINT16(); }
NvU16 * EMUBufferDescAccessor::width()      const { return _n.width(_base); }
NvU16 * EMUBufferDescAccessor::height()     const { return _n.height(_base); }
NvU16 * EMUBufferDescAccessor::channel()    const { return _n.channel(_base); }
NvU32 * EMUBufferDescAccessor::lineStride() const { return _n.lineStride(_base); }
NvU32 * EMUBufferDescAccessor::surfStride() const { return _n.surfStride(_base); }
NvU32 * EMUBufferDescAccessor::planeStride() const { return _n.planeStride(_base); }


//
// emu_power_buffer_descs
//
EMUPowerBufferDescsAccessor::EMUPowerBufferDescsAccessor(NvU8 *base, const EMUPowerBufferDescs &n) : _base(base), _n(n) { }

NvU8 * EMUPowerBufferDescsAccessor::struct_base()  const { return _base;      }
size_t EMUPowerBufferDescsAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUPowerBufferDescsAccessor::struct_align() const { return _n.struct_align(); }

EMUBufferDescAccessor EMUPowerBufferDescsAccessor::srcDataAccessor() const { return _n.srcDataAccessor(_base); }
EMUBufferDescAccessor EMUPowerBufferDescsAccessor::dstDataAccessor() const { return _n.dstDataAccessor(_base); }


//
// emu_softmax_buffer_descs
//
EMUSoftmaxBufferDescsAccessor::EMUSoftmaxBufferDescsAccessor(NvU8 *base, const EMUSoftmaxBufferDescs &n) : _base(base), _n(n) { }

NvU8 * EMUSoftmaxBufferDescsAccessor::struct_base()  const { return _base;      }
size_t EMUSoftmaxBufferDescsAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUSoftmaxBufferDescsAccessor::struct_align() const { return _n.struct_align(); }

EMUBufferDescAccessor EMUSoftmaxBufferDescsAccessor::srcDataAccessor() const { return _n.srcDataAccessor(_base); }
EMUBufferDescAccessor EMUSoftmaxBufferDescsAccessor::dstDataAccessor() const { return _n.dstDataAccessor(_base); }


//
// emu_conv_buffer_descs
//
EMUConvBufferDescsAccessor::EMUConvBufferDescsAccessor(NvU8 *base, const EMUConvBufferDescs &n) : _base(base), _n(n) { }

NvU8 * EMUConvBufferDescsAccessor::struct_base()  const { return _base;      }
size_t EMUConvBufferDescsAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUConvBufferDescsAccessor::struct_align() const { return _n.struct_align(); }

EMUBufferDescAccessor EMUConvBufferDescsAccessor::weightDataAccessor() const { return _n.weightDataAccessor(_base); }
EMUBufferDescAccessor EMUConvBufferDescsAccessor::wmbDataAccessor() const { return _n.wmbDataAccessor(_base); }
EMUBufferDescAccessor EMUConvBufferDescsAccessor::wgsDataAccessor() const { return _n.wgsDataAccessor(_base); }
EMUBufferDescAccessor EMUConvBufferDescsAccessor::biasDataAccessor() const { return _n.biasDataAccessor(_base); }
EMUBufferDescAccessor EMUConvBufferDescsAccessor::srcDataAccessor() const { return _n.srcDataAccessor(_base); }
EMUBufferDescAccessor EMUConvBufferDescsAccessor::dstDataAccessor() const { return _n.dstDataAccessor(_base); }


//
// emu_pool_buffer_descs
//
EMUPoolBufferDescsAccessor::EMUPoolBufferDescsAccessor(NvU8 *base, const EMUPoolBufferDescs &n) : _base(base), _n(n) { }

NvU8 * EMUPoolBufferDescsAccessor::struct_base()  const { return _base;      }
size_t EMUPoolBufferDescsAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUPoolBufferDescsAccessor::struct_align() const { return _n.struct_align(); }

EMUBufferDescAccessor EMUPoolBufferDescsAccessor::srcDataAccessor() const { return _n.srcDataAccessor(_base); }
EMUBufferDescAccessor EMUPoolBufferDescsAccessor::dstDataAccessor() const { return _n.dstDataAccessor(_base); }


//
// emu_conv_buffer_descs
//
EMUSdpBufferDescsAccessor::EMUSdpBufferDescsAccessor(NvU8 *base, const EMUSdpBufferDescs &n) : _base(base), _n(n) { }

NvU8 * EMUSdpBufferDescsAccessor::struct_base()  const { return _base;      }
size_t EMUSdpBufferDescsAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUSdpBufferDescsAccessor::struct_align() const { return _n.struct_align(); }

EMUBufferDescAccessor EMUSdpBufferDescsAccessor::srcDataAccessor() const { return _n.srcDataAccessor(_base); }
EMUBufferDescAccessor EMUSdpBufferDescsAccessor::x1DataAccessor() const { return _n.x1DataAccessor(_base); }
EMUBufferDescAccessor EMUSdpBufferDescsAccessor::x2DataAccessor() const { return _n.x2DataAccessor(_base); }
EMUBufferDescAccessor EMUSdpBufferDescsAccessor::yDataAccessor() const { return _n.yDataAccessor(_base); }
EMUBufferDescAccessor EMUSdpBufferDescsAccessor::dstDataAccessor() const { return _n.dstDataAccessor(_base); }


//
// emu_rubik_buffer_descs
//
EMURubikBufferDescsAccessor::EMURubikBufferDescsAccessor(NvU8 *base, const EMURubikBufferDescs &n) : _base(base), _n(n) { }

NvU8 * EMURubikBufferDescsAccessor::struct_base()  const { return _base;      }
size_t EMURubikBufferDescsAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMURubikBufferDescsAccessor::struct_align() const { return _n.struct_align(); }

EMUBufferDescAccessor EMURubikBufferDescsAccessor::srcDataAccessor() const { return _n.srcDataAccessor(_base); }
EMUBufferDescAccessor EMURubikBufferDescsAccessor::dstDataAccessor() const { return _n.dstDataAccessor(_base); }

//
// emu_operation_buffer_container
//
EMUOperationBufferContainerAccessor::EMUOperationBufferContainerAccessor(NvU8 *base, const EMUOperationBufferContainer &n) : _base(base), _n(n) { }

NvU8 * EMUOperationBufferContainerAccessor::struct_base()  const { return _base;      }
size_t EMUOperationBufferContainerAccessor::struct_size()  const { return _n.struct_size();  }
size_t EMUOperationBufferContainerAccessor::struct_align() const { return _n.struct_align(); }

EMUPowerBufferDescsAccessor EMUOperationBufferContainerAccessor::powerBufferDescsAccessor(size_t c) const { return _n.powerBufferDescsAccessor(_base, c); }
EMUSoftmaxBufferDescsAccessor EMUOperationBufferContainerAccessor::softmaxBufferDescsAccessor(size_t c) const { return _n.softmaxBufferDescsAccessor(_base, c); }
EMUConvBufferDescsAccessor EMUOperationBufferContainerAccessor::convBufferDescsAccessor(size_t c) const { return _n.convBufferDescsAccessor(_base, c); }
EMUPoolBufferDescsAccessor EMUOperationBufferContainerAccessor::poolBufferDescsAccessor(size_t c) const { return _n.poolBufferDescsAccessor(_base, c); }
EMUSdpBufferDescsAccessor EMUOperationBufferContainerAccessor::sdpBufferDescsAccessor(size_t c) const { return _n.sdpBufferDescsAccessor(_base, c); }
EMURubikBufferDescsAccessor EMUOperationBufferContainerAccessor::rubikBufferDescsAccessor(size_t c) const { return _n.rubikBufferDescsAccessor(_base, c); }


//
// EMUInterface::
//
EMUTaskDescAccessor     EMUInterface::taskDescAccessor(NvU8 *base)     const { return EMUTaskDescAccessor(base, taskDesc()); }
EMUNetworkDescAccessor  EMUInterface::networkDescAccessor(NvU8 *base)  const { return EMUNetworkDescAccessor(base, networkDesc()); }
EMUOperationContainerAccessor EMUInterface::operationContainerAccessor(NvU8 *base) const { return EMUOperationContainerAccessor(base, operationContainer()); }
EMUBufferDescAccessor EMUInterface::bufferDescAccessor(NvU8 *base) const { return EMUBufferDescAccessor(base, bufferDesc()); }
EMUOperationBufferContainerAccessor   EMUInterface::operationBufferContainerAccessor(NvU8 *base)   const { return EMUOperationBufferContainerAccessor(base, operationBufferContainer()); }

} // nvdla::priv
} // nvdla
