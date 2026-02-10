#include <stdio.h>
#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/proto.h"
#include "../Bittino.h"
#include "BIT_Generic.h"

void _bittino_BIT_Generic_init_from_args(
    bittino_bit_generic_obj_t *self,
    mp_int_t in_count,
    mp_int_t out_count
) {
    printf("\tptr - _bittino_BIT_Generic_init_from_args: %p\n", self);
    printf("ins: %d, outs: %d\n", in_count, out_count);

    self->id = 0;

    self->realtimes.count_relatime_in = in_count;
    self->realtimes.count_relatime_out = out_count;

    self->realtimes.relatime_in = port_malloc(in_count, false);
    self->realtimes.relatime_out = port_malloc(out_count, false);
}

static mp_obj_t bittino_BIT_Generic_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    bittino_bit_generic_obj_t *self = mp_obj_malloc(bittino_bit_generic_obj_t, &bittino_BIT_Generic_type);

    enum { ARG_in_count, ARG_out_count };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_in_count, MP_ARG_INT | MP_ARG_REQUIRED | MP_ARG_KW_ONLY },
        { MP_QSTR_out_count, MP_ARG_INT | MP_ARG_REQUIRED | MP_ARG_KW_ONLY },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t in_count = mp_arg_validate_int_min(args[ARG_in_count].u_int, 1, MP_QSTR_in_count);
    mp_int_t out_count = mp_arg_validate_int_min(args[ARG_out_count].u_int, 1, MP_QSTR_out_count);

    printf("\tptr - bittino_BIT_Generic_make_new: %p\n", self);
    _bittino_BIT_Generic_init_from_args(self, in_count, out_count);
    return MP_OBJ_FROM_PTR(self);
}

//|     def deinit(self) -> None:
//|         """"""
//|         ...
//|
static mp_obj_t bittino_BIT_Generic_deinit(mp_obj_t self_in) {
    bittino_bit_generic_obj_t *self = (bittino_bit_generic_obj_t *)self_in;
    port_free((void *)self->realtimes.relatime_in);
    port_free((void *)self->realtimes.relatime_out);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(bittino_BIT_Generic_deinit_obj, bittino_BIT_Generic_deinit);

static mp_obj_t bittino_BIT_Generic_write(mp_obj_t self_in, mp_obj_t seq_in) {
    bittino_bit_generic_obj_t *self = (bittino_bit_generic_obj_t *)self_in;
    if (!mp_obj_is_type(seq_in, &mp_type_list) && !mp_obj_is_type(seq_in, &mp_type_tuple)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected list/tuple"));
    }

    size_t n;
    mp_obj_t *items;
    mp_obj_get_array(seq_in, &n, &items);

    for (size_t i = 0; i < n; i++) {
        uint8_t value = mp_obj_get_int(items[i]);
        self->realtimes.relatime_in[i] = value;
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(bittino_BIT_Generic_write_obj, bittino_BIT_Generic_write);

static mp_obj_t bittino_BIT_Generic_read(mp_obj_t self_in) {
    bittino_bit_generic_obj_t *self = (bittino_bit_generic_obj_t *)self_in;
    return mp_obj_new_int(self->realtimes.relatime_out[0]);
}
static MP_DEFINE_CONST_FUN_OBJ_1(bittino_BIT_Generic_read_obj, bittino_BIT_Generic_read);

static const mp_rom_map_elem_t bittino_BIT_Generic_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&bittino_BIT_Generic_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&bittino_BIT_Generic_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&bittino_BIT_Generic_read_obj) },
};
static MP_DEFINE_CONST_DICT(bittino_BIT_Generic_locals_dict, bittino_BIT_Generic_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    bittino_BIT_Generic_type,
    MP_QSTR_BIT_Generic,
    MP_TYPE_FLAG_NONE,
    locals_dict, &bittino_BIT_Generic_locals_dict,
    make_new, bittino_BIT_Generic_make_new
);
