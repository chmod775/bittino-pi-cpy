/* ##################################################################################### */
/* ###                             CODE GENERATED USING                              ### */
/* ###    ______   ______     __  __     __    __     ______   ______     ______     ### */
/* ###   /\__  _\ /\  == \   /\ \/\ \   /\ "-./  \   /\  == \ /\  ___\   /\__  _\    ### */
/* ###   \/_/\ \/ \ \  __<   \ \ \_\ \  \ \ \-./\ \  \ \  _-/ \ \  __\   \/_/\ \/    ### */
/* ###      \ \_\  \ \_\ \_\  \ \_____\  \ \_\ \ \_\  \ \_\    \ \_____\    \ \_\    ### */
/* ###       \/_/   \/_/ /_/   \/_____/   \/_/  \/_/   \/_/     \/_____/     \/_/    ### */
/* ###                                                                               ### */
/* ###   by AlienLogic (Michele Trombetta)                                           ### */
/* ##################################################################################### */
/* ###   Built using:                                                                ### */
/* ###     - TrumpeT v1.0.0 pre                                                      ### */
/* ###     - VM v1.0.0 pre                                                           ### */   
/* ###   Usage:                                                                      ### */
/* ###     - Stack: 32 Bytes                                                         ### */
/* ###     - Code: 169 Bytes                                                         ### */
/* ###     - Tasks: 1                                                                ### */
/* ##################################################################################### */
/* ###   Libraries:                                                                  ### */
/* ##################################################################################### */

#include "tr_firmware.h"
#include "debug/vm_debug.h"

/* ##### Stack ##### */
mp_obj_t vm_stack[STACK_SIZE];

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

/* ##### Arguments helpers ##### */
// static float VM_ReadArg_Value(uint32_t arg_raw) {
//   union { uint32_t raw; float value; } data;
//   data.raw = arg_raw;
//   return data.value;
// }
// static int32_t VM_ReadArg_Int(uint32_t arg_raw) {
//   return (int32_t)arg_raw;
// }
/* ############################ */


VM_TaskInstance vm_task_instances[TASKS_MAX];

static void VM_InitTask(uint8_t task_index, uint32_t PC, uint32_t STACK_PTR) {
  VM_TaskInstance *task_instance = &vm_task_instances[task_index];
  task_instance->PC = PC;
  task_instance->STACK_PTR = STACK_PTR;
  task_instance->FLAGS.enabled = true;
  task_instance->FLAGS.is_first_start = true;
}

// static void VM_StopTask(uint8_t task_index) {
//   VM_TaskInstance *task_instance = &vm_task_instances[task_index];
//   task_instance->FLAGS.enabled = false;
// }


#define likely(x)   __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)

#define BC8(p)  (vm_code_buffer[p])

static inline uint8_t  FETCH8 (VM_TaskInstance *task_instance, uint32_t *pc){
  return vm_code_buffer[(*pc)++];
}
static inline uint16_t FETCH16(VM_TaskInstance *task_instance, uint32_t *pc){
  uint32_t a = *pc;
  uint16_t v = (uint16_t)vm_code_buffer[a]
             | ((uint16_t)vm_code_buffer[a + 1] << 8);
  *pc += 2;
  return v;
}
static inline uint32_t FETCH32(VM_TaskInstance *task_instance, uint32_t *pc){
  uint32_t a = *pc;
  uint32_t v =  (uint32_t)vm_code_buffer[a]
              | ((uint32_t)vm_code_buffer[a + 1] << 8)
              | ((uint32_t)vm_code_buffer[a + 2] << 16)
              | ((uint32_t)vm_code_buffer[a + 3] << 24);
  *pc += 4;
  return v;
}

static bool VM_ExecuteTask_Step(VM_TaskInstance *task_instance)
{
  static void *dispatch_table[256];
  dispatch_table[NOP]           = &&op_NOP;
  dispatch_table[END]           = &&op_END;
  dispatch_table[LITERAL_UINT32]= &&op_LITERAL_U32;
  dispatch_table[GOTO]          = &&op_GOTO;
  dispatch_table[POP]           = &&op_POP;
  dispatch_table[PUSH]          = &&op_PUSH;
  dispatch_table[READ_ARG]      = &&op_READ_ARG;
  dispatch_table[EXE]           = &&op_EXE;
  dispatch_table[EXE_ARGS]      = &&op_EXE_ARGS;
  dispatch_table[GROUP_END]     = &&op_GROUP_END;
  dispatch_table[MACRO_CALL]    = &&op_MACRO_CALL;
  dispatch_table[MACRO_RETURN]  = &&op_MACRO_RETURN;
  dispatch_table[SLOT_BODY]     = &&op_SLOT_BODY;
  dispatch_table[SLOT_CALL]     = &&op_SLOT_CALL;
  dispatch_table[SLOT_RETURN]   = &&op_SLOT_RETURN;
  dispatch_table[POINT_OFFSET]  = &&op_POINT_OFFSET;

  #ifdef DEBUG_TOOLS_ENABLED
    #define DISPATCH() do {                                     \
        uint8_t _op = BC8(task_instance->PC);                  \
        if (_vm_debug_check(task_instance->ID,                  \
                            task_instance->PC,                  \
                            _op,                                \
                            task_instance->STACK_PTR))          \
            return false;                                       \
        task_instance->PC++;                                    \
        goto *dispatch_table[_op] ?: &&op_DEFAULT;             \
    } while(0)
  #else
    #define DISPATCH() do {                                     \
        uint8_t _op = BC8(task_instance->PC);                  \
        task_instance->PC++;                                    \
        goto *dispatch_table[_op] ?: &&op_DEFAULT;             \
    } while(0)
  #endif

  #define COMPLETE() do {                                         \
      return false;                                               \
  } while(0)

  #define _DBG_FNAME(fn, idx, buf, bufsz)                              \
      ((fn)->kind == VM_FUNC_PY                                        \
          ? (snprintf((buf), (bufsz), "%s.%s",                         \
                      (fn)->py_module ? (fn)->py_module : "?",         \
                      (fn)->py_name   ? (fn)->py_name   : "?"), (buf)) \
          : ((fn)->c_name                                               \
              ? (fn)->c_name                                            \
              : (snprintf((buf), (bufsz), "c_func#%d", (idx)), (buf))))
 

  DISPATCH();

op_NOP:
  COMPLETE();

op_END:
  return true;

op_LITERAL_U32: {
  uint32_t x = FETCH32(task_instance, &task_instance->PC);
  SP_push_u32(&task_instance->STACK_PTR, x);
  COMPLETE();
}

op_GOTO: {
  task_instance->PC = FETCH32(task_instance, &task_instance->PC);
  COMPLETE();
}

op_POP: {
  (void)SP_pop(&task_instance->STACK_PTR);
  COMPLETE();
}

op_PUSH: {
  uint16_t size = FETCH16(task_instance, &task_instance->PC);
  for (uint32_t i = 0; i < size; ++i)
    SP_push_u32(&task_instance->STACK_PTR, 0);
  COMPLETE();
}

op_READ_ARG: {
  uint32_t off = FETCH32(task_instance, &task_instance->PC);
  uint32_t v   = SP_peek_u32(task_instance->STACK_PTR, off);
  SP_push_u32(&task_instance->STACK_PTR, v + off);
  COMPLETE();
}

op_EXE: {
  uint16_t idx = FETCH16(task_instance, &task_instance->PC);
  VM_FuncEntry *fn = &vm_functions[idx];
  if (fn->kind == VM_FUNC_C) {
    fn->c_func(task_instance, NULL);
  } else {
    mp_obj_t result = mp_call_function_n_kw(fn->py_callable, 0, 0, NULL);
    if (result != mp_const_none)
      SP_push(&task_instance->STACK_PTR, result);
  }
  COMPLETE();
}

op_EXE_ARGS: {
  uint8_t  argc = FETCH8 (task_instance, &task_instance->PC);
  uint16_t idx  = FETCH16(task_instance, &task_instance->PC);
  VM_FuncEntry *fn = &vm_functions[idx];

  uint32_t fetched_pc = task_instance->PC;
  task_instance->PC += argc - 2;

  if (fn->kind == VM_FUNC_C) {
    fn->c_func(task_instance, &vm_code_buffer[fetched_pc]);
  } else {
    mp_obj_t py_args[fn->stack_argc];
    for (int i = fn->stack_argc - 1; i >= 0; i--)
      py_args[i] = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t result = mp_call_function_n_kw(fn->py_callable, fn->stack_argc, 0, py_args);
    if (result != mp_const_none)
      SP_push(&task_instance->STACK_PTR, result);
  }
  
  COMPLETE();
}

op_GROUP_END: {
  uint16_t count = FETCH16(task_instance, &task_instance->PC);
  task_instance->STACK_PTR -= count;
  COMPLETE();
}

op_MACRO_CALL: {
  uint32_t new_pc = FETCH32(task_instance, &task_instance->PC);
  SP_push_u32(&task_instance->STACK_PTR, task_instance->PC);
  task_instance->PC = new_pc;
  COMPLETE();
}

op_MACRO_RETURN: {
  task_instance->PC = SP_pop_u32(&task_instance->STACK_PTR);
  COMPLETE();
}

op_SLOT_BODY: {
  uint32_t addr = FETCH32(task_instance, &task_instance->PC);
  SP_push_u32(&task_instance->STACK_PTR, addr);
  COMPLETE();
}

op_SLOT_CALL: {
  uint32_t off    = FETCH32(task_instance, &task_instance->PC);
  uint32_t new_pc = SP_peek_u32(task_instance->STACK_PTR, off);
  SP_push_u32(&task_instance->STACK_PTR, task_instance->PC);
  task_instance->PC = new_pc;
  COMPLETE();
}

op_SLOT_RETURN: {
  task_instance->PC = SP_pop_u32(&task_instance->STACK_PTR);
  COMPLETE();
}

op_POINT_OFFSET: {
  uint16_t off  = FETCH16(task_instance, &task_instance->PC);
  uint32_t addr = SP_pop_u32(&task_instance->STACK_PTR);
  SP_push_u32(&task_instance->STACK_PTR, addr - off);
  COMPLETE();
}

op_DEFAULT: {
  // Unknown opcode — halt and report if debug is on, hard-hang otherwise
  if (vm_debug.enabled) {
      uint8_t bad_op = vm_code_buffer[task_instance->PC - 1];
      mp_printf(MP_PYTHON_PRINTER,
          "[VM DBG] !! UNKNOWN OPCODE 0x%02X at PC=0x%08lX (task=%u) !!\n",
          bad_op,
          (unsigned long)(task_instance->PC - 1),
          task_instance->ID);
      // Park the task so inspection is possible from the REPL
      vm_debug.paused         = true;
      vm_debug.paused_task_id = task_instance->ID;
      vm_debug.paused_pc      = task_instance->PC - 1;
      vm_debug.paused_opcode  = bad_op;
      return false;
  }
  while(1); // original behaviour when debug is off
}
}

bool run_mode = true;
enum VM_Status actual_status = IDLE;

void TR_Init(void) {
  actual_status = INITIALIZING;

  for (uint32_t i = 0; i < FUNCTIONS_MAX; i++) {
    VM_FuncEntry *fn = &vm_functions[i];
    if (fn->kind != VM_FUNC_PY) continue;

    mp_obj_t mod = mp_import_name(
      qstr_from_str(fn->py_module),
      mp_const_none, mp_obj_new_int(0)
    );
    fn->py_callable = mp_load_attr(mod, qstr_from_str(fn->py_name));
  }

  for (volatile uint32_t i = 0; i < TASKS_MAX; i++) {
    VM_TaskInstance *task_instance = &vm_task_instances[i];
    memset(task_instance, 0, sizeof(VM_TaskInstance));
    task_instance->ID = i;
  }

  for (volatile uint32_t i = 0; i < (vm_taskcount_setups + vm_taskcount_events); i++) {
    VM_InitTask(i, vm_task_initializers[i].START_PC, vm_task_initializers[i].START_STACK_PTR);
  }

  for (volatile uint32_t i = 0; i < vm_taskcount_setups; i++) {
    VM_TaskInstance *task_instance = &vm_task_instances[i];
    if (!task_instance->FLAGS.enabled) continue;
    while (!VM_ExecuteTask_Step(task_instance));
  }

  actual_status = READY;
}

void TR_Step(void) {
  if (run_mode) {
    actual_status = RUNNING;

    for (volatile uint32_t i = vm_taskcount_setups; i < TASKS_MAX; i++) {
      VM_TaskInstance *task_instance = &vm_task_instances[i];
      if (!task_instance->FLAGS.enabled) continue;
      VM_ExecuteTask_Step(task_instance);
    }
  } else {
    actual_status = STOPPED;
  }
}

void port_gc_collect(void) {
  // Scan only the live portion of each task's stack.
  for (uint8_t i = 0; i < TASKS_MAX; i++) {
    VM_TaskInstance *t = &vm_task_instances[i];
    if (!t->FLAGS.enabled) continue;
    gc_collect_root((void **)&vm_stack[0], t->STACK_PTR);
  }
}




/* ##################################################################################### */
/* ###                             CODE GENERATED USING                              ### */
/* ###    ______   ______     __  __     __    __     ______   ______     ______     ### */
/* ###   /\__  _\ /\  == \   /\ \/\ \   /\ "-./  \   /\  == \ /\  ___\   /\__  _\    ### */
/* ###   \/_/\ \/ \ \  __<   \ \ \_\ \  \ \ \-./\ \  \ \  _-/ \ \  __\   \/_/\ \/    ### */
/* ###      \ \_\  \ \_\ \_\  \ \_____\  \ \_\ \ \_\  \ \_\    \ \_____\    \ \_\    ### */
/* ###       \/_/   \/_/ /_/   \/_____/   \/_/  \/_/   \/_/     \/_____/     \/_/    ### */
/* ###                                                                               ### */
/* ###   by AlienLogic (Michele Trombetta)                                           ### */
/* ##################################################################################### */
/* ###   Built using:                                                                ### */
/* ###     - TrumpeT v1.0.0 pre                                                      ### */
/* ###     - VM v1.0.0 pre                                                           ### */   
/* ###   Usage:                                                                      ### */
/* ###     - Stack: 84 Bytes                                                         ### */
/* ###     - Code: 530 Bytes                                                         ### */
/* ###     - Tasks: 3                                                                ### */
/* ##################################################################################### */
/* ###   Libraries:                                                                  ### */
/* ##################################################################################### */

// ### INC_Pin
#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/digitalio/Direction.h"
#include "shared-bindings/digitalio/Pull.h"
#include "shared-bindings/pwmio/PWMOut.h"
#include "shared-bindings/analogio/AnalogIn.h"
#include "shared-bindings/board/__init__.h"

static const mcu_pin_obj_t *gpio_pin_table[] = {
    &pin_GPIO0,  &pin_GPIO1,  &pin_GPIO2,  &pin_GPIO3,
    &pin_GPIO4,  &pin_GPIO5,  &pin_GPIO6,  &pin_GPIO7,
    &pin_GPIO8,  &pin_GPIO9,  &pin_GPIO10, &pin_GPIO11,
    &pin_GPIO12, &pin_GPIO13, &pin_GPIO14, &pin_GPIO15,
    &pin_GPIO16, &pin_GPIO17, &pin_GPIO18, &pin_GPIO19,
    &pin_GPIO20, &pin_GPIO21, &pin_GPIO22, &pin_GPIO23,
    &pin_GPIO24, &pin_GPIO25, &pin_GPIO26, &pin_GPIO27,
    &pin_GPIO28, &pin_GPIO29,
};
#define GPIO_PIN_TABLE_SIZE (sizeof(gpio_pin_table) / sizeof(gpio_pin_table[0]))
 
// ### INC_Delay
#include "py/mphal.h"

static void BLOCK_Event_OnPowerOn(VM_TaskInstance *task_instance, uint8_t *payload) {
  if (!task_instance->FLAGS.is_first_start) {
    task_instance->PC = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
  }
  task_instance->FLAGS.is_first_start = false;
}
static void BLOCK_Control_If(VM_TaskInstance *task_instance, uint8_t *payload) {
    bool condition = mp_obj_is_true(SP_pop(&task_instance->STACK_PTR));
    if (!condition)
        task_instance->PC = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8)
                          | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
}

static void BLOCK_Control_IfElse(VM_TaskInstance *task_instance, uint8_t *payload) {
    bool condition = mp_obj_is_true(SP_pop(&task_instance->STACK_PTR));
    if (!condition)
        task_instance->PC = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8)
                          | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
}

static void BLOCK_Select_Integer(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t val_false = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t val_true  = SP_pop(&task_instance->STACK_PTR);
    bool condition     = mp_obj_is_true(SP_pop(&task_instance->STACK_PTR));
    SP_push(&task_instance->STACK_PTR, condition ? val_true : val_false);
}

static void BLOCK_Loop_Wait(VM_TaskInstance *task_instance, uint8_t *payload) {
    bool condition = mp_obj_is_true(SP_pop(&task_instance->STACK_PTR));
    if (!condition)
        task_instance->PC = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8)
                          | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
}

static void BLOCK_Loop_While(VM_TaskInstance *task_instance, uint8_t *payload) {
    bool condition = mp_obj_is_true(SP_pop(&task_instance->STACK_PTR));
    if (condition)
        task_instance->PC = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8)
                          | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
}

static void BLOCK_Math_Add(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t n1 = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t n2 = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_ADD, n2, n1));
}

static void BLOCK_Math_Subtract(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t n1 = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t n2 = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_SUBTRACT, n2, n1));
}

static void BLOCK_Math_Divide(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t n1 = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t n2 = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_TRUE_DIVIDE, n2, n1));
}

static void BLOCK_Math_Multiply(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t n1 = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t n2 = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_MULTIPLY, n2, n1));
}

static void BLOCK_Const_String(VM_TaskInstance *task_instance, uint8_t *payload) {
    uint8_t len = payload[0];
    SP_push(&task_instance->STACK_PTR, mp_obj_new_str((const char *)&payload[1], len));
}

static void BLOCK_String_Assign(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t src         = SP_pop(&task_instance->STACK_PTR);
    uint32_t offset      = SP_pop_u32(&task_instance->STACK_PTR);
    SP_write(task_instance->STACK_PTR, offset - 1, src);
}
static void BLOCK_String_Create(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t src         = SP_pop(&task_instance->STACK_PTR);
    uint32_t offset      = SP_pop_u32(&task_instance->STACK_PTR);
    SP_write(task_instance->STACK_PTR, offset - 1, src);
}
static void BLOCK_String_Read(VM_TaskInstance *task_instance, uint8_t *payload) {
    uint32_t offset = SP_pop_u32(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, SP_peek(task_instance->STACK_PTR, offset));
}
static void BLOCK_String_Convert(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t value = SP_pop(&task_instance->STACK_PTR);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.3f", (double)mp_obj_get_float(value));
    SP_push(&task_instance->STACK_PTR, mp_obj_new_str(buf, strlen(buf)));
}
static void BLOCK_String_Concatenate(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t srcB = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t srcA = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_ADD, srcA, srcB));
}

static void BLOCK_String_Length(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t str = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_obj_len(str));
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
static void BLOCK_String_Substring(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t length_obj = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t start_obj  = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t src        = SP_pop(&task_instance->STACK_PTR);
    mp_int_t start      = mp_obj_get_int(start_obj);
    mp_int_t length     = mp_obj_get_int(length_obj);
    mp_obj_t slice      = mp_obj_new_slice(mp_obj_new_int(start), mp_obj_new_int(start + length), MP_OBJ_NULL);
    SP_push(&task_instance->STACK_PTR, mp_obj_subscr(src, slice, MP_OBJ_SENTINEL));
}
static void BLOCK_String_DEBUG(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t val = SP_pop(&task_instance->STACK_PTR);
    mp_obj_print_helper(&mp_plat_print, val, PRINT_STR);
    mp_printf(&mp_plat_print, "\n");
}
static void BLOCK_Comparison_Equal(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t n1 = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t n2 = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_EQUAL, n2, n1));
}

static void BLOCK_Comparison_Less(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t n1 = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t n2 = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_LESS, n2, n1));
}

static void BLOCK_Comparison_Greater(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t n1 = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t n2 = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_MORE, n2, n1));
}

static void BLOCK_Const_Value(VM_TaskInstance *task_instance, uint8_t *payload) {
    // payload: 4 bytes little-endian IEEE-754 float
    uint32_t raw = (uint32_t)payload[0]
                 | ((uint32_t)payload[1] << 8)
                 | ((uint32_t)payload[2] << 16)
                 | ((uint32_t)payload[3] << 24);
    float val;
    memcpy(&val, &raw, sizeof(float));
    SP_push(&task_instance->STACK_PTR, mp_obj_new_float(val));
}

static void BLOCK_Const_Integer(VM_TaskInstance *task_instance, uint8_t *payload) {
    // payload: 4 bytes little-endian int32
    int32_t val = (int32_t)( (uint32_t)payload[0]
                           | ((uint32_t)payload[1] << 8)
                           | ((uint32_t)payload[2] << 16)
                           | ((uint32_t)payload[3] << 24));
    SP_push(&task_instance->STACK_PTR, mp_obj_new_int(val));
}

static void BLOCK_Const_TrueFalse(VM_TaskInstance *task_instance, uint8_t *payload) {
    // payload: 1 byte — 0x00 = false, anything else = true
    SP_push(&task_instance->STACK_PTR, payload[0] ? mp_const_true : mp_const_false);
}

static void BLOCK_Variable_Read(VM_TaskInstance *task_instance, uint8_t *payload) {
    uint32_t offset = SP_pop_u32(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, SP_peek(task_instance->STACK_PTR, offset));
}

static void BLOCK_Variable_Assign(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t val         = SP_pop(&task_instance->STACK_PTR);
    uint32_t offset      = SP_pop_u32(&task_instance->STACK_PTR);
    SP_write(task_instance->STACK_PTR, offset - 1, val);
}
static void BLOCK_Variable_Create(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t val         = SP_pop(&task_instance->STACK_PTR);
    uint32_t offset      = SP_pop_u32(&task_instance->STACK_PTR);
    SP_write(task_instance->STACK_PTR, offset - 1, val);  // offset - 1 like the original
}
// payload: 4 bytes little-endian uint32 — GP pin index (from SELECT.BRD.PINS enum)
// Stack: dest_offset (u32) — variable slot to write the DigitalInOut object into
static void BLOCK_Pin_InitOutput(VM_TaskInstance *task_instance, uint8_t *payload) {
    uint32_t pin_index = (uint32_t)payload[0]
                       | ((uint32_t)payload[1] << 8)
                       | ((uint32_t)payload[2] << 16)
                       | ((uint32_t)payload[3] << 24);
 
    uint32_t dest_offset = SP_pop_u32(&task_instance->STACK_PTR);
 
    if (pin_index >= GPIO_PIN_TABLE_SIZE) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid pin index"));
    }
 
    const mcu_pin_obj_t *pin = gpio_pin_table[pin_index];
 
    digitalio_digitalinout_obj_t *dio = mp_obj_malloc(digitalio_digitalinout_obj_t, &digitalio_digitalinout_type);
    common_hal_digitalio_digitalinout_construct(dio, pin);
    common_hal_digitalio_digitalinout_switch_to_output(dio, false, DRIVE_MODE_PUSH_PULL);
 
    SP_write(task_instance->STACK_PTR, dest_offset - 1, MP_OBJ_FROM_PTR(dio));
}
// payload: 4 bytes little-endian uint32 — GPIO index
// Stack: dest_offset (u32) — variable slot to write the DigitalInOut object into
static void BLOCK_Pin_InitInput(VM_TaskInstance *task_instance, uint8_t *payload) {
    uint32_t pin_index = (uint32_t)payload[0]
                       | ((uint32_t)payload[1] << 8)
                       | ((uint32_t)payload[2] << 16)
                       | ((uint32_t)payload[3] << 24);
 
    uint32_t dest_offset = SP_pop_u32(&task_instance->STACK_PTR);
 
    if (pin_index >= GPIO_PIN_TABLE_SIZE) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid pin index"));
    }
 
    const mcu_pin_obj_t *pin = gpio_pin_table[pin_index];
 
    digitalio_digitalinout_obj_t *dio = mp_obj_malloc(digitalio_digitalinout_obj_t, &digitalio_digitalinout_type);
    common_hal_digitalio_digitalinout_construct(dio, pin);
    common_hal_digitalio_digitalinout_switch_to_input(dio, PULL_NONE);
 
    SP_write(task_instance->STACK_PTR, dest_offset - 1, MP_OBJ_FROM_PTR(dio));
}
// payload: none
// Stack: state (mp_obj_t bool), pin (mp_obj_t DigitalInOut)
static void BLOCK_Pin_Write(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t state_obj = SP_pop(&task_instance->STACK_PTR);
    uint32_t offset = SP_pop_u32(&task_instance->STACK_PTR);
    mp_obj_t pin_obj   = SP_peek(task_instance->STACK_PTR, offset);
 
    digitalio_digitalinout_obj_t *dio = MP_OBJ_TO_PTR(pin_obj);
    common_hal_digitalio_digitalinout_set_value(dio, mp_obj_is_true(state_obj));
}
// payload: none
// Stack: pin offset (u32)
// Pushes bool value onto stack
static void BLOCK_Pin_Read(VM_TaskInstance *task_instance, uint8_t *payload) {
    uint32_t offset  = SP_pop_u32(&task_instance->STACK_PTR);
    mp_obj_t pin_obj = SP_peek(task_instance->STACK_PTR, offset);
 
    digitalio_digitalinout_obj_t *dio = MP_OBJ_TO_PTR(pin_obj);
    bool value = common_hal_digitalio_digitalinout_get_value(dio);
    SP_push(&task_instance->STACK_PTR, value ? mp_const_true : mp_const_false);
}
static void BLOCK_Delay_Wait___Init(VM_TaskInstance *task_instance, uint8_t *payload) {
    SP_push_u32(&task_instance->STACK_PTR, mp_hal_ticks_ms());
}
static void BLOCK_Delay_Wait(VM_TaskInstance *task_instance, uint8_t *payload) {
    uint32_t amount_ms = (uint32_t)mp_obj_get_float(SP_pop(&task_instance->STACK_PTR));
    uint32_t start_ms   = SP_peek_u32(task_instance->STACK_PTR, 1);

    uint32_t elapsed = mp_hal_ticks_ms() - start_ms;
    if (elapsed < amount_ms) {
        task_instance->PC = (uint32_t)payload[0]
                          | ((uint32_t)payload[1] << 8)
                          | ((uint32_t)payload[2] << 16)
                          | ((uint32_t)payload[3] << 24);
    }
}

VM_FuncEntry vm_functions[FUNCTIONS_MAX] = {
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Comparison_Equal", .c_func      = BLOCK_Comparison_Equal,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Comparison_Greater", .c_func      = BLOCK_Comparison_Greater,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Comparison_Less", .c_func      = BLOCK_Comparison_Less,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Const_Integer", .c_func      = BLOCK_Const_Integer,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Const_String", .c_func      = BLOCK_Const_String,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Const_TrueFalse", .c_func      = BLOCK_Const_TrueFalse,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Const_Value", .c_func      = BLOCK_Const_Value,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Control_If", .c_func      = BLOCK_Control_If,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Control_IfElse", .c_func      = BLOCK_Control_IfElse,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Delay_Wait", .c_func      = BLOCK_Delay_Wait,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Delay_Wait___Init", .c_func      = BLOCK_Delay_Wait___Init,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Event_OnPowerOn", .c_func      = BLOCK_Event_OnPowerOn,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Loop_Wait", .c_func      = BLOCK_Loop_Wait,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Loop_While", .c_func      = BLOCK_Loop_While,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Math_Add", .c_func      = BLOCK_Math_Add,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Math_Divide", .c_func      = BLOCK_Math_Divide,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Math_Multiply", .c_func      = BLOCK_Math_Multiply,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Math_Subtract", .c_func      = BLOCK_Math_Subtract,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Pin_InitInput", .c_func      = BLOCK_Pin_InitInput,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Pin_InitOutput", .c_func      = BLOCK_Pin_InitOutput,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Pin_Read", .c_func      = BLOCK_Pin_Read,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Pin_Write", .c_func      = BLOCK_Pin_Write,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Select_Integer", .c_func      = BLOCK_Select_Integer,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_Assign", .c_func      = BLOCK_String_Assign,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_Concatenate", .c_func      = BLOCK_String_Concatenate,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_Convert", .c_func      = BLOCK_String_Convert,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_Create", .c_func      = BLOCK_String_Create,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_DEBUG", .c_func      = BLOCK_String_DEBUG,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_Length", .c_func      = BLOCK_String_Length,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_PadStart", .c_func      = BLOCK_String_PadStart,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_Read", .c_func      = BLOCK_String_Read,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_String_Substring", .c_func      = BLOCK_String_Substring,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Variable_Assign", .c_func      = BLOCK_Variable_Assign,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Variable_Create", .c_func      = BLOCK_Variable_Create,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_name = "BLOCK_Variable_Read", .c_func      = BLOCK_Variable_Read,        NULL,       NULL             }
//  { VM_FUNC_C,  2, .c_func      = my_c_handler,  NULL,       NULL             },
//  { VM_FUNC_C,  0, .c_func      = vm_noop,        NULL,       NULL             },
//  { VM_FUNC_PY, 3, .py_callable = MP_OBJ_NULL,    "mymodule", "my_py_handler"  },
};

uint8_t vm_code_buffer[CODE_SIZE] = {
0x21, 0x03,0x00,0x00,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0x00,0x00, 0x80, 0x20,0x00, 0x21, 0x03,0x00,0x00,0x00, 0x21, 0x04,0x00,0x00,0x00, 0x80, 0x22,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0x80,0x3F, 0x80, 0x0E,0x00, 0x80, 0x20,0x00, 0x91, 0x04,0x00,0x00,0x00, 0x21, 0x03,0x00,0x00,0x00, 0x80, 0x22,0x00, 0x13, 0x03,0x00,0x00,0x00, 0x80, 0x22,0x00, 0x80, 0x02,0x00, 0x81, 0x06, 0x0D,0x00, 0x10,0x00,0x00,0x00, 0x8F   ,   0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x04,0x00, 0x03,0x61,0x62,0x63, 0x80, 0x1A,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x0E, 0x04,0x00, 0x0B,0x48,0x65,0x6C,0x6C,0x6F,0x20,0x57,0x6F,0x72,0x6C,0x64, 0x80, 0x17,0x00, 0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0xC0,0x41,0x44, 0x80, 0x21,0x00, 0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x13, 0x04,0x00,0x00,0x00, 0x80, 0x1E,0x00, 0x81, 0x0B, 0x04,0x00, 0x08,0x54,0x72,0x75,0x6D,0x70,0x65,0x74,0x21, 0x80, 0x18,0x00, 0x80, 0x1A,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x80, 0x1E,0x00, 0x80, 0x1B,0x00, 0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0x00,0x00, 0x80, 0x21,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x13, 0x04,0x00,0x00,0x00, 0x80, 0x22,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0x7A,0x44, 0x80, 0x0E,0x00, 0x80, 0x20,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x80, 0x22,0x00, 0x80, 0x19,0x00, 0x80, 0x1B,0x00, 0xE8, 0x01,0x00, 0xD0, 0x0B,0x01,0x00,0x00, 0x13, 0x06,0x00,0x00,0x00, 0x80, 0x22,0x00, 0x80, 0x19,0x00, 0x80, 0x1B,0x00, 0x9F, 0x90, 0xFC,0x00,0x00,0x00, 0x13, 0x02,0x00,0x00,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0x70,0x41, 0x88, 0x00,0x00,0x00,0x00, 0xA0, 0x04,0x00,0x00,0x00, 0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x13,0x00, 0x00,0x00,0x00,0x00, 0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x13,0x00, 0x01,0x00,0x00,0x00, 0xFF   ,   0x81, 0x06, 0x0B,0x00, 0x48,0x01,0x00,0x00, 0x13, 0x09,0x00,0x00,0x00, 0x81, 0x06, 0x05,0x00, 0x01,0x00,0x00,0x00, 0x80, 0x15,0x00, 0x80, 0x0A,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0xFA,0x43, 0x81, 0x06, 0x09,0x00, 0x63,0x01,0x00,0x00, 0xE0, 0x13, 0x09,0x00,0x00,0x00, 0x81, 0x06, 0x05,0x00, 0x00,0x00,0x00,0x00, 0x80, 0x15,0x00, 0x80, 0x0A,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0xFA,0x43, 0x81, 0x06, 0x09,0x00, 0x87,0x01,0x00,0x00, 0xE0, 0x81, 0x06, 0x05,0x00, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x0D,0x00, 0x50,0x01,0x00,0x00, 0xD0, 0x48,0x01,0x00,0x00   ,   0x81, 0x06, 0x0B,0x00, 0xAD,0x01,0x00,0x00, 0x13, 0x0A,0x00,0x00,0x00, 0x81, 0x06, 0x05,0x00, 0x01,0x00,0x00,0x00, 0x80, 0x15,0x00, 0x80, 0x0A,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0xFA,0x43, 0x81, 0x06, 0x09,0x00, 0xC8,0x01,0x00,0x00, 0xE0, 0x13, 0x0A,0x00,0x00,0x00, 0x81, 0x06, 0x05,0x00, 0x00,0x00,0x00,0x00, 0x80, 0x15,0x00, 0x80, 0x0A,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0x00,0xFA,0x43, 0x81, 0x06, 0x09,0x00, 0xEC,0x01,0x00,0x00, 0xE0, 0x81, 0x06, 0x05,0x00, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x0D,0x00, 0xB5,0x01,0x00,0x00, 0xD0, 0xAD,0x01,0x00,0x00
};

VM_TaskInitializer vm_task_initializers[TASKS_MAX] = {
	{ .START_PC = 76, .START_STACK_PTR = 0 },
	{ .START_PC = 328, .START_STACK_PTR = 13 },
	{ .START_PC = 429, .START_STACK_PTR = 15 }
};

uint8_t vm_taskcount_setups = 1;
uint8_t vm_taskcount_events = 2;
uint8_t signature[32] = { 0x00 };

// 21 03 00 00 00 81 06 06 00 00 00 00 00 80 20 00 21 03 00 00 00 21 04 00 00 00 80 22 00 81 06 06 00 00 00 80 3F 80 0E 00 80 20 00 91 04 00 00 00 21 03 00 00 00 80 22 00 13 03 00 00 00 80 22 00 80 02 00 81 06 0D 00 10 00 00 00 8F E8 01 00 13 01 00 00 00 81 06 04 00 03 61 62 63 80 1A 00 13 01 00 00 00 81 0E 04 00 0B 48 65 6C 6C 6F 20 57 6F 72 6C 64 80 17 00 E8 01 00 13 01 00 00 00 81 06 06 00 00 C0 41 44 80 21 00 E8 01 00 13 01 00 00 00 13 04 00 00 00 80 1E 00 81 0B 04 00 08 54 72 75 6D 70 65 74 21 80 18 00 80 1A 00 13 01 00 00 00 80 1E 00 80 1B 00 E8 01 00 13 01 00 00 00 81 06 06 00 00 00 00 00 80 21 00 13 01 00 00 00 13 04 00 00 00 80 22 00 81 06 06 00 00 00 7A 44 80 0E 00 80 20 00 13 01 00 00 00 80 22 00 80 19 00 80 1B 00 E8 01 00 D0 0B 01 00 00 13 06 00 00 00 80 22 00 80 19 00 80 1B 00 9F 90 FC 00 00 00 13 02 00 00 00 81 06 06 00 00 00 70 41 88 00 00 00 00 A0 04 00 00 00 E8 01 00 13 01 00 00 00 81 06 13 00 00 00 00 00 E8 01 00 13 01 00 00 00 81 06 13 00 01 00 00 00 FF 81 06 0B 00 48 01 00 00 13 09 00 00 00 81 06 05 00 01 00 00 00 80 15 00 80 0A 00 81 06 06 00 00 00 FA 43 81 06 09 00 63 01 00 00 E0 13 09 00 00 00 81 06 05 00 00 00 00 00 80 15 00 80 0A 00 81 06 06 00 00 00 FA 43 81 06 09 00 87 01 00 00 E0 81 06 05 00 01 00 00 00 81 06 0D 00 50 01 00 00 D0 48 01 00 00 81 06 0B 00 AD 01 00 00 13 0A 00 00 00 81 06 05 00 01 00 00 00 80 15 00 80 0A 00 81 06 06 00 00 00 FA 43 81 06 09 00 C8 01 00 00 E0 13 0A 00 00 00 81 06 05 00 00 00 00 00 80 15 00 80 0A 00 81 06 06 00 00 00 FA 43 81 06 09 00 EC 01 00 00 E0 81 06 05 00 01 00 00 00 81 06 0D 00 B5 01 00 00 D0 AD 01 00 00'