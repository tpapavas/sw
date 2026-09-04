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

#ifndef NVDLA_PRIV_EMU_EMU1_A_EMU_INTERFACE_H
#define NVDLA_PRIV_EMU_EMU1_A_EMU_INTERFACE_H

#define NVDLA_EMU_MAX_BUFFERS_PER_TASK (6144)

/**
 * @name Op Type
 * Network is formed using a list of these operations
 * @{
 */
#define NVDLA_EMU_OP_POWER    0
#define NVDLA_EMU_OP_SOFTMAX  1
#define NVDLA_EMU_OP_CONV     2
#define NVDLA_EMU_OP_POOL     3
#define NVDLA_EMU_OP_SDP      4
#define NVDLA_EMU_OP_RUBIK    5
/** @} */

struct emu_cvt_param {
	int16_t  scale;
	uint8_t  truncate;
	uint8_t  enable;

	int32_t  offset;
} __attribute__((packed)) __attribute__((aligned(4)));

struct emu_sdp_cvt {
	struct emu_cvt_param alu_cvt;
	struct emu_cvt_param mul_cvt;
} __attribute__((packed)) __attribute__((aligned(4)));

struct emu_sdp_op {
	uint8_t enable;
	uint8_t alu_type; /* dla_sdp_alu_op_type */
	uint8_t type; /* dla_sdp_op_type */
	uint8_t mode; /* dla_sdp_op_mode */

	uint8_t act; /* dla_act_type */
	uint8_t shift_value; /* left shift */
	uint8_t truncate;
	uint8_t precision;

	int32_t alu_operand;
	int32_t mul_operand;

	struct emu_sdp_cvt  cvt;
} __attribute__((packed)) __attribute__((aligned(4)));

/**
 * Address
 */
struct emu_address
{
    void *hMem;
    NvU32 offset;
};

/**
 * Task Descriptor
 */
struct emu_task_desc
{
    NvU32 num_addresses;
    emu_address address_list[NVDLA_EMU_MAX_BUFFERS_PER_TASK];
} __attribute__ ((packed, aligned(256)));

/**
 * Network Descriptor
 *
 * Contains all information to execute a network
 *
 * @num_operations: Number of operations in the lists
 */
struct emu_network_desc
{
    NvS16 operation_desc_index;
    NvS16 operation_buffer_desc_index;
    NvU16 num_operations;
} __attribute__ ((packed, aligned(256)));

struct emu_common_op_desc
{
    NvU8 op_type;
    NvF32 input_scale_factor;
    NvF32 output_scale_factor;
};

struct emu_power_op_desc
{
    emu_common_op_desc common;
    NvF32 power;
    NvF32 scale;
    NvF32 shift;
} __attribute__ ((packed, aligned(4)));

struct emu_softmax_op_desc
{
    emu_common_op_desc common;
    NvU8 axis;
} __attribute__ ((packed, aligned(4)));

struct emu_conv_op_desc
{
    emu_common_op_desc common;
	/* Performance parameters */

	/* dla_conv_mode */
	uint8_t conv_mode;
	uint8_t data_reuse;
	uint8_t weight_reuse;
	uint8_t skip_data_rls;

	uint8_t skip_weight_rls;
	uint8_t reserved0;
	uint16_t entry_per_slice;

	/* dla_data_format */
	uint8_t data_format;
	/* dla_pixel_mapping */
	uint8_t pixel_mapping;
	/* number of free slices before fetch */
	uint16_t fetch_grain;

	uint8_t reserved_b[8];

	/* batch_num */
	uint8_t batch;
	/* dla_weight_format */
	uint8_t weight_format;
	uint8_t data_bank;
	uint8_t weight_bank;

	/* the offset in bytes of each data cube in a batch */
	uint32_t batch_stride;

	uint8_t post_extension;
	uint8_t pixel_override;
	/* number of slices need to be released */
	uint16_t release;

	 /* The input cube dimension for CSC */
	uint16_t input_width_csc;
	uint16_t input_height_csc;

	uint16_t input_channel_csc;
	uint16_t kernel_width_csc;

	uint16_t kernel_height_csc;
	uint16_t kernel_channel_csc;

	/* The input cube dimension for CMAC */
	uint16_t input_width_cmac;
	uint16_t input_height_cmac;

	/* actual size in bytes */
	uint32_t bytes_per_kernel;

	/* Algorithm parameters */

	int16_t mean_ry; /* mean value for red in RGB or Y in YUV */
	int16_t mean_gu; /* mean value for green in RGB or U in YUV */

	int16_t mean_bv; /* mean value for blue in RGB or V in YUV */
	int16_t mean_ax;

	uint8_t mean_format; /* dla_mean_format */
	uint8_t conv_stride_x;
	uint8_t conv_stride_y;
	uint8_t pad_x_left;

	uint8_t pad_x_right;
	uint8_t pad_y_top;
	uint8_t pad_y_bottom;
	uint8_t dilation_x;

	uint8_t dilation_y;
	uint8_t reserved2[2];

	/* Precision parameters */
	uint8_t pra_truncate;

	uint8_t in_precision;
	/* The output precision from CONV, it's the MAC processing precison */
	uint8_t out_precision;
	int16_t pad_val;

	/* input converter parameters */
	// struct dla_cvt_param in_cvt;
	int16_t in_cvt_scale;
	uint8_t in_cvt_truncate;
	uint8_t in_cvt_enable;
	int32_t in_cvt_offset;
	/* output converter parameters, support truncate only */
	// struct dla_cvt_param out_cvt;
	int16_t out_cvt_scale;
	uint8_t out_cvt_truncate;
	uint8_t out_cvt_enable;
	int32_t out_cvt_offset;


	//////////////////////
	/* BIAS paramteters */
	//////////////////////
	/* dla_precision */
	uint8_t src_precision;
	uint8_t dst_precision;
	int16_t lut_index;

	// struct dla_cvt_param out_cvt;

	/* Performance parameters */
	/* dla_conv_mode */
	// uint8_t conv_mode;
	uint8_t batch_num;
	// uint16_t reserved0;

	// uint32_t batch_stride;	/* will be used when batch_num > 1 */
	// int16_t out_cvt_scale;
	// uint8_t out_cvt_truncate;
	// uint8_t out_cvt_enable;
	// int32_t out_cvt_offset;

	// x1 params
	uint8_t x1_op_enable;
	uint8_t x1_op_alu_type;
	uint8_t x1_op_type;
	uint8_t x1_op_mode;

	uint8_t x1_op_act;
	uint8_t x1_op_shift_value;
	uint8_t x1_op_truncate;
	uint8_t x1_op_precision;

	int32_t x1_op_alu_operand;
	int32_t x1_op_mul_operand;

	// struct dla_sdp_cvt x1_op_cvt;
	int16_t x1_op_cvt_alu_cvt_scale;
	uint8_t x1_op_cvt_alu_cvt_truncate;
	uint8_t x1_op_cvt_alu_cvt_enable; 
	int32_t x1_op_cvt_alu_cvt_offset;

	int16_t x1_op_cvt_mul_cvt_scale;
	uint8_t x1_op_cvt_mul_cvt_truncate;
	uint8_t x1_op_cvt_mul_cvt_enable; 
	int32_t x1_op_cvt_mul_cvt_offset;

	uint8_t has_relu;
} __attribute__ ((packed, aligned(4)));

struct emu_pool_op_desc
{
    emu_common_op_desc common;
	
	/* Performance parameters */
	uint16_t  partial_in_width_first;
	uint16_t  partial_in_width_mid;

	uint16_t  partial_in_width_last;
	uint16_t  partial_width_first;

	uint16_t  partial_width_mid;
	uint16_t  partial_width_last;

	uint8_t   split_num;

	/* Algorithm parameters */
	uint8_t  pool_mode; /* dla_pool_mode */
	uint8_t  pool_width; /* dla_pool_width */
	uint8_t  pool_height; /* dla_pool_height */

	uint8_t  stride_x;
	uint8_t  stride_y;

	/**
	 * The left/right padding size,
	 * pad_right might be less than pad_left
	 */
	uint8_t  pad_left;
	uint8_t  pad_right;

	/* The top/bottom padding size */
	uint8_t  pad_top;
	uint8_t  pad_bottom;

	/* Precision parameters */
	uint8_t  precision; /* dla_precision */
	uint8_t  reserved0;
	/**
	 * if input has non-zero "offset", this value should be set
	 * There'll be 7 different paddding values, the relationship between
	 * those versions are:
	 * padding_value[0] = -offset*scaling;
	 * padding_value[1] = 2*padding_value[0]
	 * padding_value[2] = 3*padding_value[0]
	 * ...
	 * The purpose is to avoid ucode implement FP16
	 * multiplier(for FP16 mode)
	 */
	int32_t  padding_value[7];
} __attribute__ ((packed, aligned(4)));

struct emu_sdp_op_desc {
    emu_common_op_desc common;

	/* Precision parameters */
	/* dla_precision */
	uint8_t src_precision;
	uint8_t dst_precision;
	int16_t lut_index;

	struct emu_cvt_param out_cvt;
	// int16_t out_cvt_scale;
	// uint8_t out_cvt_truncate;
	// uint8_t out_cvt_enable;
	// int32_t out_cvt_offset;

	/* Performance parameters */
	/* dla_conv_mode */
	uint8_t conv_mode;
	uint8_t batch_num;
	uint16_t reserved0;

	uint32_t batch_stride;	/* will be used when batch_num > 1 */

	/* Algorithm parameters */
	struct emu_sdp_op x1_op;
	struct emu_sdp_op x2_op;
	struct emu_sdp_op y_op;
} __attribute__((packed)) __attribute__((aligned(4)));

struct emu_rubik_op_desc {
    emu_common_op_desc common;

	/* Precision parameters */
	uint8_t mode;
	uint8_t precision;
	uint8_t stride_x;
	uint8_t stride_y;
} __attribute__((packed)) __attribute__((aligned(4)));

union emu_operation_container
{
    struct emu_power_op_desc power_op;
    struct emu_softmax_op_desc softmax_op;
    struct emu_conv_op_desc conv_op;
    struct emu_pool_op_desc pool_op;
	struct emu_sdp_op_desc sdp_op;
	struct emu_rubik_op_desc rubik_op;
};

struct emu_buffer_desc
{
    /* offset to the actual IOVA in task.address_list */
    NvS16 addressIndex;
    NvU32 addressIndexOffset;
    NvU32 size;

    /* surface format */
    NvU16 format;

    /* cube dimensions */
    NvU16 width;
    NvU16 height;
    NvU16 channel;

    /* stride information */
    NvU32 line_stride;
    NvU32 surf_stride;

    // from dla_data_cube
	// /* For Rubik only */
	uint32_t plane_stride;
} __attribute__ ((packed, aligned(256)));

struct emu_power_buffer_descs
{
    /* Buffer Descriptors */
    struct emu_buffer_desc src_data;
    struct emu_buffer_desc dst_data;
} __attribute__ ((packed, aligned(4)));

struct emu_softmax_buffer_descs
{
    /* Buffer Descriptors */
    struct emu_buffer_desc src_data;
    struct emu_buffer_desc dst_data;
} __attribute__ ((packed, aligned(4)));

struct emu_conv_buffer_descs
{
    /* Buffer Descriptors */
    struct emu_buffer_desc weight_data;
    struct emu_buffer_desc wmb_data;
    struct emu_buffer_desc wgs_data;
	struct emu_buffer_desc bias_data;
    struct emu_buffer_desc src_data;
    struct emu_buffer_desc dst_data;

    // these were in dla_conv_surface_desc
    // /**
	//  * u_addr = input_data.source_addr + offset_u
	//  * this field should be set when YUV is not interleave format
	//  *
	//  */
	// int64_t offset_u;

	// /* line stride for 2nd plane, must be 32bytes aligned */
	// uint32_t in_line_uv_stride;
} __attribute__ ((packed, aligned(4)));

struct emu_pool_buffer_descs
{
    /* Avg Pool Buffer Descriptors */
    struct emu_buffer_desc src_data;
    struct emu_buffer_desc dst_data;
} __attribute__ ((packed, aligned(4)));

struct emu_sdp_buffer_descs
{
    /* Avg Pool Buffer Descriptors */
    struct emu_buffer_desc src_data;
    struct emu_buffer_desc x1_data;
    struct emu_buffer_desc x2_data;
    struct emu_buffer_desc y_data;
    struct emu_buffer_desc dst_data;
} __attribute__ ((packed, aligned(4)));

struct emu_rubik_buffer_descs {
	/* Data cube */
	struct emu_buffer_desc src_data;
	struct emu_buffer_desc dst_data;
} __attribute__((packed)) __attribute__((aligned(4)));

union emu_operation_buffer_container
{
    struct emu_power_buffer_descs power_buffers;
    struct emu_softmax_buffer_descs softmax_buffers;
    struct emu_conv_buffer_descs  conv_buffers;
    struct emu_pool_buffer_descs  pool_buffers;
    struct emu_sdp_buffer_descs  sdp_buffers;
    struct emu_rubik_buffer_descs  rubik_buffers;
};


#endif // NVDLA_PRIV_EMU_EMU1_A_EMU_INTERFACE_H
