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

//// KUMD ////
#ifndef KUMD
#define KUMD
#endif

#include <stdio.h>

#include <opendla.h>
#include <dla_debug.h>
#include "dla_engine.h"
#include <dla_err.h>
#include "dla_interface.h"

#include "dla_engine_internal.h"
#include "engine_debug.h"

#include "nvdla_ioctl.h"
#include "nvdla_linux.h"

#define MAX_NUM_ADDRESSES	256

static uint64_t roi_array_length __aligned(8);
static struct dla_network_desc network;

static inline uint8_t
get_op_batch_id(struct dla_common_op_desc* op_desc) {
	return op_desc->index % dla_get_engine()->num_batches;
}

/**
 * TODO: implement some logic
 */
static inline uint8_t
get_op_stage_id(struct dla_common_op_desc* op_desc) {
	uint8_t single_batch_op_index = op_desc->index / dla_get_engine()->num_batches;
	uint8_t num_stages = dla_get_engine()->num_stages;
	uint8_t* stage_limits = &dla_get_engine()->stage_limits;

	for (int i = 0; i < num_stages-1; i++) {
		if (single_batch_op_index < stage_limits[i]) {
			return i;
		}
	}

	return num_stages-1;
}

// [gem5-plus]
static void
update_job_map(struct dla_common_op_desc* op_desc, struct dla_processor* processor)
{
	uint8_t batch_id;
	uint8_t stage_id;
	struct dla_engine* engine;
	int8_t* current_job_dev;

	dla_debug("Enter: %s\n", __func__);

	batch_id = get_op_batch_id(op_desc);
	stage_id = get_op_stage_id(op_desc);
	engine = dla_get_engine();
	current_job_dev = &engine->job_map[batch_id][stage_id];

	/**
	 * TODO: custom assertion based on different scheduling algorithms
	 */
	assert(*current_job_dev == -1 || *current_job_dev == processor->dev_id);

	*current_job_dev = processor->dev_id;
	dla_debug("set job_map[%d][%d] = %d\n", batch_id, stage_id, engine->job_map[batch_id][stage_id]);

	dla_debug("Exit: %s\n", __func__);
}

static bool
check_custom_deps(struct dla_common_op_desc* op_desc, struct dla_processor* processor)
{
	uint8_t batch_id;
	uint8_t stage_id;
	struct dla_engine* engine;
	int8_t* current_job_dev;
	bool custom_deps_met;

	dla_debug("Enter: %s\n", __func__);

	batch_id = get_op_batch_id(op_desc);
	stage_id = get_op_stage_id(op_desc);
	engine = dla_get_engine();
	current_job_dev = &engine->job_map[batch_id][stage_id];

	custom_deps_met = false;

	dla_debug("current_job_dev: %d\n", *current_job_dev);
	if (*current_job_dev == -1) {
		/**
		 * TODO: custom condition based on different scheduling algorithms
		 */
		custom_deps_met = engine->can_schedule_op_on_dev(op_desc, batch_id, stage_id, processor->dev_id);
	} else if (*current_job_dev == processor->dev_id) {
		// current job has already been scheduled on this device
		custom_deps_met = true;
	} else if (*current_job_dev > 0) {
		// current job has already been scheduled on a different device
		custom_deps_met = false;
	}

	dla_debug("check custom deps (b_id: %d, s_id %d): %s\n", batch_id, stage_id, custom_deps_met ? "OK" : "NOT OK");

	dla_debug("Exit: %s\n", __func__);

	return custom_deps_met;
}

static int
dla_update_consumers(struct dla_processor_group *group,
			struct dla_common_op_desc *op, uint8_t event);

static int32_t
dla_read_address_list(struct dla_engine *engine)
{
	RETURN(0);
}

int32_t
dla_read_lut(struct dla_engine *engine, int16_t index, void *dst)
{
	int32_t ret = 0;
	uint64_t src_addr;

	struct dla_engine *u__engine = dla_get_u__engine();
	struct nvdla_task *u__task = (struct nvdla_task *) u__engine->task->task_data;
	struct dla_lut_param *u__luts = (struct dla_lut_param *) u__task->luts;
	struct dla_lut_param *u__lut = &u__luts[index];

	if (index == -1) {
		ret = ERR(INVALID_INPUT);
		goto exit;
	}

	src_addr = engine->task->lut_data_addr;

	ret = dla_data_read(engine->driver_context,
			engine->task->task_data,
			src_addr, (void *)dst,
			sizeof(struct dla_lut_param),
			(sizeof(struct dla_lut_param) * (uint64_t)index));
	
	// dst = (void *) u__lut;
	uint8_t *src = u__lut;
	uint8_t *my_dst = dst;
	for (int i_byte = 0; i_byte < sizeof(struct dla_lut_param); i_byte++) {
		my_dst[i_byte] = src[i_byte];
	}

exit:
	RETURN(ret);
}

static int
dla_op_enabled(struct dla_processor_group *group)
{
	int32_t ret;
	struct dla_common_op_desc *op_desc;

	dla_debug("Enter: %s\n", __func__);
	op_desc = group->op_desc;

	group->active = 1;

	/* update dependency graph for this task */
	ret = dla_update_consumers(group, op_desc, DLA_EVENT_OP_ENABLED);
	dla_debug("Exit: %s\n", __func__);

	RETURN(ret);
}

static int
dla_op_programmed(struct dla_processor *processor,
		  struct dla_processor_group *group,
		  uint8_t rdma_id)
{
	int32_t ret;
	struct dla_common_op_desc *op_desc;

	dla_debug("Enter: %s\n", __func__);
	op_desc = group->op_desc;

	group->pending = 0;

	/* update dependency graph for this task */
	ret = dla_update_consumers(group, op_desc, DLA_EVENT_OP_PROGRAMMED);
	dla_debug("Exit: %s\n", __func__);

	RETURN(ret);
}

void dumpCube(const char* tag, struct dla_data_cube c) {
            fprintf(stderr, "    [%s]\n", tag);
            fprintf(stderr, "      type        = %d\n", c.type);
            fprintf(stderr, "      addr_index  = %d\n", c.address);
            fprintf(stderr, "      offset      = %d\n", c.offset);
            fprintf(stderr, "      size        = %d\n", c.size);
            fprintf(stderr, "      WxHxC       = %d x %d x %d\n", c.width, c.height, c.channel);
            fprintf(stderr, "      line_stride = %d\n", c.line_stride);
            fprintf(stderr, "      surf_stride = %d\n", c.surf_stride);
            fprintf(stderr, "      plane_stride= %d\n", c.plane_stride);
        };

void dumpSdpOp(const char* tag, const struct dla_sdp_op o) {
	fprintf(stderr, "    [%s]\n", tag);
	fprintf(stderr, "      enable      = %d\n", (int)o.enable);
	fprintf(stderr, "      type        = %d\n", (int)o.type);
	fprintf(stderr, " mode=%d\n", (int)o.mode);
	fprintf(stderr, " act=%d\n", (int)o.act);
	fprintf(stderr, "      alu_type    = %d\n", (int)o.alu_type);
	fprintf(stderr, "      shift_value = %d\n", (int)o.shift_value);
	fprintf(stderr, " truncate=%d\n", (int)o.truncate);
	fprintf(stderr, "      precision   = %d\n", (int)o.precision);
	fprintf(stderr, "      alu_operand = %d\n", o.alu_operand);
	fprintf(stderr, " mul_operand=%d\n", o.mul_operand);
	fprintf(stderr, "      alu_cvt: scale=%d\n", o.cvt.alu_cvt.scale);
	fprintf(stderr, " trunc=%d\n", (int)o.cvt.alu_cvt.truncate);
	fprintf(stderr, " enable=%d\n", (int)o.cvt.alu_cvt.enable);
	fprintf(stderr, " offset=%d\n", o.cvt.alu_cvt.offset);
	fprintf(stderr, "      mul_cvt: scale=%d\n", o.cvt.mul_cvt.scale);
	fprintf(stderr, " trunc=%d\n", (int)o.cvt.mul_cvt.truncate);
	fprintf(stderr, " enable=%d\n", (int)o.cvt.mul_cvt.enable);
	fprintf(stderr, " offset=%d\n", o.cvt.mul_cvt.offset);
}

static int32_t
dla_read_config(struct dla_task *task, struct dla_processor *processor,
					struct dla_processor_group *group)
{
	int32_t ret;
	uint64_t base;
	int16_t index;
	uint8_t roi_index;
	struct dla_engine *engine;

	dla_debug("Enter: %s\n", __func__);

	engine = dla_get_engine();
	
	struct dla_engine *u__engine = dla_get_u__engine();
	struct nvdla_task *u__task = (struct nvdla_task *) u__engine->task->task_data;
    union dla_operation_container *u__ops = (union dla_operation_container *) u__task->ops;
	union dla_operation_container *u__operation_desc;
	union dla_surface_container *u__surfs = (union dla_surface_container *) u__task->surfs;
	union dla_surface_container *u__surface_desc;

	roi_index = group->roi_index;
	index = group->op_desc->index;

	u__operation_desc = &u__ops[index];
	u__surface_desc = &u__surfs[index];
	//// TODO: Check operation_desc or surface container info against Nikos output
	dla_trace("[KUMD] dla_read_config: common_operation_desc index: %d\n", index);
	dla_trace("[KUMD] surface_container address: 0x%08x\n", u__surface_desc);

	base = (sizeof(union dla_operation_container) *
			(uint64_t)engine->network->num_operations *
			(uint64_t)roi_index);
	base = base + (sizeof(union dla_operation_container) *
			(uint64_t)index);

	LOG_EVENT(roi_index, group->id, processor->op_type,
					LOG_READ_OP_CONFIG_START);

	ret = dla_data_read(engine->driver_context, task->task_data,
				task->operation_desc_addr,
				(void *)group->operation_desc,
				sizeof(union dla_operation_container),
				base);
	if (ret)
		goto exit;
	
	// group->operation_desc = u__operation_desc;
	uint8_t *src = u__operation_desc;
	uint8_t* dst = group->operation_desc;
	for (int i_byte = 0; i_byte < sizeof(union dla_operation_container); i_byte++) {
		dst[i_byte] = src[i_byte];
	}

	switch (group->op_desc->op_type) {
        case DLA_OP_BDMA: {
			break;
		}
        case DLA_OP_CONV: {
            struct dla_conv_op_desc conv = group->operation_desc->conv_op;
            struct dla_conv_surface_desc cs  = u__surface_desc->conv_surface;

			// fprintf(stderr, "    [CONV op_desc]\n");
			// fprintf(stderr, "      mode           = %d\n", (int)conv.conv_mode);
			// fprintf(stderr, "      data_reuse     = %d", (int)conv.data_reuse);
			// fprintf(stderr, " weight_reuse=%d\n", (int)conv.weight_reuse);
			// fprintf(stderr, "      skip_data_rls  = %d\n", (int)conv.skip_data_rls);
			// fprintf(stderr, " skip_weight_rls=%d\n", (int)conv.skip_weight_rls);
			// fprintf(stderr, "      entry_per_slice= %d\n", conv.entry_per_slice);
			// fprintf(stderr, "      data_format    = %d", (int)conv.data_format);
			// fprintf(stderr, " pixel_mapping=%d\n", (int)conv.pixel_mapping);
			// fprintf(stderr, "      fetch_grain    = %d\n", conv.fetch_grain);
			// fprintf(stderr, "      batch          = %d\n", (int)conv.batch);
			// fprintf(stderr, " weight_format=%d\n", (int)conv.weight_format);
			// fprintf(stderr, "      data_bank      = %d", (int)conv.data_bank);
			// fprintf(stderr, " weight_bank=%d\n", (int)conv.weight_bank);
			// fprintf(stderr, "      batch_stride   = %d\n", conv.batch_stride);
			// fprintf(stderr, "      post_extension = %d\n", (int)conv.post_extension);
			// fprintf(stderr, " release=%d\n", conv.release);
			// fprintf(stderr, "      input CSC WxHxC= %dx%dx%d\n",
			// conv.input_width_csc, conv.input_height_csc, conv.input_channel_csc);
			// fprintf(stderr, "      kernel WxHxC   = %dx%dx%d\n",
			// conv.kernel_width_csc, conv.kernel_height_csc, conv.kernel_channel_csc);
			// fprintf(stderr, "      input CMAC WxH = %dx%d\n",
			// conv.input_width_cmac, conv.input_height_cmac);
			// fprintf(stderr, "      bytes_per_kernel = %d\n", conv.bytes_per_kernel);
			// fprintf(stderr, "      conv_stride     = (%d, %d)\n",
			// (int)conv.conv_stride_x, (int)conv.conv_stride_y);
			// fprintf(stderr, "      pad (l,t,r,b)   = (%d,%d,%d,%d)\n",
			// (int)conv.pad_x_left, (int)conv.pad_y_top,
			// (int)conv.pad_x_right, (int)conv.pad_y_bottom);
			// fprintf(stderr, "      dilation (x,y)  = (%d, %d)\n",
			// (int)conv.dilation_x, (int)conv.dilation_y);
			// fprintf(stderr, "      in_precision    = %d", (int)conv.in_precision);
			// fprintf(stderr, " out_precision=%d\n", (int)conv.out_precision);
			// fprintf(stderr, "      pad_val         = %d\n", conv.pad_val);
			// fprintf(stderr, "      in_cvt:  scale=%d\n", conv.in_cvt.scale);
			// fprintf(stderr, " trunc=%d\n", (int)conv.in_cvt.truncate);
			// fprintf(stderr, " enable=%d\n", (int)conv.in_cvt.enable);
			// fprintf(stderr, " offset=%d\n", conv.in_cvt.offset);
			// fprintf(stderr, "      out_cvt: scale=%d\n", conv.out_cvt.scale);
			// fprintf(stderr, " trunc=%d\n", (int)conv.out_cvt.truncate);
			// fprintf(stderr, " enable=%d\n", (int)conv.out_cvt.enable);
			// fprintf(stderr, " offset=%d\n", conv.out_cvt.offset);

            // dumpCube("SRC",    cs.src_data);
            // dumpCube("DST",    cs.dst_data);
            // dumpCube("WEIGHT", cs.weight_data);
			break;
		}
        case DLA_OP_SDP: {
			struct dla_sdp_op_desc sdp = group->operation_desc->sdp_op;
            struct dla_sdp_surface_desc ss = u__surface_desc->sdp_surface;
			// const dla_sdp_op_desc&     sdp = oc.sdp_op;
            // const dla_sdp_surface_desc& ss = sc.sdp_surface;

            // fprintf(stderr, "    [SDP op_desc]\n");
            // fprintf(stderr, "      src_precision = %d\n", (int)sdp.src_precision);
            // fprintf(stderr, " dst_precision=%d\n", (int)sdp.dst_precision);
            // fprintf(stderr, "      lut_index     = %d\n", sdp.lut_index);
            // fprintf(stderr, "      conv_mode     = %d\n", (int)sdp.conv_mode);
            // fprintf(stderr, "      batch_num     = %d\n", (int)sdp.batch_num);
            // fprintf(stderr, " batch_stride=%d\n", sdp.batch_stride);

            // dumpSdpOp("X1_OP", sdp.x1_op);
            // dumpSdpOp("X2_OP", sdp.x2_op);
            // dumpSdpOp("Y_OP",  sdp.y_op);

            // dumpCube("SRC", ss.src_data);
            // dumpCube("X1",  ss.x1_data);
            // dumpCube("X2",  ss.x2_data);
            // dumpCube("Y",   ss.y_data);
            // dumpCube("DST", ss.dst_data);
			break;
		}
        case DLA_OP_PDP: {
			struct dla_pdp_op_desc pdp = group->operation_desc->pdp_op;
            struct dla_pdp_surface_desc ps = u__surface_desc->pdp_surface;

            // fprintf(stderr, "    [PDP op_desc]\n");
            // fprintf(stderr, "      pool_mode    = %d", (int)pdp.pool_mode);
            // fprintf(stderr, " pool_width=%d", (int)pdp.pool_width);
            // fprintf(stderr, " pool_height=%d\n", (int)pdp.pool_height);
            // fprintf(stderr, "      split_num    = %d\n",  (int)pdp.split_num);
            // fprintf(stderr, "      stride_x/y   = (%d, %d)\n",
			// 	(int)pdp.stride_x, (int)pdp.stride_y);
            // fprintf(stderr, "      pad (l,r,t,b)= (%d , %d, %d, %d)\n",
			// 	(int)pdp.pad_left, (int)pdp.pad_right,
			// 	(int)pdp.pad_top, (int)pdp.pad_bottom);
            // fprintf(stderr, "      precision    = %d\n", (int)pdp.precision);

            // dumpCube("SRC", ps.src_data);
            // dumpCube("DST", ps.dst_data);
			break;
		}
        case DLA_OP_CDP: {
			break;
		}
        case DLA_OP_RUBIK: {
			break;
		}

	}

	LOG_EVENT(roi_index, group->id, processor->op_type,
					LOG_READ_OP_CONFIG_END);

	base = (sizeof(union dla_surface_container) *
			(uint64_t)engine->network->num_operations *
			(uint64_t)roi_index);

	base = base + (sizeof(union dla_surface_container) *
			(uint64_t)index);

	LOG_EVENT(roi_index, group->id, processor->op_type,
					LOG_READ_SURF_CONFIG_START);

	ret = dla_data_read(engine->driver_context, task->task_data,
				task->surface_desc_addr,
				(void *)group->surface_desc,
				sizeof(union dla_surface_container), base);
	if (ret)
		goto exit;

	// group->surface_desc = u__surface_desc;
	src = u__surface_desc;
	dst = group->surface_desc;
	for (int i_byte = 0; i_byte < sizeof(union dla_surface_container); i_byte++) {
		dst[i_byte] = src[i_byte];
	}

	LOG_EVENT(roi_index, group->id, processor->op_type,
					LOG_READ_SURF_CONFIG_END);

	processor->dump_config(group);

exit:
	dla_debug("Exit: %s\n", __func__);
	RETURN(ret);
}

static void
dla_reset_group(struct dla_processor_group *group)
{
	int32_t i;

	for (i = 0; i < DLA_OP_NUM; i++) {
		dla_put_op_desc(group->consumers[i]);
		group->consumers[i] = NULL;
	}

	dla_put_op_desc(group->fused_parent);
	group->fused_parent = NULL;

	dla_put_op_desc(group->op_desc);
	group->op_desc = NULL;
}

static int
dla_prepare_operation(struct dla_processor *processor,
			struct dla_common_op_desc *op_desc,
			uint8_t roi_index, uint32_t *group_number)
{
	int32_t ret = 0;
	uint8_t group_id;
	uint8_t rdma_id;
	struct dla_processor_group *group;
	struct dla_engine *engine = dla_get_engine();
	struct nvdla_device *nvdla_dev = (struct nvdla_device *)engine->driver_context;
	nvdla_dev->current_dla_id = processor->dev_id;
	int32_t ii;

	dla_debug("Enter: %s\n", __func__);
	/*
	 * If not already programmed then find out if
	 * processor is free and which group is free
	 */
	ret = utils_get_free_group(processor, &group_id, &rdma_id);
	if (ret) {
		dla_debug("processor:%s register groups are busy\n",
			processor->name);
		goto exit;
	} else {
		dla_info("processor:%s group:%d, rdma_group:%d available\n",
				processor->name, group_id, rdma_id);
	}
	*group_number = group_id;
	group = &processor->groups[group_id];

	/*
	 * update operation descriptor
	 */
	group->op_desc = op_desc;
	dla_get_refcount(op_desc);
	group->id = group_id;
	group->roi_index = roi_index;
	group->rdma_id = rdma_id;

	ret = dla_read_config(engine->task, processor, group);
	if (ret)
		goto exit;

	group->pending = 1;

	processor->group_status |= (1 << group->id);

	processor->rdma_check(group);
	if (group->is_rdma_needed) {
		group->rdma_id = rdma_id;
		processor->rdma_status |= (1 << rdma_id);
	}

	// int16_t max_index = -1;
	// int16_t curr_processor_tail_op_index = -1;
	// struct dla_processor* max_processor;
	struct dla_processor* curr_processor;
	for (ii = 0; ii < engine->num_dlas; ii++) {
		curr_processor = &engine->processors[ii][processor->op_type];
		curr_processor->tail_op = op_desc;
		// if (curr_processor->tail_op != NULL) {
		// 	if (curr_processor->tail_op->consumers[processor->op_type].index == -1) {
		// 		max_index = -1;
		// 		break;
		// 	}
		// 	if (curr_processor->tail_op->consumers[processor->op_type].index > max_index) {
		// 		max_index = curr_processor->tail_op->consumers[processor->op_type].index;
		// 		max_processor = &engine->processors[ii][processor->op_type];
		// 		dla_debug("%s: ", curr_processor->name);
		// 		dla_debug("tail_op index: %d, tail_op consumer index: %d\n",
		// 			curr_processor->tail_op->index,
		// 			curr_processor->tail_op->consumers[processor->op_type].index);
		// 	}
		// }
	}
	// processor->tail_op = op_desc;

	// index = max_index;
	// max_processor->roi_index = 0;

	// [gem5-plus]
	// I assume here we can safely say
	// that this op_desc is assigned to this processor.
	// Update job_map.
	update_job_map(op_desc, processor);
exit:
	dla_debug("Exit: %s status=%d\n", __func__, ret);
	RETURN(ret);
}

static int
dla_program_operation(struct dla_processor *processor,
			struct dla_processor_group *group)
{
	int32_t i;
	int32_t ret = 0;
	struct dla_common_op_desc *op_desc;
	struct dla_engine *engine = dla_get_engine();
	struct nvdla_device *nvdla_dev = (struct nvdla_device *)engine->driver_context;
	nvdla_dev->current_dla_id = processor->dev_id;

	dla_debug("Enter: %s\n", __func__);

	dla_info("Program %s operation index %d ROI %d Group[%d]\n",
					processor->name,
					group->op_desc->index,
					group->roi_index,
					group->id);

	group->programming = 1;

	op_desc = group->op_desc;

	processor->set_producer(group->id, group->rdma_id);

	LOG_EVENT(group->roi_index, group->id, processor->op_type,
						LOG_PROGRAM_START);

	ret = processor->program(group);
	if (ret)
		goto exit;

	LOG_EVENT(group->roi_index, group->id, processor->op_type,
						LOG_PROGRAM_END);

	/**
	 * Pre-fetch consumers
	 */
	for (i = 0; i < DLA_OP_NUM; i++) {
		group->consumers[i] = dla_get_op_desc(engine->task,
					op_desc->consumers[i].index, i,
					group->roi_index);
	}

	group->fused_parent = dla_get_op_desc(engine->task,
					op_desc->fused_parent.index,
					op_desc->op_type - 1,
					group->roi_index);

	if (group->fused_parent != NULL) {
		if (group->fused_parent->op_type != (op_desc->op_type - 1)) {
			dla_warn("Invalid fused op type");
			ret = ERR(INVALID_INPUT);
			goto exit;
		}
	}

	ret = dla_op_programmed(processor, group, group->rdma_id);
	if (!ret)
		goto exit;

exit:
	group->programming = 0;
	dla_debug("Exit: %s status=%d\n", __func__, ret);
	RETURN(ret);
}

static int
dla_enable_operation(struct dla_processor *processor,
			struct dla_common_op_desc *op_desc)
{
	int32_t ret = 0;
	int32_t group_id;
	struct dla_engine *engine;
	struct dla_processor_group *group;
	struct nvdla_device *nvdla_dev;

	dla_debug("Enter: %s\n", __func__);
	assert(op_desc->dependency_count == 0);

	/**
	 * If some operation has reported error then skip
	 * enabling next operations
	 */
	engine = dla_get_engine();
	if (engine->status)
		goto exit;

	nvdla_dev = (struct nvdla_device *)engine->driver_context;
	nvdla_dev->current_dla_id = processor->dev_id;

	/**
	 * Find out if operation is already programmed
	 */
	group_id = 0;
	group = &processor->groups[group_id];
	if ((processor->group_status & (1 << group_id)) &&
			group->op_desc->index == op_desc->index &&
			group->roi_index == op_desc->roi_index &&
			!group->pending)
		goto enable_op;

	group_id = 1;
	group = &processor->groups[group_id];
	if ((processor->group_status & (1 << group_id)) &&
			group->op_desc->index == op_desc->index &&
			group->roi_index == op_desc->roi_index &&
			!group->pending)
		goto enable_op;

	/**
	 * Operation is not programmed yet, ignore
	 */
	dla_debug("exit %s without actual enable due to processor "
				"hasn't been programmed\n", __func__);
	goto exit;

enable_op:
	/**
	 * If this event is triggered as part of programming same
	 * group then skip enable, it will get enabled after programming
	 * is complete
	 */
	if (group->programming)
		goto exit;

	if (group->active) {
		dla_debug("Processor:%s already enabled on group:%d\n",
			processor->name, group_id);
		goto exit;
	}

	dla_info("Enable %s operation index %d ROI %d\n",
					processor->name,
					group->op_desc->index,
					group->roi_index);

	processor->set_producer(group->id, group->rdma_id);

	LOG_EVENT(group->roi_index, group->id, processor->op_type,
						LOG_OPERATION_START);

	ret = processor->enable(group);
	if (ret)
		goto exit;

	ret = dla_op_enabled(group);
exit:
	dla_debug("Exit: %s status=%d\n", __func__, ret);
	RETURN(ret);
}

static int
dla_submit_operation(struct dla_processor *processor,
			struct dla_common_op_desc *op_desc,
			uint8_t roi_index)
{
	int32_t err;
	uint32_t group_id = 0;

	dla_debug("Enter: %s\n", __func__);

	dla_info("Prepare %s operation index %d ROI %d dep_count %d\n",
			processor->name, op_desc->index, roi_index,
			op_desc->dependency_count);
	err = dla_prepare_operation(processor, op_desc, roi_index, &group_id);
	if (err)
		goto exit;

	if (!processor->is_ready(processor, &processor->groups[group_id]))
		goto exit;

	err = dla_program_operation(processor, &processor->groups[group_id]);
	if (err)
		goto exit;

	if (op_desc->dependency_count == 0)
		err = dla_enable_operation(processor, op_desc);

exit:
	dla_debug("Exit: %s\n", __func__);
	RETURN(err);
}

/**
 * Dequeue next operation of same type from list of operations
 */
static int32_t
dla_dequeue_operation(struct dla_engine *engine,
			struct dla_processor *processor)
{
	int32_t ret = 0;
	int16_t index;
	struct dla_common_op_desc *consumer;
	int32_t ii;

	dla_debug("Enter: %s\n", __func__);

	if (engine->status) {
		dla_debug("Skip dequeue op as engine has reported error\n");
		goto exit;
	}

	/**
	 * If we are done processing all ROIs for current op then
	 * load next op of same type otherwise reload same op for
	 * next ROI.
	 */
	if (processor->roi_index == (engine->network->num_rois - 1)) {
		index = processor->tail_op->consumers[processor->op_type].index;
		if (-1 == index) {
			/**
			 * It means we are done processing
			 * all ops of this type
			 */
			dla_debug("exit %s as there's no further operation\n",
				processor->name);
			goto exit;
		}
		processor->roi_index = 0;
	} else {
		processor->roi_index++;
		index = processor->tail_op->index;
	}

	dla_debug("Dequeue op from %s processor, index=%d ROI=%d\n",
			processor->name, index, processor->roi_index);

	/**
	 * Get operation descriptor
	 */
	consumer = dla_get_op_desc(engine->task, index,
				processor->op_type, processor->roi_index);
	if (consumer == NULL) {
		ret = ERR(NO_MEM);
		dla_error("Failed to allocate op_desc");
		goto exit;
	}

	uint8_t op_type = processor->op_type;
	// processor = &engine->processors[1][op_type];
	for (ii = 0; ii < engine->num_dlas; ii++) {
		processor = &engine->processors[ii][op_type];
		dla_debug("Check custom dependencies");
		if (check_custom_deps(consumer, processor)) {
			dla_debug("custom dependencies solved");
			ret = dla_submit_operation(processor, consumer, processor->roi_index);
			/**
			 * TODO: check that operation was actually scheduled
			 * (maybe both groups were busy)
			 */
			break;
		}
	}
	dla_put_op_desc(consumer);

exit:
	dla_debug("Exit: %s\n", __func__);
	RETURN(ret);
}

static int
dla_update_dependency(struct dla_consumer *consumer,
			struct dla_common_op_desc *op_desc,
			uint8_t event, uint8_t roi_index, uint8_t processor_id)
{
	int32_t i;
	int32_t ret = 0;
	struct dla_processor *processor;
	struct dla_engine *engine = dla_get_engine();
	int32_t ii;

	if (consumer->index == -1)
		goto exit;

	/* Update dependency only if event matches */
	if (event != consumer->event)
		goto exit;

	/**
	 * If consumer index is valid but op desc is NULL means
	 * op desc for consumer was not pre-fetched
	 */
	if (op_desc == NULL) {
		ret = ERR(INVALID_INPUT);
		dla_error("Operation descriptor is NULL, consumer index %d",
				consumer->index);
		goto exit;
	}

	assert(op_desc->dependency_count > 0);

	dla_debug("Update dependency operation index %d ROI %d DEP_COUNT=%d\n",
					op_desc->index, op_desc->roi_index,
					op_desc->dependency_count);
	op_desc->dependency_count--;

	if (op_desc->dependency_count == 0) {
		for (ii = 0; ii < engine->num_dlas; ii++) {
			/**
			 * TODO: [GEM5-PLUS, MULTI-DLA] maybe check both DLAs for operation
			 */
			dla_debug("check processor in dev %d", ii);
			processor = &engine->processors[ii][op_desc->op_type];
			dla_debug("enable %s in %s as depdency are resolved\n",
				processor->name, __func__);

			ret = dla_enable_operation(processor, op_desc);
			// if (ret)
			// 	goto exit;
		}
	}
exit:
	RETURN(ret);
}

static int
dla_update_consumers(struct dla_processor_group *group,
		     struct dla_common_op_desc *op,
		     uint8_t event)
{
	int32_t i;
	int32_t ret = 0;
	struct dla_engine *engine = dla_get_engine();

	if (engine->status) {
		dla_debug("Skip update as engine has reported error\n");
		goto exit;
	}

	for (i = 0; i < DLA_OP_NUM; i++) {
		ret = dla_update_dependency(&op->consumers[i],
						group->consumers[i],
						event, group->roi_index,
						group->processor_id);
		if (ret) {
			dla_error("Failed to update dependency for "
				"consumer %d, ROI %d", i, group->roi_index);
			goto exit;
		}
	}

	ret = dla_update_dependency(&op->fused_parent,
					group->fused_parent,
					event, group->roi_index,
					group->processor_id);
	if (ret) {
		dla_error("Failed to update dependency for "
			"fused parent, ROI %d", group->roi_index);
		goto exit;
	}

exit:
	RETURN(ret);
}

/**
 * Handle operation completion notification
 */
int
dla_op_completion(struct dla_processor *processor,
		  struct dla_processor_group *group)
{
	int32_t ret;
#if STAT_ENABLE
	uint64_t stat_data_address;
	uint64_t stat_base;
#endif /* STAT_ENABLE */
	struct dla_task *task;
	struct dla_common_op_desc *op_desc;
	struct dla_processor_group *next_group;
	struct dla_engine *engine = dla_get_engine();

	dla_debug("Enter:%s processor %s group%u\n", __func__,
					processor->name, group->id);

	dla_info("Completed %s operation index %d ROI %d\n",
					processor->name,
					group->op_desc->index,
					group->roi_index);

	task = engine->task;

	/**
	 * Mark OP as done only when all ROIs are done for that
	 * operation
	 */
	if (group->roi_index == (engine->network->num_rois - 1))
		engine->num_proc_hwl++;

	op_desc = group->op_desc;

// [gem5-plus-se] WARNING: just disabling it; I don't really know what it does
#ifndef STAT_ENABLE
	if (engine->stat_enable == (uint32_t)1) {
		processor->get_stat_data(processor, group);

		processor->dump_stat(processor);

		stat_data_address = (uint64_t)(engine->task->stat_data_addr +
				(sizeof(union dla_stat_container) *
				(uint64_t)(engine->network->num_operations) *
				(uint64_t)(op_desc->roi_index)));

		stat_base = (stat_data_address +
				(sizeof(union dla_stat_container) *
				(uint64_t)op_desc->index));

		/**
		 * Flush stat descriptor to DRAM
		 */
		ret = dla_data_write(engine->driver_context, task->task_data,
					(void *)(processor->stat_data_desc),
					stat_base,
					sizeof(union dla_stat_container),
					0);
		if (ret < 0)
			dla_error("Failed to write stats to DMA memory\n");
	}
#endif /* STAT_ENABLE */

	/**
	 * Get an extra reference count to keep op descriptor
	 * in cache until this operation completes
	 */
	dla_get_refcount(op_desc);

	LOG_EVENT(group->roi_index, group->id, processor->op_type,
						LOG_OPERATION_END);

	processor->group_status &= ~(1 << group->id);
	if (group->is_rdma_needed) {
		group->is_rdma_needed = 0;
		processor->rdma_status &= ~(1 << group->rdma_id);
		group->rdma_id = 0;
	}
	group->active = 0;
	group->lut_index = -1;
	processor->last_group = group->id;

	/**
	 * Switch consumer pointer to next group
	 */
	processor->consumer_ptr = !group->id;

	/**
	 * update dependency graph for this task
	 * TODO: Add proper error handling
	 */
	ret = dla_update_consumers(group, op_desc, DLA_EVENT_OP_COMPLETED);
	if (ret)
		goto exit;

	dla_info("%d HWLs done, totally %d layers\n",
				engine->num_proc_hwl,
				engine->network->num_operations);

	/* free operation descriptor from cache */
	dla_reset_group(group);

	/* if not hwl pending, means network completed */
	if (engine->network->num_operations == engine->num_proc_hwl) {
		dla_put_op_desc(op_desc);
		goto exit;
	}

	next_group = &processor->groups[!group->id];
	if (next_group->pending && !engine->status) {
		/**
		 * Next group must be ready here for programming,
		 * if not means it is an error
		 */
		if (!processor->is_ready(processor, next_group))
			goto dequeue_op;

		ret = dla_program_operation(processor, next_group);
		if (ret)
			goto exit;

		if (next_group->op_desc->dependency_count != 0)
			goto dequeue_op;

		ret = dla_enable_operation(processor,
					   next_group->op_desc);
		if (ret)
			goto exit;
	}

dequeue_op:
	/* dequeue operation from this processor */
	ret = dla_dequeue_operation(engine, processor);

exit:
	dla_put_op_desc(op_desc);
	dla_debug("Exit:%s processor %s group%u status=%d\n",
				__func__, processor->name,
				group->id, ret);

	RETURN(ret);
}

/**
 * Read network configuration from DRAM, network descriptor address
 * is always first in the address list. Network configuration contains
 * offset in address list for addresses of other lists used to
 * execute network
 *
 * @engine: Engine instance
 * @return: 0 for success
 */
static int
dla_read_network_config(struct dla_engine *engine,
				struct nvdla_ioctl_submit_task *u__task)
{
	int32_t ret;
	uint64_t network_addr;
	struct dla_task *task = engine->task;

	dla_debug("Enter:%s\n", __func__);

	/**
	 * Read address list from DRAM to DMEM
	 */
	ret = dla_read_address_list(engine);
	if (ret) {
		dla_error("Failed to read address list");
		goto exit;
	}

	//// [KUMD] Reached HERE ////

	/**
	 * Read network descriptor address from address list. It is always
	 * at index 0.
	 */
	ret = dla_get_dma_address(engine->driver_context, task->task_data,
						0, (void *)&network_addr,
						DESTINATION_PROCESSOR);
	if (ret) {
		dla_error("Failed to read network desc address");
		goto exit;
	}

	/**
	 * Read network descriptor, it has information for a network
	 * such as all address indexes.
	 */
	ret = dla_data_read(engine->driver_context, task->task_data,
				network_addr, (void *)&network,
				sizeof(struct dla_network_desc),
				0);
	if (ret) {
		dla_error("Failed to read network descriptor");
		goto exit;
	}

	//// [KUMD] Compare network info ////
	// if (u__task->network != (&network)) {
	// 	dla_error("u__network is different, kernel: 0x%08x\n", &network);
	// 	ret = ERR(INVALID_INPUT);
	// 	goto exit;
	// }
	dla_info("[KUMD]: u__task->network: 0x%08x\n", u__task->network);

	// dla_debug_network_desc(&network);
	dla_debug_network_desc(u__task->network);

	if (u__task->network->num_operations == 0)
		goto exit;

	/**
	 * Read operation descriptor list address from address list
	 */
	ret = dla_get_dma_address(engine->driver_context, task->task_data,
				network.operation_desc_index,
				(void *)&task->operation_desc_addr,
				DESTINATION_PROCESSOR);
	if (ret) {
		dla_error("Failed to read operation desc list address");
		goto exit;
	}

	dla_info("[KUMD] task->operation_desc_addr: 0x%08x\n", task->operation_desc_addr);

	/**
	 * Read surface descriptor list address from address list
	 */
	ret = dla_get_dma_address(engine->driver_context, task->task_data,
				network.surface_desc_index,
				(void *)&task->surface_desc_addr,
				DESTINATION_PROCESSOR);
	// if (ret) {
	// 	dla_error("Failed to read surface desc list address");
	// 	goto exit;
	// }

	/**
	 * Read dependency graph address from address list
	 */
	ret = dla_get_dma_address(engine->driver_context, task->task_data,
				network.dependency_graph_index,
				(void *)&task->dependency_graph_addr,
				DESTINATION_PROCESSOR);
	// if (ret) {
	// 	dla_error("Failed to ready dependency graph address");
	// 	goto exit;
	// }

	/**
	 * Read LUT data list address from address list
	 */
	if (network.num_luts) {
		ret = dla_get_dma_address(engine->driver_context,
					task->task_data,
					network.lut_data_index,
					(void *)&task->lut_data_addr,
					DESTINATION_PROCESSOR);
		// if (ret) {
		// 	dla_error("Failed to read LUT list address");
		// 	goto exit;
		// }
	}

	/**
	 * Read address for ROI information
	 */
	if (network.dynamic_roi) {
		/**
		 * Read ROI array address from address list
		 */
		ret = dla_get_dma_address(engine->driver_context,
					task->task_data,
					network.roi_array_index,
					(void *)&task->roi_array_addr,
					DESTINATION_PROCESSOR);
		if (ret) {
			dla_error("Failed to read ROI array address");
			goto exit;
		}

		ret = dla_data_read(engine->driver_context, task->task_data,
					task->roi_array_addr,
					(void *)&roi_array_length,
					sizeof(uint64_t),
					0);
		if (ret) {
			dla_error("Failed to read ROI array length");
			goto exit;
		}

		/**
		 * Number of ROIs detected can't be greater than maximum number
		 * ROIs this network can process
		 */
		if (roi_array_length > network.num_rois) {
			dla_error("Invalid number of ROIs detected");
			ret = ERR(INVALID_INPUT);
			goto exit;
		}

		network.num_rois = roi_array_length;

		/**
		 * Read surface address from address list
		 */
		ret = dla_get_dma_address(engine->driver_context,
						task->task_data,
						network.surface_index,
						(void *)&task->surface_addr,
						DESTINATION_DMA);
		if (ret) {
			dla_error("Failed to read surface address");
			goto exit;
		}
	}

#ifndef STAT_ENABLE
	if (network.stat_list_index != -1) {
		ret = dla_get_dma_address(engine->driver_context,
						task->task_data,
						network.stat_list_index,
						(void *)&task->stat_data_addr,
						DESTINATION_PROCESSOR);
		if (ret) {
			dla_error("Failed to read stat address");
			goto exit;
		}
	}
#endif /* STAT_ENABLE */

exit:
	dla_debug("Exit:%s status=%d\n", __func__, ret);
	RETURN(ret);
}

static int
dla_initiate_processors(struct dla_engine *engine)
{
	int32_t i;
	int32_t ret = 0;
	int16_t index;
	struct dla_processor *processor;
	struct dla_common_op_desc *consumer;
	struct dla_network_desc *nw;
	struct nvdla_device *nvdla_dev = (struct nvdla_device *)engine->driver_context;

	dla_debug("Enter: %s\n", __func__);

	if (!engine) {
		ret = ERR(INVALID_INPUT);
		goto exit;
	}

	nw = engine->network;

	/* Validate operation heads before initiating processors */
	for (i = 0; i < DLA_OP_NUM; i++) {
		if (nw->op_head[i] >= nw->num_operations) {
			ret = ERR(INVALID_INPUT);
			dla_error("Invalid op_head %d for op %d",
						nw->op_head[i], i);
			goto exit;
		}
	}

	for (i = 0; i < DLA_OP_NUM; i++) {
		index = nw->op_head[i];

		/* If there is no op for this type then continue */
		if (-1 == index)
			continue;

		consumer = dla_get_op_desc(engine->task, index, i, 0);
		/*
		 * if consumer is NULL, it means either data copy error
		 * or cache insufficient - we should fix it
		 **/
		if (consumer == NULL) {
			dla_error("Failed to allocate memory for op_head[%d]=%d",
							i, index);
			ret = ERR(NO_MEM);
			goto exit;
		}

		processor = &engine->processors[0][consumer->op_type];

		ret = dla_submit_operation(processor, consumer, 0);
		dla_put_op_desc(consumer);
		if (ret && ret != ERR(PROCESSOR_BUSY)) {
			dla_error("Failed to submit %s op from index %u\n",
						processor->name, index);
			goto exit;
		}

		ret = dla_dequeue_operation(engine, processor);
		if (ret) {
			dla_error("Failed to dequeue op for %s processor",
							processor->name);
			goto exit;
		}
	}
exit:
	dla_debug("Exit: %s status=%d\n", __func__, ret);
	RETURN(ret);
}

static int
dla_handle_events(struct dla_processor *processor)
{
	int32_t j;
	int32_t ret = 0;
	uint8_t group_id;
	struct dla_processor_group *group;

	dla_debug("Enter:%s, processor:%s\n", __func__, processor->name);

	group_id = !processor->last_group;

	for (j = 0; j < DLA_NUM_GROUPS; j++) {
		group = &processor->groups[group_id];

		if ((1 << DLA_EVENT_CDMA_WT_DONE) & group->events) {
			dla_info("Handle cdma weight done event, processor %s "
				"group %u\n", processor->name, group->id);

			ret = dla_update_consumers(group,
						   group->op_desc,
						   DLA_EVENT_CDMA_WT_DONE);
			if (ret)
				goto exit;
		}

		if ((1 << DLA_EVENT_CDMA_DT_DONE) & group->events) {
			dla_info("Handle cdma data done event, processor %s "
				"group %u\n", processor->name, group->id);

			ret = dla_update_consumers(group,
						   group->op_desc,
						   DLA_EVENT_CDMA_DT_DONE);
			if (ret)
				goto exit;
		}

		/**
		 * Handle complete after all other events
		 */
		if ((1 << DLA_EVENT_OP_COMPLETED) & group->events) {
			dla_info("Handle op complete event, processor %s "
				"group %u\n", processor->name, group->id);

			ret = dla_op_completion(processor, group);
			if (ret)
				goto exit;
		}

		/**
		 * Clear all events
		 */
		group->events = 0;
		group_id = !group_id;
	}
exit:
	dla_debug("Exit:%s, ret:%x\n", __func__, ret);
	RETURN(ret);
}

int
dla_process_events(void *engine_context, uint32_t *task_complete)
{
	int32_t i;
	int32_t ret = 0;
	struct dla_engine *engine = (struct dla_engine *)engine_context;
	int32_t ii;

	for (ii = 0; ii < engine->num_dlas; ii++) {
		for (i = 0; i < DLA_OP_NUM; i++) {
			struct dla_processor *processor;

			processor = &engine->processors[ii][i];
			ret = dla_handle_events(processor);
			/**
			 * Incase engine status is non-zero, then don't
			 * update the engine status. We should keep its
			 * status for later cleaning of engine.
			 */
			if (!engine->status)
				engine->status = ret;
		}
	}

	if (engine->network->num_operations == engine->num_proc_hwl)
		*task_complete = 1;

	RETURN(ret);
}

/**
 * Execute task selected by task scheduler
 *
 * 1. Read network configuration for the task
 * 2. Initiate processors with head of list for same op
 * 3. Start processing events received
 */
int
dla_execute_task(void *engine_context, void *task_data, void *config_data,
			void *u_task)
{
	int32_t ret;
	struct dla_engine *engine = (struct dla_engine *)engine_context;

	struct dla_engine *u__engine = dla_get_engine();

	struct nvdla_ioctl_submit_task *u__task = (struct nvdla_ioctl_submit_task *) u_task;

	// [KUMD] //
	// compare engines
	// if (u__engine != engine) {
	// 	dla_error("u__engine is different\n");
	// 	ret = ERR(INVALID_INPUT);
	// 	goto complete;
	// }

	if (u__engine == NULL) {
		dla_error("engine is NULL\n");
		ret = ERR(INVALID_INPUT);
		goto complete;
	}

	if (u__engine->task == NULL) {
		dla_error("task is NULL\n");
		ret = ERR(INVALID_INPUT);
		goto complete;
	}

	if (u__engine->task->task_data != NULL) {
		/* We have on the fly tasks running */
		dla_warn("Already some task in progress");
		ret = ERR(PROCESSOR_BUSY);
		goto complete;
	}

	engine->task->task_data = task_data;
	engine->config_data = config_data;
	engine->network = u__task->network; // &network;
	engine->num_proc_hwl = 0;
	engine->stat_enable = 0;

	//// [KUMD] ////
	// u__engine = dla_get_u__engine();
	// u__engine->task->task_data = task_data;
	// u__engine->task->task_data = u__task;
	// u__engine->config_data = config_data;
	// // // u__engine->network = &network;
	// u__engine->network = u__task->network;
	// u__engine->num_proc_hwl = 0;
	// u__engine->stat_enable = 0;

	LOG_EVENT(0, 0, 0, LOG_TASK_START);

	// ret = dla_read_network_config(u__engine, u__task);
	ret = dla_read_network_config(engine, u__task);
	if (ret)
		goto complete;

	dla_debug_address_info(engine->task);

	/**
	 * If no operations in a task means nothing to do, NULL task
	 */
	if (engine->network->num_operations == 0)
		goto complete;

#ifndef STAT_ENABLE
	if (network.stat_list_index != -1)
		engine->stat_enable = 1;
#endif /* STAT_ENABLE */

	ret = dla_initiate_processors(engine);
	engine->status = ret;

complete:
	LOG_EVENT(0, 0, 0, LOG_TASK_END);

	RETURN(ret);
}

void
dla_clear_task(void *engine_context)
{
	int32_t i, j;
	struct dla_engine *engine = (struct dla_engine *)engine_context;

	for (i = 0; i < DLA_OP_NUM; i++) {
		struct dla_processor *processor = &engine->processors[0][i];

		processor->roi_index = 0;
		processor->group_status = 0;
		processor->rdma_status = 0;

		processor->tail_op = NULL;

		for (j = 0; j < DLA_NUM_GROUPS; j++) {
			struct dla_processor_group *group =
						&processor->groups[j];

			group->rdma_id = group->id;
			group->active = 0;
			group->events = 0;
			group->roi_index = 0;
			group->is_rdma_needed = 0;
			group->lut_index = -1;
		}
	}

	engine->task->task_data = NULL;
	engine->network = NULL;
	engine->num_proc_hwl = 0;
	engine->status = 0;
	engine->stat_enable = 0;

	dla_debug("reset engine done\n");
}
