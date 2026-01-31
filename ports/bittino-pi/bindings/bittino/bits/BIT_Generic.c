#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/proto.h"
#include "../Bittino.h"
#include "BIT_Generic.h"

static mp_obj_t bittino_BIT_Generic_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_in_count, ARG_out_count };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_in_count, MP_ARG_INT | MP_ARG_REQUIRED | MP_ARG_KW_ONLY },
        { MP_QSTR_out_count, MP_ARG_INT | MP_ARG_REQUIRED | MP_ARG_KW_ONLY },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t in_count = mp_arg_validate_int_min(args[ARG_in_count].u_int, 1, MP_QSTR_in_count);
    mp_int_t out_count = mp_arg_validate_int_min(args[ARG_out_count].u_int, 1, MP_QSTR_out_count);

    bittino_bit_generic_obj_t *self = mp_obj_malloc(bittino_bit_generic_obj_t, &bittino_BIT_Generic_type);

    self->realtimes.count_relatime_in = in_count;
    self->realtimes.count_relatime_out = out_count;
    
    self->realtimes.relatime_in = port_malloc(in_count, false);
    self->realtimes.relatime_out = port_malloc(out_count, false);

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

static bittino_realtimes_t* bittino_BIT_Generic_get_realtimes_proto(mp_obj_t self_in) {
    bittino_bit_generic_obj_t *self = (bittino_bit_generic_obj_t *)self_in;
    return &self->realtimes;
}


static const bittino_module_p_t bittino_BIT_Generic_proto = {
    MP_PROTO_IMPLEMENT(MP_QSTR_protocol_bittino)
    .get_realtimes = bittino_BIT_Generic_get_realtimes_proto
};

static const mp_rom_map_elem_t bittino_BIT_Generic_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&bittino_BIT_Generic_deinit_obj) },
};
static MP_DEFINE_CONST_DICT(bittino_BIT_Generic_locals_dict, bittino_BIT_Generic_locals_dict_table);


MP_DEFINE_CONST_OBJ_TYPE(
    bittino_BIT_Generic_type,
    MP_QSTR_BIT_Generic,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS,
    locals_dict, &bittino_BIT_Generic_locals_dict,
    make_new, bittino_BIT_Generic_make_new,
    protocol, &bittino_BIT_Generic_proto
);
