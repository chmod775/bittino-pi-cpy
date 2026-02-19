#include <stdio.h>
#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/proto.h"
#include "../../Bittino.h"
#include "../../__init__.h"
#include "BIT_COM.h"

static mp_obj_t bittino_BIT_COM_make_new(const mp_obj_type_t *type,
                                size_t n_args, size_t n_kw,
                                const mp_obj_t *all_args) {

    bittino_bit_com_obj_t *self = mp_obj_malloc(bittino_bit_com_obj_t, &bittino_BIT_COM_type);
    _bittino_BIT_Generic_init_from_args(&self->super, 1, 1);

    return MP_OBJ_FROM_PTR(self);
}


static mp_obj_t bittino_BIT_COM_send(mp_obj_t self_in, mp_obj_t address_in, mp_obj_t data_in) {
    bittino_bit_com_obj_t *self = (bittino_bit_com_obj_t *)self_in;
    if (!mp_obj_is_type(data_in, &mp_type_list) && !mp_obj_is_type(data_in, &mp_type_tuple)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected list/tuple"));
    }

    mp_int_t address_int = mp_obj_get_int(address_in);

    size_t data_count;
    mp_obj_t *data_items;
    mp_obj_get_array(data_in, &data_count, &data_items);

    uint8_t data[256];
    for (size_t i = 0; i < data_count; i++) {
        data[i] = mp_obj_get_int(data_items[i]);
    }

    bittino_master_write_registers(self->super.id, BIT_COM_REG_tx_len, 1, &(uint8_t){ data_count });
    bittino_master_write_registers(self->super.id, BIT_COM_REG_tx_data, data_count, data);
    bittino_master_write_registers(self->super.id, BIT_COM_REG_send, 1, &(uint8_t){ address_int });

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(bittino_BIT_COM_send_obj, bittino_BIT_COM_send);


static const mp_rom_map_elem_t bittino_BIT_COM_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&bittino_BIT_COM_send_obj) },
};
static MP_DEFINE_CONST_DICT(bittino_BIT_COM_locals_dict, bittino_BIT_COM_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    bittino_BIT_COM_type,
    MP_QSTR_BIT_COM,
    MP_TYPE_FLAG_NONE,
    locals_dict, &bittino_BIT_COM_locals_dict,
    make_new, bittino_BIT_COM_make_new,
    parent, &bittino_BIT_Generic_type
);
