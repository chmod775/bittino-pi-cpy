#pragma once
#include "py/obj.h"
#include "py/proto.h"
#include "hardware/irq.h"

#ifdef BITTINO_DEBUG
#include <stdio.h>
#define BITTINO_DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define BITTINO_DEBUG_PRINT(...) (void) (0)
#endif

typedef struct {
    uint8_t count_relatime_in;
    uint8_t count_relatime_out;
    uint8_t *relatime_in;
    uint8_t *relatime_out;
} bittino_realtimes_t;

typedef bittino_realtimes_t* (*bittino_module_get_realtimes)(mp_obj_t);

typedef struct _bittino_module_p_t {
    MP_PROTOCOL_HEAD // MP_QSTR_protocol_bittino_module

    bittino_module_get_realtimes get_realtimes;
} bittino_module_p_t;

extern repeating_timer_t bittino_frame_timer;

extern bool bittino_comm_frame(repeating_timer_t *rt);
extern void bittino_master_write_registers(uint8_t slave_id, uint16_t address, uint8_t count, uint8_t *registers, bool skip_errors);