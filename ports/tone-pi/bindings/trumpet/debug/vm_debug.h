#pragma once
#include <stdint.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Compile-time limits
// ─────────────────────────────────────────────────────────────────────────────
#ifndef VM_DBG_MAX_PC_BREAKS
#  define VM_DBG_MAX_PC_BREAKS    16
#endif
#ifndef VM_DBG_MAX_OP_BREAKS
#  define VM_DBG_MAX_OP_BREAKS    8
#endif
#ifndef VM_DBG_STACK_DUMP_DEPTH
#  define VM_DBG_STACK_DUMP_DEPTH 16
#endif
#ifndef VM_DBG_MAX_PAYLOAD_BYTES
#  define VM_DBG_MAX_PAYLOAD_BYTES 32
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Debug state
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    bool    enabled;
    bool    step_mode;
    bool    stack_on_pause;

    int8_t  task_filter;        // -1 = all tasks, 0..N = specific task ID

    volatile bool   paused;
    volatile bool   step_requested;

    uint8_t  paused_task_id;
    uint32_t paused_pc;
    uint8_t  paused_opcode;

    uint32_t pc_breaks[VM_DBG_MAX_PC_BREAKS];
    uint8_t  pc_break_count;

    uint8_t  op_breaks[VM_DBG_MAX_OP_BREAKS];
    uint8_t  op_break_count;
} VM_DebugState;

extern VM_DebugState vm_debug;

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────
void vm_debug_enable(int8_t task_filter);
void vm_debug_disable(void);
void vm_debug_step_mode_enable(void);
void vm_debug_step_mode_disable(void);
void vm_debug_step(void);

bool vm_debug_add_pc_break(uint32_t pc);
bool vm_debug_remove_pc_break(uint32_t pc);
void vm_debug_clear_pc_breaks(void);

bool vm_debug_add_op_break(uint8_t opcode);
bool vm_debug_remove_op_break(uint8_t opcode);
void vm_debug_clear_op_breaks(void);

void vm_debug_inspect_pc(void);
void vm_debug_inspect_stack(void);
void vm_debug_inspect_all_tasks(void);
void vm_debug_list_breaks(void);

// ─────────────────────────────────────────────────────────────────────────────
//  Internal — called from vm.c
// ─────────────────────────────────────────────────────────────────────────────

// Called at DISPATCH time — returns true if task should be blocked (paused)
bool _vm_debug_check(uint8_t task_id, uint32_t pc, uint8_t opcode,
                     uint32_t stack_ptr);

// Called from COMPLETE() macro — prints SP after execution (optional)
void _vm_debug_log_sp_after(uint8_t task_id, uint32_t sp_after);