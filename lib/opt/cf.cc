/*
 * cf.cc
 *
 *  Created on: Mar 18, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_CF_CC_
#define project_CF_CC_

extern "C" {
#   include <simple.h>
}

#include "include/opt/cf.h"

#include "include/cfg.h"
#include "include/diag.h"
#include "include/basic_block.h"
#include "include/instr.h"
#include "include/operator.h"
#include "include/data_flow/var_def.h"

#include <map>
#include <cstring>

/// maintains a mapping of non-floating-point-type temporary registers
/// whose values are loaded with constants
struct cf_state {

    /// map of temporary registers to the values they contain
    std::map<simple_reg *, int> constants;

    /// map of non-temporary registers to the values they currently contain
    std::map<simple_reg *, int> peephole;

    /// true iff any constant folding was applied
    bool updated;

    /// return true iff a constant is stored in the register. if the register
    /// contains a constant, assign the constant to the variable constant passed
    /// in by reference
    bool get_constant(simple_reg *reg, int &constant) throw() {
        if(0U != peephole.count(reg)) {
            constant = peephole[reg];
            return true;
        }

        if(0U != constants.count(reg)) {
            constant = constants[reg];
            return true;
        }

        return false;
    }

    /// clear the peephole cache
    void clear(void) throw() {
        peephole.clear();
    }

    /// update the set of constants
    void update(simple_reg *reg, int value) throw() {
        if(TEMP_REG == reg->kind) {
            constants[reg] = value;
        } else {
            peephole[reg] = value;
        }
    }

    /// does a simple form of local constant propagation for constant folding
    void peek(simple_instr *in) throw() {
        simple_reg *source(0), *dest(0);
        if(CPY_OP == in->opcode) {
            dest = in->u.base.dst;
            source = in->u.base.src1;

            // re-assign in peephole; inductive case
            if(peephole.count(source) && TEMP_REG != dest->kind) {
                peephole[dest] = peephole[source];

            // assign into peephold; base case
            } else if(constants.count(source)) {
                peephole[dest] = constants[source];
            }

        // any definition kills the register
        } else if(for_each_var_def(in, dest)) {
            peephole.erase(dest);
        }
    }
};

/// fold an abitrary binary operator
template <
    int (*operator_func)(const int &, const int &),
    bool (*check_args)(int, int)
>
class binary {
public:
    static bool fold(
        cf_state &state,
        simple_reg *left_reg,
        simple_reg *right_reg,
        int &result
    ) throw() {
        int left_val(0), right_val(0);
        if(state.get_constant(left_reg, left_val)
        && state.get_constant(right_reg, right_val)
        && check_args(left_val, right_val)) {
            result = operator_func(left_val, right_val);
            return true;
        }
        return false;
    }
};

namespace op {

    /// automatically accept the arguments as valid
    bool accept_args(int, int) throw() {
        return true;
    }

    /// check for non-zero right argument for modulo, divide
    bool accept_nonzero_right(int, int right) throw() {
        if(0 != right) {
            return true;
        }

        diag::warning("Denominator to DIV or REM must not be zero.\n");
        return false;
    }


    /// computes a modulo in terms of how SUIF sees it, i.e. always returning
    /// a non-negative integer
    int mod(const int &ll, const int &rr) throw() {
        int mod(ll % rr);
        if(0 > mod) {
            mod += rr;
        }
        return mod;
    }

    /// compute a logical right shift (fill in zeroes for high order bits)
    int lsr(const int &ll, const int &rr) throw() {
        if(rr > (sizeof ll * 8U)) {
            diag::warning("Right shift of size %d is too big.", rr);
            return ll >= 0 ? 0 : ~0;

        } else if(rr < 0) {
            diag::warning("Right shift of size %d is too small.", rr);
        }

        // TODO: issue on x < rr < num bits, where x is max size of shift right?
        return static_cast<int>(
            static_cast<unsigned>(ll) >> static_cast<unsigned>(rr)
        );
    }

    /// perform a logical left shift.
    /// TODO: worry about rr being too big?
    int lsl(const int &ll, const int &rr) throw() {
        return ll << rr;
    }

    enum {
        INT_NUM_BITS = sizeof(int) * 8U
    };

    /// compute an arithmetic right shift (use high order bit as fill)
    int asr(const int &ll, const int &rr) throw() {
        int lsr_(lsr(ll, rr));
        if(ll >= 0) {
            return lsr_;
        } else {
            return lsr_ | lsl(~0, INT_NUM_BITS - rr);
        }
    }

    /// compute a rotation of the bits
    int rot(const int &ll, const int &rr) throw() {
        unsigned ul(static_cast<unsigned>(ll));
        int sr(rr);

        if(rr > 0) { // left
            return static_cast<int>((ul << sr) | (ul >> (INT_NUM_BITS - sr)));

        } else if(0 == rr) { // nowhere
            return ll;

        } else { // right
            sr = -sr;
            return static_cast<int>((ul >> sr) | (ul << (INT_NUM_BITS - sr)));
        }
    }
};

/// table of functions to fold binary operators
typedef bool (fold_op_func)(cf_state &, simple_reg *, simple_reg *, int &);
fold_op_func *fold_funcs[] = {
    binary<binary_operator<int>::add, op::accept_args>::fold, // ADD_OP
    binary<binary_operator<int>::subtract, op::accept_args>::fold, // SUB_OP
    binary<binary_operator<int>::multiply, op::accept_args>::fold, // MUL_OP
    binary<binary_operator<int>::divide, op::accept_nonzero_right>::fold, // DIV_OP
    binary<binary_operator<int>::modulo, op::accept_nonzero_right>::fold, // REM_OP
    binary<op::mod, op::accept_nonzero_right>::fold, // MOD_OP
    binary<binary_operator<int>::bitwise_and, op::accept_args>::fold, // AND_OP
    binary<binary_operator<int>::bitwise_or, op::accept_args>::fold, // IOR_OP
    binary<binary_operator<int>::bitwise_xor, op::accept_args>::fold, // XOR_OP
    binary<op::asr, op::accept_args>::fold, // ASR_OP
    binary<op::lsl, op::accept_args>::fold, // LSL_OP
    binary<op::lsr, op::accept_args>::fold, // LSR_OP
    binary<op::rot, op::accept_args>::fold, // ROT_OP
    binary<binary_operator<int>::equal, op::accept_args>::fold, // SEQ_OP
    binary<binary_operator<int>::not_equal, op::accept_args>::fold, // SNE_OP
    binary<binary_operator<int>::less_than, op::accept_args>::fold, // SL_OP
    binary<binary_operator<int>::less_than_equal, op::accept_args>::fold, // SLE_OP
};

/// go fold constants in each block
static bool fold_constants(basic_block *bb, cf_state &state) throw() {
    simple_instr *in(bb->first);
    if(0 == in) {
        return true;
    }

    state.clear();

    const simple_instr *end(bb->last->next);
    for(; in != end; in = in->next) {
        state.peek(in);

        if(!instr::is_expression(in)) {
            continue;
        }

        bool updated_locally(false);
        int result(0);

        switch(in->opcode) {

        // type conversion, between signed and unsigned, no bits change
        case CVT_OP:
            if(state.get_constant(in->u.base.src1, result)) {
                updated_locally = true;
            }
            break;

        // unary minus
        case NEG_OP:
            if(state.get_constant(in->u.base.src1, result)) {
                updated_locally = true;
                result = -result;
            }
            break;

        // unary bitwise not
        case NOT_OP:
            if(state.get_constant(in->u.base.src1, result)) {
                updated_locally = true;
                result = ~result;
            }
            break;

        // any binary operator
        default:
            const int op(in->opcode - ADD_OP);
            if(fold_funcs[op](state, in->u.base.src1, in->u.base.src2, result)) {
                updated_locally = true;
            }
            break;
        }

        if(!updated_locally) {
            continue;
        }

        // we've computed a result
        simple_reg *dest(in->u.base.dst);

        // update the instruction in place
        if(TEMP_REG == dest->kind) {

            in->opcode = LDC_OP;
            in->u.ldc.dst = dest;
            in->u.ldc.value.format = IMMED_INT;
            in->u.ldc.value.u.ival = result;

            state.update(dest, result);

        // add in a new instruction and update the instruction
        // to be a cpy
        } else {

            simple_instr *lin(new_instr(LDC_OP, dest->var->type));
            simple_reg *ldest(new_register(dest->var->type, TEMP_REG));

            // fill in the load constant instruction
            lin->u.ldc.dst = dest;
            lin->u.ldc.value.format = IMMED_INT;
            lin->u.ldc.value.u.ival = result;

            // update the instruction to a copy
            in->opcode = CPY_OP;
            in->u.base.src1 = ldest;
            in->u.base.src2 = 0;

            // chain the new instruction in
            lin->next = in;
            lin->prev = in->prev;

            if(0 != in->prev) {
                in->prev->next = lin;
            }

            in->prev = lin;

            // make sure blocks are kept consistent
            if(bb->first == in) {
                bb->first = lin;
            }

            state.update(ldest, result);
            state.update(dest, result);
        }

        state.updated = true;
    }

    return true;
}

/// go find all temporary registers containing constants
static bool find_constants(basic_block *bb, cf_state &state) throw() {
    simple_instr *in(bb->first);
    if(0 == in) {
        return true;
    }

    const simple_instr *end(bb->last->next);
    for(; in != end; in = in->next) {
        if(LDC_OP != in->opcode
        || TEMP_REG != in->u.ldc.dst->kind
        || IMMED_INT != in->u.ldc.value.format) {
            continue;
        }

        state.constants[in->u.ldc.dst] = in->u.ldc.value.u.ival;
    }

    return true;
}

/// go find basic copies of temporary variables
static bool find_temp_copies(basic_block *bb, cf_state &state) throw() {
    simple_instr *in(bb->first);
    if(0 == in) {
        return true;
    }

    const simple_instr *end(bb->last->next);
    for(; in != end; in = in->next) {
        if(CPY_OP != in->opcode
        || TEMP_REG != in->u.base.dst->kind
        || TEMP_REG != in->u.base.src1->kind
        || 0U == state.constants.count(in->u.base.src1)) {
            continue;
        }

        state.constants[in->u.base.dst] = state.constants[in->u.base.src1];
        state.updated = true;
    }

    return true;
}

/// apply the constant folding optimization to a control flow graph. returns
/// true if the graph was updated.
bool fold_constants(cfg &graph) throw() {
    cf_state state;
    state.updated = false;

    graph.for_each_basic_block(find_constants, state);

    if(!state.constants.empty()) {
        state.updated = true;

        while(state.updated) {
            state.updated = false;
            graph.for_each_basic_block(find_temp_copies, state);
        }

        graph.for_each_basic_block(fold_constants, state);
    }

    return state.updated;
}

#endif /* project_CF_CC_ */
