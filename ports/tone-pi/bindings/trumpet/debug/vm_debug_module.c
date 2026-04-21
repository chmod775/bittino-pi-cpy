// vm_debug_module.c — import vm_debug from CircuitPython REPL
//
// Usage:
//   import vm_debug
//   vm_debug.enable(-1)
//   vm_debug.op_detail(True)       # ← new: show func names + payloads
//   vm_debug.step_mode(True)
//   vm_debug.add_pc_break(0x0042)
//   vm_debug.add_op_break(0x88)
//   vm_debug.step()
//   vm_debug.inspect_pc()
//   vm_debug.inspect_stack()
//   vm_debug.inspect_tasks()
//   vm_debug.list_breaks()
//   vm_debug.stack_on_pause(True)
//   vm_debug.disable()

#include "py/obj.h"
#include "py/runtime.h"
#include "vm_debug.h"

static mp_obj_t mp_vm_debug_enable(mp_obj_t task_filter_obj) {
    vm_debug_enable((int8_t)mp_obj_get_int(task_filter_obj));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_vm_debug_enable_obj, mp_vm_debug_enable);

static mp_obj_t mp_vm_debug_disable(void) {
    vm_debug_disable();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_vm_debug_disable_obj, mp_vm_debug_disable);

static mp_obj_t mp_vm_debug_step_mode(mp_obj_t enabled_obj) {
    if (mp_obj_is_true(enabled_obj)) vm_debug_step_mode_enable();
    else                             vm_debug_step_mode_disable();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_vm_debug_step_mode_obj, mp_vm_debug_step_mode);

static mp_obj_t mp_vm_debug_step(void) {
    vm_debug_step();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_vm_debug_step_obj, mp_vm_debug_step);

// ── vm_debug.op_detail(enabled: bool) ────────────────────────────────────────
static mp_obj_t mp_vm_debug_add_pc_break(mp_obj_t pc_obj) {
    vm_debug_add_pc_break((uint32_t)mp_obj_get_int(pc_obj));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_vm_debug_add_pc_break_obj, mp_vm_debug_add_pc_break);

static mp_obj_t mp_vm_debug_remove_pc_break(mp_obj_t pc_obj) {
    vm_debug_remove_pc_break((uint32_t)mp_obj_get_int(pc_obj));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_vm_debug_remove_pc_break_obj, mp_vm_debug_remove_pc_break);

static mp_obj_t mp_vm_debug_add_op_break(mp_obj_t op_obj) {
    vm_debug_add_op_break((uint8_t)mp_obj_get_int(op_obj));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_vm_debug_add_op_break_obj, mp_vm_debug_add_op_break);

static mp_obj_t mp_vm_debug_remove_op_break(mp_obj_t op_obj) {
    vm_debug_remove_op_break((uint8_t)mp_obj_get_int(op_obj));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_vm_debug_remove_op_break_obj, mp_vm_debug_remove_op_break);

static mp_obj_t mp_vm_debug_clear_pc_breaks(void) {
    vm_debug_clear_pc_breaks();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_vm_debug_clear_pc_breaks_obj, mp_vm_debug_clear_pc_breaks);

static mp_obj_t mp_vm_debug_clear_op_breaks(void) {
    vm_debug_clear_op_breaks();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_vm_debug_clear_op_breaks_obj, mp_vm_debug_clear_op_breaks);

static mp_obj_t mp_vm_debug_list_breaks(void) {
    vm_debug_list_breaks();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_vm_debug_list_breaks_obj, mp_vm_debug_list_breaks);

static mp_obj_t mp_vm_debug_inspect_pc(void) {
    vm_debug_inspect_pc();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_vm_debug_inspect_pc_obj, mp_vm_debug_inspect_pc);

static mp_obj_t mp_vm_debug_inspect_stack(void) {
    vm_debug_inspect_stack();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_vm_debug_inspect_stack_obj, mp_vm_debug_inspect_stack);

static mp_obj_t mp_vm_debug_inspect_tasks(void) {
    vm_debug_inspect_all_tasks();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_vm_debug_inspect_tasks_obj, mp_vm_debug_inspect_tasks);

static mp_obj_t mp_vm_debug_stack_on_pause(mp_obj_t enabled_obj) {
    vm_debug.stack_on_pause = mp_obj_is_true(enabled_obj);
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] stack_on_pause = %s\n",
        vm_debug.stack_on_pause ? "true" : "false");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_vm_debug_stack_on_pause_obj, mp_vm_debug_stack_on_pause);

// ── Module table ──────────────────────────────────────────────────────────────
static const mp_rom_map_elem_t vm_debug_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_vm_debug)              },
    { MP_ROM_QSTR(MP_QSTR_enable),          MP_ROM_PTR(&mp_vm_debug_enable_obj)        },
    { MP_ROM_QSTR(MP_QSTR_disable),         MP_ROM_PTR(&mp_vm_debug_disable_obj)       },
    { MP_ROM_QSTR(MP_QSTR_step_mode),       MP_ROM_PTR(&mp_vm_debug_step_mode_obj)     },
    { MP_ROM_QSTR(MP_QSTR_step),            MP_ROM_PTR(&mp_vm_debug_step_obj)          },
    { MP_ROM_QSTR(MP_QSTR_add_pc_break),    MP_ROM_PTR(&mp_vm_debug_add_pc_break_obj)  },
    { MP_ROM_QSTR(MP_QSTR_remove_pc_break), MP_ROM_PTR(&mp_vm_debug_remove_pc_break_obj)},
    { MP_ROM_QSTR(MP_QSTR_add_op_break),    MP_ROM_PTR(&mp_vm_debug_add_op_break_obj)  },
    { MP_ROM_QSTR(MP_QSTR_remove_op_break), MP_ROM_PTR(&mp_vm_debug_remove_op_break_obj)},
    { MP_ROM_QSTR(MP_QSTR_clear_pc_breaks), MP_ROM_PTR(&mp_vm_debug_clear_pc_breaks_obj)},
    { MP_ROM_QSTR(MP_QSTR_clear_op_breaks), MP_ROM_PTR(&mp_vm_debug_clear_op_breaks_obj)},
    { MP_ROM_QSTR(MP_QSTR_list_breaks),     MP_ROM_PTR(&mp_vm_debug_list_breaks_obj)   },
    { MP_ROM_QSTR(MP_QSTR_inspect_pc),      MP_ROM_PTR(&mp_vm_debug_inspect_pc_obj)    },
    { MP_ROM_QSTR(MP_QSTR_inspect_stack),   MP_ROM_PTR(&mp_vm_debug_inspect_stack_obj) },
    { MP_ROM_QSTR(MP_QSTR_inspect_tasks),   MP_ROM_PTR(&mp_vm_debug_inspect_tasks_obj) },
    { MP_ROM_QSTR(MP_QSTR_stack_on_pause),  MP_ROM_PTR(&mp_vm_debug_stack_on_pause_obj)},
};
static MP_DEFINE_CONST_DICT(vm_debug_module_globals, vm_debug_module_globals_table);

const mp_obj_module_t vm_debug_module = {
    .base    = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&vm_debug_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_vm_debug, vm_debug_module);