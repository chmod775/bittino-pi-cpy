#include <stdio.h>
#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/proto.h"
#include "../../Bittino.h"
#include "BIT_IO.h"

static mp_obj_t bittino_BIT_IO_make_new(const mp_obj_type_t *type,
                                size_t n_args, size_t n_kw,
                                const mp_obj_t *all_args) {

    bittino_bit_io_obj_t *self = mp_obj_malloc(bittino_bit_io_obj_t, &bittino_BIT_IO_type);
    _bittino_BIT_Generic_init_from_args(&self->super, 1, 1);
    return MP_OBJ_FROM_PTR(self);
}


static mp_obj_t bittino_BIT_IO_set(mp_obj_t self_in, mp_obj_t value) {
    bittino_bit_io_obj_t *self = (bittino_bit_io_obj_t *)self_in;
    mp_int_t value_int = mp_obj_get_int(value);

    self->super.realtimes.relatime_in[0] = value_int;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(bittino_BIT_IO_set_obj, bittino_BIT_IO_set);


static const mp_rom_map_elem_t bittino_BIT_IO_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_set), MP_ROM_PTR(&bittino_BIT_IO_set_obj) },
};
static MP_DEFINE_CONST_DICT(bittino_BIT_IO_locals_dict, bittino_BIT_IO_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    bittino_BIT_IO_type,
    MP_QSTR_BIT_IO,
    MP_TYPE_FLAG_NONE,
    locals_dict, &bittino_BIT_IO_locals_dict,
    make_new, bittino_BIT_IO_make_new,
    parent, &bittino_BIT_Generic_type
);
