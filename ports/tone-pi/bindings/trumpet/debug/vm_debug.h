#pragma once
#include <stdint.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Compile-time limits
// ─────────────────────────────────────────────────────────────────────────────
#ifndef VM_DBG_MAX_PC_BREAKS
#  define VM_DBG_MAX_PC_BREAKS   16
#endif
#ifndef VM_DBG_MAX_OP_BREAKS
#  define VM_DBG_MAX_OP_BREAKS   8
#endif
#ifndef VM_DBG_STACK_DUMP_DEPTH
#  define VM_DBG_STACK_DUMP_DEPTH 16   // max slots printed on inspect
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Debug state  (one global instance, zero-initialised at startup)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    // --- mode flags ----------------------------------------------------------
    bool    enabled;        // master switch; when false, zero overhead
    bool    step_mode;      // pause after every opcode until vm_debug_step()
    bool    stack_on_pause; // print stack automatically on every pause

    // --- task filter ---------------------------------------------------------
    int8_t  task_filter;    // -1 = watch all tasks, 0..N = specific task ID

    // --- pause / resume handshake --------------------------------------------
    volatile bool   paused;         // VM sets this; REPL clears via vm_debug_step()
    volatile bool   step_requested; // REPL sets this; VM clears after consuming

    // --- which task is currently paused --------------------------------------
    uint8_t  paused_task_id;
    uint32_t paused_pc;     // PC *before* the opcode that caused the pause
    uint8_t  paused_opcode;

    // --- PC breakpoints ------------------------------------------------------
    uint32_t pc_breaks[VM_DBG_MAX_PC_BREAKS];
    uint8_t  pc_break_count;

    // --- opcode-type breakpoints ---------------------------------------------
    uint8_t  op_breaks[VM_DBG_MAX_OP_BREAKS];
    uint8_t  op_break_count;
} VM_DebugState;

extern VM_DebugState vm_debug;

// ─────────────────────────────────────────────────────────────────────────────
//  Public API  — wire these to CircuitPython mp_obj later
// ─────────────────────────────────────────────────────────────────────────────

// Enable debug for a specific task (-1 = all tasks)
void vm_debug_enable(int8_t task_filter);

// Disable debug entirely
void vm_debug_disable(void);

// Enter step-by-step mode (implies debug enabled)
void vm_debug_step_mode_enable(void);
void vm_debug_step_mode_disable(void);

// Release one step while paused in step mode
void vm_debug_step(void);

// ── PC breakpoints ────────────────────────────────────────────────────────────
bool vm_debug_add_pc_break(uint32_t pc);
bool vm_debug_remove_pc_break(uint32_t pc);
void vm_debug_clear_pc_breaks(void);

// ── Opcode breakpoints ────────────────────────────────────────────────────────
bool vm_debug_add_op_break(uint8_t opcode);
bool vm_debug_remove_op_break(uint8_t opcode);
void vm_debug_clear_op_breaks(void);

// ── Inspect (print to CircuitPython REPL) ─────────────────────────────────────
void vm_debug_inspect_pc(void);           // current PC + opcode of paused task
void vm_debug_inspect_stack(void);        // stack dump of paused task
void vm_debug_inspect_all_tasks(void);    // brief status of every task
void vm_debug_list_breaks(void);          // print all active breakpoints

// ─────────────────────────────────────────────────────────────────────────────
//  Internal  — called from inside VM_ExecuteTask_Step
// ─────────────────────────────────────────────────────────────────────────────
// Returns true if execution should be blocked (task is paused)
bool _vm_debug_check(uint8_t task_id, uint32_t pc, uint8_t opcode,
                     uint32_t stack_ptr);
