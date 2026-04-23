#include "py/dynruntime.h"

typedef struct {
    uint16_t ID;
    uint32_t PC;
    uint32_t STACK_PTR;
    uint32_t FLAGS;
} VM_TaskInstance;

// Globals (no static, no initializer — BSS rule)
mp_obj_t *vm_stack;
void (**block_handlers)(VM_TaskInstance *task, uint8_t *payload);

static inline mp_obj_t  SP_pop     (uint32_t *sp)                    { return vm_stack[--(*sp)]; }
static inline void      SP_push    (uint32_t *sp, mp_obj_t v)        { vm_stack[(*sp)++] = v; }
static inline mp_obj_t  SP_peek    (uint32_t sp, uint32_t off)       { return vm_stack[sp - off]; }
static inline void SP_write(uint32_t sp, uint32_t offset, mp_obj_t value) { vm_stack[sp - offset - 1] = value; }

// Raw uint32 — no GC, direct bit cast (PC values, literals, offsets)
static inline uint32_t  SP_pop_u32 (uint32_t *sp)                    { return (uint32_t)(uintptr_t)vm_stack[--(*sp)]; }
static inline void      SP_push_u32(uint32_t *sp, uint32_t v)        { vm_stack[(*sp)++] = (mp_obj_t)(uintptr_t)v; }
static inline uint32_t  SP_peek_u32(uint32_t sp, uint32_t off)       { return (uint32_t)(uintptr_t)vm_stack[sp - off]; }
static inline void SP_write_u32(uint32_t sp, uint32_t offset, uint32_t value) { vm_stack[sp - offset - 1] = (mp_obj_t)(uintptr_t)value; }
/* ################## */

// Block implementation
static void BLOCK_Math_Add(VM_TaskInstance *task, uint8_t *payload) {
    mp_obj_t n1 = vm_stack[--task->STACK_PTR];
    mp_obj_t n2 = vm_stack[--task->STACK_PTR];
    vm_stack[task->STACK_PTR++] = mp_binary_op(MP_BINARY_OP_ADD, n2, n1);
}

static void BLOCK_String_PadStart(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_int_t total_size = (mp_int_t)payload[0];
    mp_obj_t pad_obj    = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t src_obj    = SP_pop(&task_instance->STACK_PTR);
    size_t src_len, pad_len;
    const char *src_str = mp_obj_str_get_data(src_obj, &src_len);
    const char *pad_str = mp_obj_str_get_data(pad_obj, &pad_len);
    mp_int_t padding    = total_size - (mp_int_t)src_len;
    if (padding < 0) padding = 0;
    vstr_t vstr;
    vstr_init(&vstr, total_size);
    for (mp_int_t i = 0; i < padding; i++)
        vstr_add_char(&vstr, pad_len > 0 ? pad_str[i % pad_len] : ' ');
    vstr_add_strn(&vstr, src_str, src_len);
    SP_push(&task_instance->STACK_PTR, mp_obj_new_str_from_vstr(&vstr));
}

// Register function: takes the two pointers and the opcode from Python
static mp_obj_t register_add(mp_obj_t handlers_ptr, mp_obj_t stack_ptr, mp_obj_t opcode_obj) {
    block_handlers = (void (**)(VM_TaskInstance*, uint8_t*)) mp_obj_get_int(handlers_ptr);
    vm_stack       = (mp_obj_t*)                 mp_obj_get_int(stack_ptr);
    uint8_t opcode = (uint8_t)                   mp_obj_get_int(opcode_obj);

    block_handlers[opcode] = BLOCK_Math_Add;
    block_handlers[opcode + 1] = BLOCK_String_PadStart;

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(register_add_obj, register_add);

// Module init
mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    MP_DYNRUNTIME_INIT_ENTRY
    mp_store_global(MP_QSTR_register_add, MP_OBJ_FROM_PTR(&register_add_obj));
    MP_DYNRUNTIME_INIT_EXIT
}