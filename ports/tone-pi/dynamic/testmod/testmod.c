// testmod.c — minimal dynruntime smoke test for CircuitPython
//
// Exercises exactly the three things the patches fix:
//   1. A direct fun_table call (mp_obj_new_str) to verify slot offsets
//   2. mp_raise_msg via the raise_msg_str macro to verify Patch B is a no-op
//   3. A small rodata constant to verify VIPERRODATA bit alignment (Patch C)
//
// If `import testmod; testmod.hello()` returns "world" on the board,
// your ABI is aligned and you can move on to your real module.
//
// If it hardfaults, capture PC/LR/SP from the fault frame and compare
// against the firmware .elf — the remaining mismatch is most likely in
// the fun_table ordering.

#include "py/dynruntime.h"

// A rodata string to force the linker to emit a VIPERRODATA section.
// If relocations are still broken, accessing this through mp_obj_new_str
// will land on garbage.
static const char greeting[] = "world";

static mp_obj_t hello(void) {
    return mp_obj_new_str(greeting, sizeof(greeting) - 1);
}
static MP_DEFINE_CONST_FUN_OBJ_0(hello_obj, hello);

// A function that raises, exercising the raise_msg_str path.
static mp_obj_t raise_test(void) {
    mp_raise_msg(&mp_type_ValueError, "testmod raise ok");
    return mp_obj_new_str(greeting, sizeof(greeting) - 1);
}
static MP_DEFINE_CONST_FUN_OBJ_0(raise_test_obj, raise_test);

// Simple arithmetic to exercise a fun_table call that returns a number.
static mp_obj_t add_one(mp_obj_t x_in) {
    mp_int_t x = mp_obj_get_int(x_in);
    return mp_obj_new_int(x + 1);
}
static MP_DEFINE_CONST_FUN_OBJ_1(add_one_obj, add_one);

mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    MP_DYNRUNTIME_INIT_ENTRY

    mp_store_global(MP_QSTR_hello, MP_OBJ_FROM_PTR(&hello_obj));
    mp_store_global(MP_QSTR_raise_test, MP_OBJ_FROM_PTR(&raise_test_obj));
    mp_store_global(MP_QSTR_add_one, MP_OBJ_FROM_PTR(&add_one_obj));

    MP_DYNRUNTIME_INIT_EXIT
}
