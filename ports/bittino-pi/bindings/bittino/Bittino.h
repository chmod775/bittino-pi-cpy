#pragma once
#include "py/obj.h"
#include "py/proto.h"

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