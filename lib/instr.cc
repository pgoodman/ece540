/*
 * instr.cc
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cassert>

#include "include/instr.h"

namespace instr {

    /// return true iff some instruction is a jump instruction
    bool is_local_control_flow_transfer(const simple_instr *in) throw() {
        if(0 == in) {
            return false;
        }
        switch(in->opcode) {
        case JMP_OP: case BTRUE_OP: case BFALSE_OP: case MBR_OP:
            return true;
        default:
            return false;
        }
    }

    /// return true iff some instruction is a label
    bool is_label(const simple_instr *in) throw() {
        if(0 == in) {
            return false;
        }
        return LABEL_OP == in->opcode;
    }

    /// return true iff some instruction is a return instruction
    bool is_return(const simple_instr *in) throw() {
        if(0 == in) {
            return false;
        }
        return RET_OP == in->opcode;
    }

    /// replace a symbol in an instruction with another symbol, if the first
    /// symbol appears; return the number of times it was replaced. This won't
    /// replace a symbol as it appears in a label, only in jump operations.
    unsigned replace_symbol(
        simple_instr *in,
        const simple_sym *search,
        simple_sym *replace
    ) throw() {
        unsigned num(0U);

        if(0 == in) {
            return num;
        }

        switch(in->opcode) {
        case JMP_OP:
        case BTRUE_OP:
        case BFALSE_OP:
            if(search == in->u.bj.target) {
                in->u.bj.target = replace;
                ++num;
            }
            break;
        case MBR_OP:
            if(search == in->u.mbr.deflab) {
                in->u.mbr.deflab = replace;
                ++num;
            }
            for(unsigned i(0U); i < in->u.mbr.ntargets; ++i) {
                if(search == in->u.mbr.targets[i]) {
                    in->u.mbr.targets[i] = replace;
                    ++num;
                }
            }
            break;
        default:
            break;
        }
        return num;
    }

    /// return true iff some instruction jumps to a label with a specific
    /// symbol name
    bool jumps_to(const simple_instr *in, const simple_sym *sym) throw() {
        return 0U != replace_symbol(
            const_cast<simple_instr *>(in),
            sym,
            const_cast<simple_sym *>(sym)
        );
    }

    /// insert an instruction into the stream before naother one
    void insert_before(simple_instr *to_insert, simple_instr *before) throw() {
        assert(0 != to_insert);
        assert(0 != before);

        to_insert->next = before;
        to_insert->prev = before->prev;

        if(0 != before->prev) {
            before->prev->next = to_insert;
        }

        before->prev = to_insert;
    }

    /// insert an instruction into the stream after another one
    void insert_after(simple_instr *to_insert, simple_instr *after) throw() {
        assert(0 != to_insert);
        assert(0 != after);

        to_insert->next = after->next;
        to_insert->prev = after;

        if(0 != after->next) {
            after->next->prev = to_insert;
        }

        after->next = to_insert;
    }

    /// return true if execution of an instruction can fall through to the next
    /// instruction in the stream
    bool can_fall_through(const simple_instr *in) throw() {
        if(0 == in) {
            return true;
        }
        return can_transfer(in, in->next);
    }

    /// can the default execution (no jumps to labels) fall through to the
    /// next instruction?
    bool can_default_fall_through(const simple_instr *in) throw() {
        if(0 == in) {
            return true;
        }

        if(!is_local_control_flow_transfer(in)) {
            return true;
        }

        return (BTRUE_OP == in->opcode || BFALSE_OP == in->opcode);
    }

    /// return true if the first instruction can transfer to the second
    bool can_transfer(const simple_instr *in, const simple_instr *next) throw() {
        if(0 == in) {
            return 0 == next || 0 == next->prev;
        }

        if(!is_local_control_flow_transfer(in)
        && in->next == next) {
            return true;
        }

        // fall-through
        if(BTRUE_OP == in->opcode || BFALSE_OP == in->opcode) {
            if(in->next == next) {
                return true;
            }
        }

        // impossible to jump
        if(!is_label(next)) {
            return false;
        }

        return jumps_to(in, next->u.label.lab);
    }

    /// return true if the instruction has a destination register and it is used
    bool is_var_def(const simple_instr *in) throw() {
        if(0 == in) {
            return false;
        }
        switch(in->opcode) {
        case CPY_OP: case CVT_OP: case NEG_OP: case NOT_OP: case LOAD_OP:
        case ADD_OP: case SUB_OP: case MUL_OP: case DIV_OP: case REM_OP:
        case MOD_OP: case AND_OP: case IOR_OP: case XOR_OP: case ASR_OP:
        case LSL_OP: case LSR_OP: case ROT_OP: case SEQ_OP: case SNE_OP:
        case SL_OP: case SLE_OP: return true;
        case CALL_OP:
            if(0 != in->u.call.dst) {
                return true;
            }
            // fall-through
        default: return false;
        }
    }

    /// return true if the instruction represents an expression
    bool is_expression(const simple_instr *in) throw() {
        if(!is_var_def(in)) {
            return false;
        }

        switch(in->opcode) {
        case CPY_OP: case LOAD_OP: case CALL_OP:
            return false;
        default:;
        }

        /*
        // TODO: for assignment, seems that only pseudo regs should be
        //       considered
        if(PSEUDO_REG != in->u.base.src1->kind) {
            return false;
        }

        if(0 != in->u.base.src2 && PSEUDO_REG != in->u.base.src2->kind) {
            return false;
        }
        */

        return true;
    }
}


