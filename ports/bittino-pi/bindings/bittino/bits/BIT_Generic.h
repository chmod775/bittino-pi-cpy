#pragma once
#include "py/obj.h"
#include "../Bittino.h"

extern const mp_obj_type_t bittino_BIT_Generic_type;

typedef struct {
    mp_obj_base_t base;
    uint8_t id;
    bittino_realtimes_t realtimes;
} bittino_bit_generic_obj_t;

void _bittino_BIT_Generic_init_from_args(
    bittino_bit_generic_obj_t *self,
    size_t n_args, size_t n_kw,
    const mp_obj_t *all_args
);