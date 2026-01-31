#pragma once
#include "py/obj.h"
#include "../Bittino.h"

extern const mp_obj_type_t bittino_BIT_Generic_type;

typedef struct {
    mp_obj_base_t base;
    uint8_t id;
    bittino_realtimes_t realtimes;
} bittino_bit_generic_obj_t;