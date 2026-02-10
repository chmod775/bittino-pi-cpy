#pragma once
#include "py/obj.h"
#include "../../Bittino.h"
#include "../BIT_Generic.h"

extern const mp_obj_type_t bittino_BIT_COM_type;

#define BIT_COM_REG_send  100

#define BIT_COM_REG_tx_len  111
#define BIT_COM_REG_rx_len  112

#define BIT_COM_REG_tx_data  1000
#define BIT_COM_REG_rx_data  2000

typedef struct {
    bittino_bit_generic_obj_t super;
} bittino_bit_com_obj_t;

MP_STATIC_ASSERT(offsetof(bittino_bit_com_obj_t, super) == 0);