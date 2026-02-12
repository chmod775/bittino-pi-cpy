#pragma once
#include "py/obj.h"
#include "../Bittino.h"

extern const mp_obj_type_t bittino_BIT_Generic_type;

typedef struct {
    mp_obj_base_t base;
    uint8_t id;
    uint32_t type;
    bittino_realtimes_t realtimes;
} bittino_bit_generic_obj_t;

void _bittino_BIT_Generic_init_from_args(
    bittino_bit_generic_obj_t *self,
    mp_int_t in_count,
    mp_int_t out_count
);

MP_STATIC_ASSERT(offsetof(bittino_bit_generic_obj_t, base) == 0);
