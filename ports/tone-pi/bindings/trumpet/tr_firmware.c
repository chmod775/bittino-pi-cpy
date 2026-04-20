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
/* ###     - Code: 170 Bytes                                                         ### */
/* ###     - Tasks: 1                                                                ### */
/* ##################################################################################### */
/* ###   Libraries:                                                                  ### */
/* ##################################################################################### */

#include "tr_firmware.h"

#define TASKS_MAX       4
#define STACK_SIZE      400
#define CODE_SIZE       1000
#define FUNCTIONS_MAX   27


enum VM_Opcodes {
  NOP = 0x00,

  LITERAL_UINT32 = 0x13,

  READ_ARG = 0x21,

  EXE = 0x80,
  EXE_ARGS = 0x81,

  MACRO_CALL = 0x88,
  MACRO_RETURN = 0x8F,

  SLOT_BODY = 0x90,
  SLOT_CALL = 0x91,
  SLOT_RETURN = 0x9F,

  GROUP_END = 0xA0,

  POINT_OFFSET = 0xB1,

  GOTO = 0xD0,

  POP = 0xE0,
  PUSH = 0xE8,

  END = 0xFF
};

typedef struct
{
  unsigned int enabled: 1;
  unsigned int is_first_start: 1;
} VM_Task_Flags;

typedef struct {
  uint16_t ID;
  uint32_t PC;
  uint32_t STACK_PTR;
  VM_Task_Flags FLAGS;
} VM_TaskInstance;


/* ##### Stack ##### */
mp_obj_t vm_stack[STACK_SIZE];

static inline mp_obj_t  SP_pop     (uint32_t *sp)                    { return vm_stack[--(*sp)]; }
static inline void      SP_push    (uint32_t *sp, mp_obj_t v)        { vm_stack[(*sp)++] = v; }
static inline mp_obj_t  SP_peek    (uint32_t sp, uint32_t off)       { return vm_stack[sp - off]; }

// Raw uint32 — no GC, direct bit cast (PC values, literals, offsets)
static inline uint32_t  SP_pop_u32 (uint32_t *sp)                    { return (uint32_t)(uintptr_t)vm_stack[--(*sp)]; }
static inline void      SP_push_u32(uint32_t *sp, uint32_t v)        { vm_stack[(*sp)++] = (mp_obj_t)(uintptr_t)v; }
static inline uint32_t  SP_peek_u32(uint32_t sp, uint32_t off)       { return (uint32_t)(uintptr_t)vm_stack[sp - off]; }
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

extern uint8_t vm_code_buffer[CODE_SIZE];

typedef enum { VM_FUNC_C, VM_FUNC_PY } VM_FuncKind;

typedef struct {
  VM_FuncKind kind;
  uint8_t     stack_argc;
  union {
    void     (*c_func)(VM_TaskInstance *ti, uint8_t *payload);
    mp_obj_t  py_callable;
  };
  const char *py_module;
  const char *py_name;
} VM_FuncEntry;

extern VM_FuncEntry vm_functions[FUNCTIONS_MAX];

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

  #define DISPATCH() do {                                       \
      uint8_t op = BC8(task_instance->PC); task_instance->PC++; \
      goto *dispatch_table[op] ?: &&op_DEFAULT;                 \
  } while(0)

  #define COMPLETE() do { return false; } while (0)

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

  if (fn->kind == VM_FUNC_C) {
    fn->c_func(task_instance, &vm_code_buffer[task_instance->PC]);
  } else {
    mp_obj_t py_args[fn->stack_argc];
    for (int i = fn->stack_argc - 1; i >= 0; i--)
      py_args[i] = SP_pop(&task_instance->STACK_PTR);

    mp_obj_t result = mp_call_function_n_kw(fn->py_callable, fn->stack_argc, 0, py_args);
    if (result != mp_const_none)
      SP_push(&task_instance->STACK_PTR, result);
  }

  task_instance->PC += argc;

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

op_DEFAULT:
  while(1);
}


typedef struct {
  uint32_t START_PC;
  uint32_t START_STACK_PTR;
} VM_TaskInitializer;
extern VM_TaskInitializer vm_task_initializers[TASKS_MAX];

extern uint8_t vm_taskcount_setups;
extern uint8_t vm_taskcount_events;

extern uint8_t signature[32];

enum VM_Status {
  IDLE,
  INITIALIZING,
  READY,
  RUNNING,
  STOPPED
};

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
    uint32_t dest_offset = SP_pop_u32(&task_instance->STACK_PTR);
    vm_stack[task_instance->STACK_PTR - dest_offset - 1] = src;
}

static void BLOCK_String_Concatenate(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t srcB = SP_pop(&task_instance->STACK_PTR);
    mp_obj_t srcA = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_binary_op(MP_BINARY_OP_ADD, srcA, srcB));
}

static void BLOCK_String_Convert(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t value = SP_pop(&task_instance->STACK_PTR);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.3f", (double)mp_obj_get_float(value));
    SP_push(&task_instance->STACK_PTR, mp_obj_new_str(buf, strlen(buf)));
}

static void BLOCK_String_Length(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t str = SP_pop(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, mp_obj_len(str));
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

static void BLOCK_String_Read(VM_TaskInstance *task_instance, uint8_t *payload) {
    uint32_t offset = SP_pop_u32(&task_instance->STACK_PTR);
    SP_push(&task_instance->STACK_PTR, SP_peek(task_instance->STACK_PTR, offset));
}

static void BLOCK_String_Create(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t src         = SP_pop(&task_instance->STACK_PTR);
    uint32_t dest_offset = SP_pop_u32(&task_instance->STACK_PTR);
    vm_stack[task_instance->STACK_PTR - dest_offset - 1] = src;
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
    mp_obj_t val    = SP_pop(&task_instance->STACK_PTR);
    uint32_t offset = SP_pop_u32(&task_instance->STACK_PTR);
    vm_stack[task_instance->STACK_PTR - offset - 1] = val;
}

static void BLOCK_Variable_Create(VM_TaskInstance *task_instance, uint8_t *payload) {
    mp_obj_t val    = SP_pop(&task_instance->STACK_PTR);
    uint32_t offset = SP_pop_u32(&task_instance->STACK_PTR);
    vm_stack[task_instance->STACK_PTR - offset - 1] = val;
}


VM_FuncEntry vm_functions[FUNCTIONS_MAX] = {
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Comparison_Equal,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Comparison_Greater,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Comparison_Less,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Const_Integer,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Const_String,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Const_TrueFalse,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Const_Value,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Control_If,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Control_IfElse,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Loop_Wait,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Loop_While,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Math_Add,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Math_Divide,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Math_Multiply,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Math_Subtract,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Select_Integer,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_String_Assign,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_String_Concatenate,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_String_Convert,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_String_Create,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_String_Length,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_String_PadStart,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_String_Read,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_String_Substring,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Variable_Assign,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Variable_Create,        NULL,       NULL             },
	{ VM_FUNC_C,  0, .c_func      = BLOCK_Variable_Read,        NULL,       NULL             }
//  { VM_FUNC_C,  2, .c_func      = my_c_handler,  NULL,       NULL             },
//  { VM_FUNC_C,  0, .c_func      = vm_noop,        NULL,       NULL             },
//  { VM_FUNC_PY, 3, .py_callable = MP_OBJ_NULL,    "mymodule", "my_py_handler"  },
};

uint8_t vm_code_buffer[CODE_SIZE] = {
0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x04,0x00, 0x03,0x61,0x62,0x63, 0x80, 0x13,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x0E, 0x04,0x00, 0x0B,0x48,0x65,0x6C,0x6C,0x6F,0x20,0x57,0x6F,0x72,0x6C,0x64, 0x80, 0x10,0x00, 0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x81, 0x06, 0x06,0x00, 0x00,0xC0,0x41,0x44, 0x80, 0x19,0x00, 0xE8, 0x01,0x00, 0x13, 0x01,0x00,0x00,0x00, 0x13, 0x04,0x00,0x00,0x00, 0x80, 0x16,0x00, 0x81, 0x0B, 0x04,0x00, 0x08,0x54,0x72,0x75,0x6D,0x70,0x65,0x74,0x21, 0x80, 0x11,0x00, 0x80, 0x13,0x00, 0xFF
};

VM_TaskInitializer vm_task_initializers[TASKS_MAX] = {
	{ .START_PC = 0, .START_STACK_PTR = 0 }
};

uint8_t vm_taskcount_setups = 1;
uint8_t vm_taskcount_events = 0;
uint8_t signature[32] = { 0x00 };
