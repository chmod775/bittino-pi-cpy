#pragma once
#include "py/obj.h"
#include "../../Bittino.h"
#include "../BIT_Generic.h"

extern const mp_obj_type_t bittino_BIT_IO_type;

typedef struct {
    bittino_bit_generic_obj_t super;
} bittino_bit_io_obj_t;

MP_STATIC_ASSERT(offsetof(bittino_bit_io_obj_t, super) == 0);
MP_STATIC_ASSERT(offsetof(bittino_bit_generic_obj_t, base) == 0);