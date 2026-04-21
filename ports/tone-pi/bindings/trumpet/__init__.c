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

static mp_obj_t trumpet_load(mp_obj_t hex_str_obj) {
    size_t str_len;
    const char *str = mp_obj_str_get_data(hex_str_obj, &str_len);

    size_t count = 0;
    const char *p = str;
    const char *end = str + str_len;

    while (p < end) {
        // skip spaces
        while (p < end && *p == ' ') p++;
        if (p >= end) break;

        if (count >= CODE_SIZE) {
            mp_raise_ValueError(MP_ERROR_TEXT("firmware too large"));
        }

        // parse two hex digits
        uint8_t t_byte = 0;
        for (int i = 0; i < 2; i++, p++) {
            t_byte <<= 4;
            if      (*p >= '0' && *p <= '9') t_byte |= *p - '0';
            else if (*p >= 'a' && *p <= 'f') t_byte |= *p - 'a' + 10;
            else if (*p >= 'A' && *p <= 'F') t_byte |= *p - 'A' + 10;
            else mp_raise_ValueError(MP_ERROR_TEXT("invalid hex string"));
        }

        vm_code_buffer[count++] = t_byte;
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(trumpet_load_obj, trumpet_load);


static mp_obj_t trumpet_set_task(mp_obj_t index_obj, mp_obj_t pc_obj, mp_obj_t stack_ptr_obj) {
    mp_int_t index     = mp_obj_get_int(index_obj);
    mp_int_t pc        = mp_obj_get_int(pc_obj);
    mp_int_t stack_ptr = mp_obj_get_int(stack_ptr_obj);

    if (index < 0 || index >= TASKS_MAX) {
        mp_raise_ValueError(MP_ERROR_TEXT("task index out of range"));
    }

    vm_task_initializers[index].START_PC        = (uint32_t)pc;
    vm_task_initializers[index].START_STACK_PTR = (uint32_t)stack_ptr;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(trumpet_set_task_obj, trumpet_set_task);

static const mp_rom_map_elem_t trumpet_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_trumpet) },

    { MP_ROM_QSTR(MP_QSTR_load),     MP_ROM_PTR(&trumpet_load_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_task), MP_ROM_PTR(&trumpet_set_task_obj) },

    { MP_ROM_QSTR(MP_QSTR_init),  MP_ROM_PTR(&trumpet_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_step),  MP_ROM_PTR(&trumpet_step_obj) },
};

static MP_DEFINE_CONST_DICT(trumpet_module_globals, trumpet_module_globals_table);

const mp_obj_module_t trumpet_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&trumpet_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_trumpet, trumpet_module);
