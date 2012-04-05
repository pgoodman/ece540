/*
 * cf.cc
 *
 *  Created on: Mar 18, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_CF_CC_
#define project_CF_CC_

#include <map>
#include <vector>
#include <cstring>
#include <cassert>
#include <stdint.h>
#include <cstdlib>

extern "C" {
#   include <simple.h>
}

#include "include/opt/cf.h"

#include "include/cfg.h"
#include "include/optimizer.h"
#include "include/diag.h"
#include "include/basic_block.h"
#include "include/instr.h"
#include "include/operator.h"

#include "include/data_flow/var_use.h"
#include "include/data_flow/var_def.h"

/// maintains a mapping of non-floating-point-type temporary registers
/// whose values are loaded with constants
struct cf_state {

    /// map of temporary registers to the values they contain
    std::map<simple_reg *, int> constants;

    /// map of non-temporary registers to the values they currently contain
    std::map<simple_reg *, int> peephole;

    /// update the result for what was done to the cfg
    optimizer *opt;

    /// true iff we should keep looking for constants
    bool keep_looking_for_constants;

private:

    template <typename T>
    bool get_constant_impl(simple_reg *reg, T &constant) throw() {
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

public:

    /// return true iff a constant is stored in the register. if the register
    /// contains a constant, assign the constant to the variable constant passed
    /// in by reference
    bool get_constant(simple_reg *reg, int &constant) throw() {
        return get_constant_impl(reg, constant);
    }

    bool get_constant(simple_reg *reg, unsigned &constant) throw() {
        return get_constant_impl(reg, constant);
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
    typename T0, typename T1,
    int (*operator_func)(const T0 &, const T1 &),
    bool (*check_args)(T0, T1)
>
class binary {
public:
    static bool fold(
        cf_state &state,
        simple_reg *left_reg,
        simple_reg *right_reg,
        int &result
    ) throw() {
        T0 left_val(0);
        T1 right_val(0);
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
    bool accept_args(int, unsigned) throw() {
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
};

/// table of functions to fold binary operators
typedef bool (fold_op_func)(cf_state &, simple_reg *, simple_reg *, int &);
fold_op_func *fold_funcs[] = {
    binary<int,int,binary_operator<int>::add, op::accept_args>::fold, // ADD_OP
    binary<int,int,binary_operator<int>::subtract, op::accept_args>::fold, // SUB_OP
    binary<int,int,binary_operator<int>::multiply, op::accept_args>::fold, // MUL_OP
    binary<int,int,binary_operator<int>::divide, op::accept_nonzero_right>::fold, // DIV_OP
    binary<int,int,binary_operator<int>::modulo, op::accept_nonzero_right>::fold, // REM_OP
    binary<int,int,op::mod, op::accept_nonzero_right>::fold, // MOD_OP
    binary<int,int,binary_operator<int>::bitwise_and, op::accept_args>::fold, // AND_OP
    binary<int,int,binary_operator<int>::bitwise_or, op::accept_args>::fold, // IOR_OP
    binary<int,int,binary_operator<int>::bitwise_xor, op::accept_args>::fold, // XOR_OP
    binary<int,unsigned,op::asr, op::accept_args>::fold, // ASR_OP
    binary<int,unsigned,op::lsl, op::accept_args>::fold, // LSL_OP
    binary<int,unsigned,op::lsr, op::accept_args>::fold, // LSR_OP
    binary<int,int,op::rot, op::accept_args>::fold, // ROT_OP
    binary<int,int,binary_operator<int>::equal, op::accept_args>::fold, // SEQ_OP
    binary<int,int,binary_operator<int>::not_equal, op::accept_args>::fold, // SNE_OP
    binary<int,int,binary_operator<int>::less_than, op::accept_args>::fold, // SL_OP
    binary<int,int,binary_operator<int>::less_than_equal, op::accept_args>::fold, // SLE_OP
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

        bool updated_locally(false);
        int result(0);
        int64_t result64(0);

        switch(in->opcode) {

        // branch true
        case BTRUE_OP:
            if(!state.get_constant(in->u.bj.src, result)) {
                continue;
            }

            if(result) {
                in->opcode = JMP_OP;
                in->u.bj.src = 0;
            } else {
                in->opcode = NOP_OP;
            }

            state.opt->changed_block();
            continue;

        // branch false
        case BFALSE_OP:
            if(!state.get_constant(in->u.bj.src, result)) {
                continue;
            }

            if(!result) {
                in->opcode = JMP_OP;
                in->u.bj.src = 0;
            } else {
                in->opcode = NOP_OP;
            }

            state.opt->changed_block();
            continue;

        // multi-way branch
        case MBR_OP:
            if(!state.get_constant(in->u.bj.src, result)) {
                continue;
            }

            // try not to lose precision when using unsigned/signed types as
            // the source of the mbr
            if(UNSIGNED_TYPE == in->u.bj.src->var->type->base) {
                unsigned uresult(0);
                memcpy(&uresult, &result, sizeof uresult);
                result64 = uresult;
            } else {
                result64 = result;
            }

            result64 -= in->u.mbr.offset;

            // outside the range of valid offsets, jump to default
            if(result64 < 0 || in->u.mbr.ntargets < result64) {
                simple_sym *default_target(in->u.mbr.deflab);
                in->opcode = JMP_OP;
                in->u.bj.target = default_target;

            // jump to specific target at the offset
            } else {
                simple_sym *target(in->u.mbr.targets[result64]);
                in->opcode = JMP_OP;
                in->u.bj.target = target;
            }

            state.opt->changed_block();
            continue;

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
            if(!instr::is_expression(in)) {
                continue;
            }

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

            state.opt->changed_def();

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

            instr::insert_before(lin, in);

            // make sure blocks are kept consistent
            if(bb->first == in) {
                bb->first = lin;
            }

            state.update(ldest, result);
            state.update(dest, result);
        }

        state.opt->changed_use();
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
        state.keep_looking_for_constants = true;
    }

    return true;
}

namespace std {

    /// define a strict weak ordering for immediate constants
    template <>
    struct less<simple_immed> {
    public:
        bool operator()(const simple_immed &a, const simple_immed &b) const throw() {
            if(a.format < b.format) {
                return true;
            } else if(a.format > b.format) {
                return false;
            }

            switch(a.format) {
            case IMMED_INT:
                return a.u.ival < b.u.ival;

            case IMMED_FLOAT:
                return a.u.fval < b.u.fval;

            case IMMED_SYMBOL:
                if(a.u.s.symbol < b.u.s.symbol) {
                    return true;
                } else if(a.u.s.symbol > b.u.s.symbol) {
                    return false;
                } else {
                    return a.u.s.offset < b.u.s.offset;
                }
                return true;
            }

            return true;
        }
    };
}

/// remap a temporary register
static void remap_temp_reg(
    simple_reg *reg,
    simple_reg **use,
    simple_instr *,
    std::map<simple_reg *, simple_reg *> &remapped_regs
) throw() {
    if(TEMP_REG == reg->kind) {
        simple_reg *remapped_reg(remapped_regs[reg]);
        if(0 != remapped_reg) {
            *use = remapped_reg;
        }
    }
}

/// look for multiple LDC's of the same constant and combine them into a single
/// LDC of that constant
static bool combine_constants(basic_block *bb, optimizer &o) throw() {
    if(0 == bb->last) {
        return true;
    }

    typedef std::map<simple_immed, std::vector<simple_instr *> > constant_instr_map;
    constant_instr_map constant_ins;

    // go collect all constants in a basic block, mapping to the instructions
    // that load them, in the order that the instructions appear
    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
        assert(0 != in);

        if(LDC_OP == in->opcode) {
            constant_ins[in->u.ldc.value].push_back(in);
        }
    }

    constant_instr_map::iterator c_it(constant_ins.begin())
                               , c_end(constant_ins.end());

    for(; c_it != c_end; ++c_it) {
        std::vector<simple_instr *> &instrs(c_it->second);

        // only one use of this constant
        if(1U == instrs.size()) {
            continue;
        }

        o.changed_def();
        o.changed_use();

        simple_instr *first_ldc(instrs[0]);
        simple_reg *new_loc(new_register(
            first_ldc->u.ldc.dst->var->type,
            PSEUDO_REG
        ));

        // copy the constant into a new register
        simple_instr *cpy_instr(new_instr(CPY_OP, new_loc->var->type));
        cpy_instr->u.base.src1 = first_ldc->u.ldc.dst;
        cpy_instr->u.base.dst = new_loc;

        // add the instruction in
        instr::insert_after(cpy_instr, first_ldc);

        // create a map of registers that need to be replaced, and kill the
        // other LDCs
        std::map<simple_reg *, simple_reg *> remapped_regs;
        remapped_regs[first_ldc->u.ldc.dst] = new_loc;
        for(unsigned i(1U); i < instrs.size(); ++i) {
            remapped_regs[instrs[i]->u.ldc.dst] = new_loc;
            instrs[i]->opcode = NOP_OP;
        }

        // replace all uses of the LDC registers with their equivalent remmaped
        // ones
        for(simple_instr *in(cpy_instr->next); in != bb->last->next; in = in->next) {
            assert(0 != in);
            for_each_var_use(remap_temp_reg, in, remapped_regs);
        }
    }

    return true;
}

/// apply the constant folding optimization to a control flow graph. returns
/// true if the graph was updated.
void fold_constants(optimizer &opt, cfg &graph) throw() {
    if(0 != getenv("ECE540_DISABLE_CF")) {
        return;
    }

    cf_state state;
    state.opt = &opt;

    graph.for_each_basic_block(combine_constants, opt);
    graph.for_each_basic_block(find_constants, state);

    if(!state.constants.empty()) {
        state.keep_looking_for_constants = true;

        while(state.keep_looking_for_constants) {
            state.keep_looking_for_constants = false;
            graph.for_each_basic_block(find_temp_copies, state);
        }

        graph.for_each_basic_block(fold_constants, state);
    }
}

#endif /* project_CF_CC_ */
