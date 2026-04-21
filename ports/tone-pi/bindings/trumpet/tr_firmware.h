#pragma once

#include "py/gc.h"
#include "py/obj.h"
#include "py/runtime.h"
#include <stdio.h>

#include "supervisor/port.h"

#include <stdint.h>
#include <stdbool.h>

#define STACK_SIZE      400
extern mp_obj_t vm_stack[STACK_SIZE];


#define TASKS_MAX       4
typedef struct {
  uint32_t START_PC;
  uint32_t START_STACK_PTR;
} VM_TaskInitializer;
extern VM_TaskInitializer vm_task_initializers[TASKS_MAX];


#define CODE_SIZE       2000
extern uint8_t vm_code_buffer[CODE_SIZE];

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

extern VM_TaskInstance vm_task_instances[TASKS_MAX];


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

#define FUNCTIONS_MAX   28
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
  const char *c_name;
} VM_FuncEntry;

extern VM_FuncEntry vm_functions[FUNCTIONS_MAX];


// #define DEBUG
void TR_Init(void);
void TR_Step(void);