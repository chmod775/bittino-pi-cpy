#include "py/dynruntime.h"

static mp_obj_t add(mp_obj_t a, mp_obj_t b) {
    return mp_obj_new_int(mp_obj_get_int(a) + mp_obj_get_int(b));
}
static MP_DEFINE_CONST_FUN_OBJ_2(add_obj, add);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    MP_DYNRUNTIME_INIT_ENTRY
    mp_store_global(MP_QSTR_add, MP_OBJ_FROM_PTR(&add_obj));
    MP_DYNRUNTIME_INIT_EXIT
}