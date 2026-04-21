#include "vm_debug.h"
#include "bindings/trumpet/tr_firmware.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Global debug state
// ─────────────────────────────────────────────────────────────────────────────
VM_DebugState vm_debug = {
    .enabled        = false,
    .step_mode      = false,
    .stack_on_pause = false,
    .task_filter    = -1,
    .paused         = false,
    .step_requested = false,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Opcode name table
// ─────────────────────────────────────────────────────────────────────────────
static const char *_opcode_name(uint8_t op) {
    switch (op) {
        case 0x00: return "NOP";
        case 0x13: return "LITERAL_UINT32";
        case 0x21: return "READ_ARG";
        case 0x80: return "EXE";
        case 0x81: return "EXE_ARGS";
        case 0x88: return "MACRO_CALL";
        case 0x8F: return "MACRO_RETURN";
        case 0x90: return "SLOT_BODY";
        case 0x91: return "SLOT_CALL";
        case 0x9F: return "SLOT_RETURN";
        case 0xA0: return "GROUP_END";
        case 0xB1: return "POINT_OFFSET";
        case 0xD0: return "GOTO";
        case 0xE0: return "POP";
        case 0xE8: return "PUSH";
        case 0xFF: return "END";
        default:   return "???";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
static inline bool _task_matches(uint8_t task_id) {
    return vm_debug.task_filter < 0 || vm_debug.task_filter == (int8_t)task_id;
}

static bool _is_pc_break(uint32_t pc) {
    for (uint8_t i = 0; i < vm_debug.pc_break_count; i++)
        if (vm_debug.pc_breaks[i] == pc) return true;
    return false;
}

static bool _is_op_break(uint8_t op) {
    for (uint8_t i = 0; i < vm_debug.op_break_count; i++)
        if (vm_debug.op_breaks[i] == op) return true;
    return false;
}

static void _print_pause_header(const char *reason) {
    mp_printf(MP_PYTHON_PRINTER,
        "\n[VM DBG] ── PAUSED (task=%u, reason=%s) ──────────────\n"
        "           PC=0x%08lX  OP=0x%02X (%s)\n",
        vm_debug.paused_task_id, reason,
        (unsigned long)vm_debug.paused_pc,
        vm_debug.paused_opcode,
        _opcode_name(vm_debug.paused_opcode));
}

static void _dump_stack(uint32_t sp) {
    uint32_t depth = sp < VM_DBG_STACK_DUMP_DEPTH ? sp : VM_DBG_STACK_DUMP_DEPTH;
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] Stack (SP=%lu, showing top %lu):\n",
        (unsigned long)sp, (unsigned long)depth);
    for (uint32_t i = 0; i < depth; i++) {
        uint32_t raw = (uint32_t)(uintptr_t)vm_stack[sp - 1 - i];
        mp_printf(MP_PYTHON_PRINTER,
            "           [SP-%lu] raw=0x%08lX (%lu)\n",
            (unsigned long)(i + 1),
            (unsigned long)raw,
            (unsigned long)raw);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Peek helpers — read vm_code_buffer without touching PC
// ─────────────────────────────────────────────────────────────────────────────
static inline uint8_t _peek8(uint32_t pc, uint8_t off) {
    return vm_code_buffer[pc + off];
}
static inline uint16_t _peek16(uint32_t pc, uint8_t off) {
    return (uint16_t)vm_code_buffer[pc + off]
         | ((uint16_t)vm_code_buffer[pc + off + 1] << 8);
}
static inline uint32_t _peek32(uint32_t pc, uint8_t off) {
    return (uint32_t)vm_code_buffer[pc + off]
         | ((uint32_t)vm_code_buffer[pc + off + 1] << 8)
         | ((uint32_t)vm_code_buffer[pc + off + 2] << 16)
         | ((uint32_t)vm_code_buffer[pc + off + 3] << 24);
}

// Resolve function display name into buf.
// idx_off: byte offset from opcode byte to the u16 idx field.
static void _peek_fn_name(uint32_t pc, uint8_t idx_off, char *buf, size_t bufsz) {
    uint16_t      idx = _peek16(pc, idx_off);
    VM_FuncEntry *fn  = &vm_functions[idx];
    if (fn->kind == VM_FUNC_PY)
        snprintf(buf, bufsz, "%s.%s",
                 fn->py_module ? fn->py_module : "?",
                 fn->py_name   ? fn->py_name   : "?");
    else
        snprintf(buf, bufsz, "%s",
                 fn->c_name ? fn->c_name : "?");
}

// ─────────────────────────────────────────────────────────────────────────────
//  _fmt_operands
//  Builds a human-readable operand + payload string for any opcode.
//  Reads entirely from vm_code_buffer via peek — no PC mutation.
//  Returns buf (empty string if the opcode carries no operands).
// ─────────────────────────────────────────────────────────────────────────────
static const char *_fmt_operands(uint8_t opcode, uint32_t pc,
                                  char *buf, size_t bufsz)
{
    buf[0] = '\0';
    int  n = 0;
    char fn[48];

    switch (opcode) {

    case 0x13: /* LITERAL_UINT32 — u32 value */ {
        uint32_t v = _peek32(pc, 1);
        snprintf(buf, bufsz,
            "value=0x%08lX (%lu)  raw: %02X %02X %02X %02X",
            (unsigned long)v, (unsigned long)v,
            _peek8(pc,1), _peek8(pc,2), _peek8(pc,3), _peek8(pc,4));
        break;
    }

    case 0x80: /* EXE — u16 idx */ {
        uint16_t idx = _peek16(pc, 1);
        _peek_fn_name(pc, 1, fn, sizeof(fn));
        snprintf(buf, bufsz,
            "fn[%u]=%s  raw: %02X %02X",
            idx, fn, _peek8(pc,1), _peek8(pc,2));
        break;
    }

    case 0x81: /* EXE_ARGS — u8 argc, u16 idx, (argc-2) payload bytes */ {
        uint8_t  argc    = _peek8 (pc, 1);
        uint16_t idx     = _peek16(pc, 2);
        uint8_t  pay_len = (argc >= 2) ? (argc - 2) : 0;
        _peek_fn_name(pc, 2, fn, sizeof(fn));

        n = snprintf(buf, bufsz,
            "fn[%u]=%s argc=%u  raw: %02X %02X %02X",
            idx, fn, argc,
            _peek8(pc,1), _peek8(pc,2), _peek8(pc,3));

        if (pay_len > 0 && n < (int)bufsz) {
            uint8_t print_len = pay_len < VM_DBG_MAX_PAYLOAD_BYTES
                              ? pay_len : VM_DBG_MAX_PAYLOAD_BYTES;
            n += snprintf(buf + n, bufsz - n, " |");
            for (uint8_t i = 0; i < print_len && n < (int)bufsz; i++)
                n += snprintf(buf + n, bufsz - n, " %02X", _peek8(pc, 4 + i));
            if (pay_len > VM_DBG_MAX_PAYLOAD_BYTES && n < (int)bufsz)
                snprintf(buf + n, bufsz - n, " ...");
        }
        break;
    }

    case 0xE8: /* PUSH — u16 size */ {
        uint16_t size = _peek16(pc, 1);
        snprintf(buf, bufsz,
            "size=%u  raw: %02X %02X",
            size, _peek8(pc,1), _peek8(pc,2));
        break;
    }

    case 0xA0: /* GROUP_END — u16 count */ {
        uint16_t count = _peek16(pc, 1);
        snprintf(buf, bufsz,
            "count=%u  raw: %02X %02X",
            count, _peek8(pc,1), _peek8(pc,2));
        break;
    }

    case 0xD0: /* GOTO — u32 target */ {
        uint32_t target = _peek32(pc, 1);
        snprintf(buf, bufsz,
            "-> 0x%08lX  raw: %02X %02X %02X %02X",
            (unsigned long)target,
            _peek8(pc,1), _peek8(pc,2), _peek8(pc,3), _peek8(pc,4));
        break;
    }

    case 0x88: /* MACRO_CALL — u32 target */ {
        uint32_t target = _peek32(pc, 1);
        snprintf(buf, bufsz,
            "-> 0x%08lX  raw: %02X %02X %02X %02X",
            (unsigned long)target,
            _peek8(pc,1), _peek8(pc,2), _peek8(pc,3), _peek8(pc,4));
        break;
    }

    case 0x90: /* SLOT_BODY — u32 addr */ {
        uint32_t addr = _peek32(pc, 1);
        snprintf(buf, bufsz,
            "addr=0x%08lX  raw: %02X %02X %02X %02X",
            (unsigned long)addr,
            _peek8(pc,1), _peek8(pc,2), _peek8(pc,3), _peek8(pc,4));
        break;
    }

    case 0x91: /* SLOT_CALL — u32 stack offset */ {
        uint32_t off = _peek32(pc, 1);
        snprintf(buf, bufsz,
            "stack_off=%lu  raw: %02X %02X %02X %02X",
            (unsigned long)off,
            _peek8(pc,1), _peek8(pc,2), _peek8(pc,3), _peek8(pc,4));
        break;
    }

    case 0x21: /* READ_ARG — u32 offset */ {
        uint32_t off = _peek32(pc, 1);
        snprintf(buf, bufsz,
            "off=%lu  raw: %02X %02X %02X %02X",
            (unsigned long)off,
            _peek8(pc,1), _peek8(pc,2), _peek8(pc,3), _peek8(pc,4));
        break;
    }

    case 0xB1: /* POINT_OFFSET — u16 offset */ {
        uint16_t off = _peek16(pc, 1);
        snprintf(buf, bufsz,
            "off=%u  raw: %02X %02X",
            off, _peek8(pc,1), _peek8(pc,2));
        break;
    }

    // NOP=0x00, POP=0xE0, MACRO_RETURN=0x8F, SLOT_RETURN=0x9F, END=0xFF
    // carry no operands — leave buf empty
    default:
        break;
    }

    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
//  _vm_debug_check  — dispatch-time hook, called from DISPATCH macro
// ─────────────────────────────────────────────────────────────────────────────
bool _vm_debug_check(uint8_t task_id, uint32_t pc, uint8_t opcode,
                     uint32_t stack_ptr)
{
    if (!vm_debug.enabled)       return false;
    if (!_task_matches(task_id)) return false;

    // Already paused for this task — stay blocked until step is requested
    if (vm_debug.paused && vm_debug.paused_task_id == task_id) {
        if (vm_debug.step_requested) {
            vm_debug.step_requested = false;
            vm_debug.paused         = false;
            // fall through: execute this opcode, re-evaluate pause below
        } else {
            return true;
        }
    }

    // ── Trace line with inline operands ─────────────────────────────────────
    char ops_buf[128];
    const char *ops = _fmt_operands(opcode, pc, ops_buf, sizeof(ops_buf));

    if (ops[0] != '\0')
        mp_printf(MP_PYTHON_PRINTER,
            "[VM DBG] task=%u  PC=0x%08lX  OP=0x%02X (%-14s)  SP=%lu  %s\n",
            task_id, (unsigned long)pc, opcode, _opcode_name(opcode),
            (unsigned long)stack_ptr, ops);
    else
        mp_printf(MP_PYTHON_PRINTER,
            "[VM DBG] task=%u  PC=0x%08lX  OP=0x%02X (%s)  SP=%lu\n",
            task_id, (unsigned long)pc, opcode, _opcode_name(opcode),
            (unsigned long)stack_ptr);

    // ── Pause logic ──────────────────────────────────────────────────────────
    const char *pause_reason = NULL;
    if      (vm_debug.step_mode)   pause_reason = "step";
    else if (_is_pc_break(pc))     pause_reason = "pc_break";
    else if (_is_op_break(opcode)) pause_reason = "op_break";

    if (pause_reason) {
        vm_debug.paused         = true;
        vm_debug.paused_task_id = task_id;
        vm_debug.paused_pc      = pc;
        vm_debug.paused_opcode  = opcode;
        _print_pause_header(pause_reason);
        if (vm_debug.stack_on_pause) _dump_stack(stack_ptr);
        mp_printf(MP_PYTHON_PRINTER,
            "[VM DBG] Call vm_debug_step() to advance one opcode.\n\n");
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  _vm_debug_log_sp_after  — called from COMPLETE() macro
// ─────────────────────────────────────────────────────────────────────────────
void _vm_debug_log_sp_after(uint8_t task_id, uint32_t sp_after) {
    if (!vm_debug.enabled)       return;
    if (!_task_matches(task_id)) return;
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG]                                                SP->%lu\n",
        (unsigned long)sp_after);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void vm_debug_enable(int8_t task_filter) {
    vm_debug.task_filter = task_filter;
    vm_debug.enabled     = true;
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] Enabled. task_filter=%d  (%s)\n",
        task_filter,
        task_filter < 0 ? "all tasks" : "single task");
}

void vm_debug_disable(void) {
    vm_debug.enabled        = false;
    vm_debug.paused         = false;
    vm_debug.step_requested = false;
    mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Disabled.\n");
}

void vm_debug_step_mode_enable(void) {
    vm_debug.step_mode = true;
    mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Step mode ON.\n");
}

void vm_debug_step_mode_disable(void) {
    vm_debug.step_mode = false;
    mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Step mode OFF.\n");
}

void vm_debug_step(void) {
    if (!vm_debug.paused) {
        mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Not paused — nothing to step.\n");
        return;
    }
    vm_debug.step_requested = true;
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] Step -> will execute OP=0x%02X (%s) at PC=0x%08lX\n",
        vm_debug.paused_opcode,
        _opcode_name(vm_debug.paused_opcode),
        (unsigned long)vm_debug.paused_pc);
}

// ── PC breakpoints ────────────────────────────────────────────────────────────

bool vm_debug_add_pc_break(uint32_t pc) {
    if (vm_debug.pc_break_count >= VM_DBG_MAX_PC_BREAKS) {
        mp_printf(MP_PYTHON_PRINTER, "[VM DBG] PC breakpoint table full.\n");
        return false;
    }
    for (uint8_t i = 0; i < vm_debug.pc_break_count; i++)
        if (vm_debug.pc_breaks[i] == pc) return true;
    vm_debug.pc_breaks[vm_debug.pc_break_count++] = pc;
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] PC breakpoint added: 0x%08lX\n", (unsigned long)pc);
    return true;
}

bool vm_debug_remove_pc_break(uint32_t pc) {
    for (uint8_t i = 0; i < vm_debug.pc_break_count; i++) {
        if (vm_debug.pc_breaks[i] == pc) {
            vm_debug.pc_breaks[i] = vm_debug.pc_breaks[--vm_debug.pc_break_count];
            mp_printf(MP_PYTHON_PRINTER,
                "[VM DBG] PC breakpoint removed: 0x%08lX\n", (unsigned long)pc);
            return true;
        }
    }
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] PC breakpoint not found: 0x%08lX\n", (unsigned long)pc);
    return false;
}

void vm_debug_clear_pc_breaks(void) {
    vm_debug.pc_break_count = 0;
    mp_printf(MP_PYTHON_PRINTER, "[VM DBG] All PC breakpoints cleared.\n");
}

// ── Opcode breakpoints ────────────────────────────────────────────────────────

bool vm_debug_add_op_break(uint8_t opcode) {
    if (vm_debug.op_break_count >= VM_DBG_MAX_OP_BREAKS) {
        mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Opcode breakpoint table full.\n");
        return false;
    }
    for (uint8_t i = 0; i < vm_debug.op_break_count; i++)
        if (vm_debug.op_breaks[i] == opcode) return true;
    vm_debug.op_breaks[vm_debug.op_break_count++] = opcode;
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] Opcode breakpoint added: 0x%02X (%s)\n",
        opcode, _opcode_name(opcode));
    return true;
}

bool vm_debug_remove_op_break(uint8_t opcode) {
    for (uint8_t i = 0; i < vm_debug.op_break_count; i++) {
        if (vm_debug.op_breaks[i] == opcode) {
            vm_debug.op_breaks[i] = vm_debug.op_breaks[--vm_debug.op_break_count];
            mp_printf(MP_PYTHON_PRINTER,
                "[VM DBG] Opcode breakpoint removed: 0x%02X (%s)\n",
                opcode, _opcode_name(opcode));
            return true;
        }
    }
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] Opcode breakpoint not found: 0x%02X\n", opcode);
    return false;
}

void vm_debug_clear_op_breaks(void) {
    vm_debug.op_break_count = 0;
    mp_printf(MP_PYTHON_PRINTER, "[VM DBG] All opcode breakpoints cleared.\n");
}

// ── Inspect ───────────────────────────────────────────────────────────────────

void vm_debug_inspect_pc(void) {
    if (!vm_debug.paused) {
        mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Not paused — no snapshot available.\n");
        return;
    }
    mp_printf(MP_PYTHON_PRINTER,
        "[VM DBG] Paused task=%u  PC=0x%08lX  OP=0x%02X (%s)\n",
        vm_debug.paused_task_id,
        (unsigned long)vm_debug.paused_pc,
        vm_debug.paused_opcode,
        _opcode_name(vm_debug.paused_opcode));
}

void vm_debug_inspect_stack(void) {
    if (!vm_debug.paused) {
        mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Not paused — no snapshot available.\n");
        return;
    }
    VM_TaskInstance *t = &vm_task_instances[vm_debug.paused_task_id];
    _dump_stack(t->STACK_PTR);
}

void vm_debug_inspect_all_tasks(void) {
    mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Task status:\n");
    for (uint8_t i = 0; i < TASKS_MAX; i++) {
        VM_TaskInstance *t = &vm_task_instances[i];
        mp_printf(MP_PYTHON_PRINTER,
            "  [%u] enabled=%u  PC=0x%08lX  SP=%lu  first_start=%u\n",
            i,
            t->FLAGS.enabled,
            (unsigned long)t->PC,
            (unsigned long)t->STACK_PTR,
            t->FLAGS.is_first_start);
    }
}

void vm_debug_list_breaks(void) {
    mp_printf(MP_PYTHON_PRINTER, "[VM DBG] PC breakpoints (%u):\n",
        vm_debug.pc_break_count);
    for (uint8_t i = 0; i < vm_debug.pc_break_count; i++)
        mp_printf(MP_PYTHON_PRINTER,
            "  [%u] 0x%08lX\n", i, (unsigned long)vm_debug.pc_breaks[i]);
    mp_printf(MP_PYTHON_PRINTER, "[VM DBG] Opcode breakpoints (%u):\n",
        vm_debug.op_break_count);
    for (uint8_t i = 0; i < vm_debug.op_break_count; i++)
        mp_printf(MP_PYTHON_PRINTER,
            "  [%u] 0x%02X (%s)\n", i,
            vm_debug.op_breaks[i],
            _opcode_name(vm_debug.op_breaks[i]));
}