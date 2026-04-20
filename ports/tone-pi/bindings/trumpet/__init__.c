#include "__init__.h"
#include "bindings/trumpet/tr_firmware.h"


static mp_obj_t trumpet_init(void) {
    TR_Init();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(trumpet_init_obj, trumpet_init);


static mp_obj_t trumpet_step(void) {
    TR_Step();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(trumpet_step_obj, trumpet_step);



static const mp_rom_map_elem_t trumpet_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_trumpet) },

    { MP_ROM_QSTR(MP_QSTR_init),  MP_ROM_PTR(&trumpet_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_step),  MP_ROM_PTR(&trumpet_step_obj) },
};

static MP_DEFINE_CONST_DICT(trumpet_module_globals, trumpet_module_globals_table);

const mp_obj_module_t trumpet_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&trumpet_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_trumpet, trumpet_module);
