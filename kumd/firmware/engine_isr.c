/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
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

#include <stdio.h>

#include <opendla.h>
#include <dla_debug.h>
#include <dla_engine.h>
#include <dla_interface.h>

#include "dla_engine_internal.h"
#include "nvdla_linux.h"
/*
int32_t dla_isr_handler(void *engine_data, uint8_t dla_id)
{
	uint32_t mask;
	uint32_t reg;
	struct dla_processor *processor = NULL;
	struct dla_processor_group *group;
	struct dla_engine *engine = (struct dla_engine *)engine_data;
	struct nvdla_device *nvdla_dev = (struct nvdla_device *)engine->driver_context;

	nvdla_dev->current_dla_id = dla_id;

	mask = glb_reg_read(S_INTR_MASK);
	reg = glb_reg_read(S_INTR_STATUS);
	
	dla_trace("Enter: dla_isr_handler, reg:%x, mask:%x\n", reg, mask);
	fprintf(stderr, " In dla_isr_handler \n");
	if (reg & MASK(GLB_S_INTR_STATUS_0, CACC_DONE_STATUS0)) {
		processor = &engine->processors[dla_id][DLA_OP_CONV];
		group = &processor->groups[0];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, CACC_DONE_STATUS1)) {
		processor = &engine->processors[dla_id][DLA_OP_CONV];
		group = &processor->groups[1];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, SDP_DONE_STATUS0)) {
		processor = &engine->processors[dla_id][DLA_OP_SDP];
		group = &processor->groups[0];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, SDP_DONE_STATUS1)) {
		processor = &engine->processors[dla_id][DLA_OP_SDP];
		group = &processor->groups[1];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, CDP_DONE_STATUS0)) {
		processor = &engine->processors[dla_id][DLA_OP_CDP];
		group = &processor->groups[0];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, CDP_DONE_STATUS1)) {
		processor = &engine->processors[dla_id][DLA_OP_CDP];
		group = &processor->groups[1];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, RUBIK_DONE_STATUS0)) {
		processor = &engine->processors[dla_id][DLA_OP_RUBIK];
		group = &processor->groups[0];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, RUBIK_DONE_STATUS1)) {
		processor = &engine->processors[dla_id][DLA_OP_RUBIK];
		group = &processor->groups[1];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, PDP_DONE_STATUS0)) {
		processor = &engine->processors[dla_id][DLA_OP_PDP];
		group = &processor->groups[0];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, PDP_DONE_STATUS1)) {
		processor = &engine->processors[dla_id][DLA_OP_PDP];
		group = &processor->groups[1];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, BDMA_DONE_STATUS0)) {
		processor = &engine->processors[dla_id][DLA_OP_BDMA];
		group = &processor->groups[0];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, BDMA_DONE_STATUS1)) {
		processor = &engine->processors[dla_id][DLA_OP_BDMA];
		group = &processor->groups[1];
		group->events |= (1 << DLA_EVENT_OP_COMPLETED);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, CDMA_DAT_DONE_STATUS0)) {
		processor = &engine->processors[dla_id][DLA_OP_CONV];
		group = &processor->groups[0];
		group->events |= (1 << DLA_EVENT_CDMA_DT_DONE);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, CDMA_DAT_DONE_STATUS1)) {
		processor = &engine->processors[dla_id][DLA_OP_CONV];
		group = &processor->groups[1];
		group->events |= (1 << DLA_EVENT_CDMA_DT_DONE);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, CDMA_WT_DONE_STATUS0)) {
		processor = &engine->processors[dla_id][DLA_OP_CONV];
		group = &processor->groups[0];
		group->events |= (1 << DLA_EVENT_CDMA_WT_DONE);
	}
	if (reg & MASK(GLB_S_INTR_STATUS_0, CDMA_WT_DONE_STATUS1)) {
		processor = &engine->processors[dla_id][DLA_OP_CONV];
		group = &processor->groups[1];
		group->events |= (1 << DLA_EVENT_CDMA_WT_DONE);
	}

	glb_reg_write(S_INTR_STATUS, reg);

	mask = glb_reg_read(S_INTR_MASK);
	reg = glb_reg_read(S_INTR_STATUS);

	dla_trace("Exit: dla_isr_handler, reg:%x, mask:%x\n", reg, mask);
	RETURN(0);
}

*/

int32_t dla_isr_handler(void *engine_data, uint8_t dla_id)
{
    uint32_t mask;
    uint32_t reg;
    struct dla_processor *processor = NULL;
    struct dla_processor_group *group;
    struct dla_engine *engine = (struct dla_engine *)engine_data;
    struct nvdla_device *nvdla_dev = (struct nvdla_device *)engine->driver_context;

    nvdla_dev->current_dla_id = dla_id;

    dla_trace("\n================= NVDLA ISR ENTER =================\n");
    dla_trace("[ISR] dla_id = %u\n", dla_id);

    mask = glb_reg_read(S_INTR_MASK);
    reg  = glb_reg_read(S_INTR_STATUS);

    dla_trace("[ISR] RAW STATUS = 0x%x\n", reg);
    dla_trace("[ISR] MASK       = 0x%x\n", mask);
    dla_trace("[ISR] ACTIVE     = 0x%x (reg & ~mask)\n", reg & ~mask);

    // ---------------- CACC ----------------
    if (reg & MASK(GLB_S_INTR_STATUS_0, CACC_DONE_STATUS0)) {
        dla_trace("[ISR] CACC_DONE_STATUS0\n");
        processor = &engine->processors[dla_id][DLA_OP_CONV];
        group = &processor->groups[0];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);

    }

    if (reg & MASK(GLB_S_INTR_STATUS_0, CACC_DONE_STATUS1)) {
        dla_trace("[ISR] CACC_DONE_STATUS1\n");
        processor = &engine->processors[dla_id][DLA_OP_CONV];
        group = &processor->groups[1];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);

    }

    // ---------------- SDP ----------------
    if (reg & MASK(GLB_S_INTR_STATUS_0, SDP_DONE_STATUS0)) {
        dla_trace("[ISR] SDP_DONE_STATUS0\n");
        processor = &engine->processors[dla_id][DLA_OP_SDP];
        group = &processor->groups[0];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);
    }

    if (reg & MASK(GLB_S_INTR_STATUS_0, SDP_DONE_STATUS1)) {
        dla_trace("[ISR] SDP_DONE_STATUS1\n");
        processor = &engine->processors[dla_id][DLA_OP_SDP];
        group = &processor->groups[1];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);
    }

    // ---------------- PDP ----------------
    if (reg & MASK(GLB_S_INTR_STATUS_0, PDP_DONE_STATUS0)) {
        dla_trace("[ISR] PDP_DONE_STATUS0\n");
        processor = &engine->processors[dla_id][DLA_OP_PDP];
        group = &processor->groups[0];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);
    }

    if (reg & MASK(GLB_S_INTR_STATUS_0, PDP_DONE_STATUS1)) {
        dla_trace("[ISR] PDP_DONE_STATUS1\n");
        processor = &engine->processors[dla_id][DLA_OP_PDP];
        group = &processor->groups[1];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);
    }

    // ---------------- CDP ----------------
    if (reg & MASK(GLB_S_INTR_STATUS_0, CDP_DONE_STATUS0)) {
        dla_trace("[ISR] CDP_DONE_STATUS0\n");
        processor = &engine->processors[dla_id][DLA_OP_CDP];
        group = &processor->groups[0];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);
    }

    if (reg & MASK(GLB_S_INTR_STATUS_0, CDP_DONE_STATUS1)) {
        dla_trace("[ISR] CDP_DONE_STATUS1\n");
        processor = &engine->processors[dla_id][DLA_OP_CDP];
        group = &processor->groups[1];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);
    }

    // ---------------- BDMA ----------------
    if (reg & MASK(GLB_S_INTR_STATUS_0, BDMA_DONE_STATUS0)) {
        dla_trace("[ISR] BDMA_DONE_STATUS0\n");
        processor = &engine->processors[dla_id][DLA_OP_BDMA];
        group = &processor->groups[0];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);
    }

    if (reg & MASK(GLB_S_INTR_STATUS_0, BDMA_DONE_STATUS1)) {
        dla_trace("[ISR] BDMA_DONE_STATUS1\n");
        processor = &engine->processors[dla_id][DLA_OP_BDMA];
        group = &processor->groups[1];
        group->events |= (1 << DLA_EVENT_OP_COMPLETED);
    }

    // ---------------- CDMA ----------------
    if (reg & MASK(GLB_S_INTR_STATUS_0, CDMA_DAT_DONE_STATUS0)) {
        dla_trace("[ISR] CDMA_DAT_DONE_STATUS0\n");
        processor = &engine->processors[dla_id][DLA_OP_CONV];
        group = &processor->groups[0];
        group->events |= (1 << DLA_EVENT_CDMA_DT_DONE);
    }

    if (reg & MASK(GLB_S_INTR_STATUS_0, CDMA_DAT_DONE_STATUS1)) {
        dla_trace("[ISR] CDMA_DAT_DONE_STATUS1\n");
        processor = &engine->processors[dla_id][DLA_OP_CONV];
        group = &processor->groups[1];
        group->events |= (1 << DLA_EVENT_CDMA_DT_DONE);
    }

    if (reg & MASK(GLB_S_INTR_STATUS_0, CDMA_WT_DONE_STATUS0)) {
        dla_trace("[ISR] CDMA_WT_DONE_STATUS0\n");
        processor = &engine->processors[dla_id][DLA_OP_CONV];
        group = &processor->groups[0];
        group->events |= (1 << DLA_EVENT_CDMA_WT_DONE);
    }

    if (reg & MASK(GLB_S_INTR_STATUS_0, CDMA_WT_DONE_STATUS1)) {
        dla_trace("[ISR] CDMA_WT_DONE_STATUS1\n");
        processor = &engine->processors[dla_id][DLA_OP_CONV];
        group = &processor->groups[1];
        group->events |= (1 << DLA_EVENT_CDMA_WT_DONE);
    }

    // ---------------- CLEAR ----------------
    dla_trace("[ISR] Clearing interrupt: 0x%x\n", reg);
    glb_reg_write(S_INTR_STATUS, reg);

    mask = glb_reg_read(S_INTR_MASK);
    reg  = glb_reg_read(S_INTR_STATUS);

    dla_trace("[ISR] AFTER CLEAR STATUS = 0x%x\n", reg);
    dla_trace("[ISR] AFTER CLEAR MASK   = 0x%x\n", mask);

    dla_trace("================= NVDLA ISR EXIT =================\n\n");

    RETURN(0);
}