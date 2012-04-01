/*
 * eval.cc
 *
 *  Created on: Mar 31, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

extern "C" {
#   include <simple.h>
}

#include <map>
#include <cassert>
#include <stdint.h>

#include "include/optimizer.h"
#include "include/cfg.h"
#include "include/basic_block.h"
#include "include/operator.h"
#include "include/instr.h"

typedef std::map<simple_sym *, simple_instr *> branch_map;

/// check to see if this code can be interpreted at compile time
static bool is_interpretable(simple_instr *in) throw() {
    for(; 0 != in; in = in->next) {
        switch(in->opcode) {
        case CALL_OP: case LOAD_OP: case STR_OP: case MCPY_OP:
            return false;
        case LDC_OP:
            if(IMMED_SYMBOL == in->u.ldc.value.format) {
                return false;
            }
            continue;
        default:
            continue;
        }
    }
    return true;
}

/// get the basic blocks associated with each label
static void get_branch_targets(cfg &flow, branch_map &bt) throw() {
    basic_block_iterator bb_it(flow.begin()), bb_end(flow.end());
    for(; bb_it != bb_end; ++bb_it) {
        simple_instr *in((*bb_it)->first);
        if(0 != in && LABEL_OP == in->opcode) {
            bt[in->u.label.lab] = in;
        }
    }
}

/// runtime value
struct value {
public:
    union {
        int as_int;
        unsigned as_uint;
        double as_float;
    } val;
    enum {
        INT, UNSIGNED, FLOAT
    } type;
};

struct eval_state {
public:
    std::map<simple_reg *, value> regs;
    branch_map branch_targets;
    value return_val;

    simple_instr *pc;
    simple_instr *ret_pc;

    bool has_return_val;
    bool has_returned;

    simple_type *return_type;
};

template <
    template <typename L, typename R, typename O>
    class OpFunctor,
    typename L0
>
class op_functor_filter {
public:
    typedef OpFunctor<L0,L0,L0> functor;
};

class useless_double_functor {
public:
    double operator()(const double &, const double &) throw() {
        return 0.0;
    }
};

#define USELESS_DOUBLE_FUNCTOR(the_op) \
    template <> \
    class op_functor_filter<op::the_op, double> { \
    public: \
        typedef useless_double_functor functor; \
    };

USELESS_DOUBLE_FUNCTOR(modulo)
USELESS_DOUBLE_FUNCTOR(bitwise_and)
USELESS_DOUBLE_FUNCTOR(bitwise_or)
USELESS_DOUBLE_FUNCTOR(bitwise_xor)

template <
    template <typename L, typename R, typename O>
    class OpFunctor
>
static value eval_binary_operator(eval_state &s, simple_reg *a0, simple_reg *a1) throw() {
    value &l(s.regs[a0]);
    value &r(s.regs[a1]);
    value ret;
    ret.type = l.type;

    typedef typename op_functor_filter<OpFunctor, int>::functor int_functor;
    typedef typename op_functor_filter<OpFunctor, unsigned>::functor unsigned_functor;
    typedef typename op_functor_filter<OpFunctor, double>::functor double_functor;

    switch(l.type) {
    case value::INT:
        ret.val.as_int = int_functor()(l.val.as_int, r.val.as_int);
        fprintf(stderr, "%d op %d = %d\n", l.val.as_int, r.val.as_int, ret.val.as_int);
        return ret;

    case value::UNSIGNED:
        ret.val.as_uint = unsigned_functor()(l.val.as_uint, r.val.as_uint);
        fprintf(stderr, "%u op %u = %u\n", l.val.as_uint, r.val.as_uint, ret.val.as_uint);
        return ret;

    case value::FLOAT:
        ret.val.as_float = double_functor()(l.val.as_float, r.val.as_float);
        fprintf(stderr, "%lf op %lf = %lf\n", l.val.as_float, r.val.as_float, ret.val.as_float);
        return ret;
    }
}

/// interpret a single instruction; returns true if execution should continue
static bool interpret_instruction(eval_state &s) throw() {
    simple_instr *in(s.pc);

    if(0 == in) {
        s.has_returned = false;
        return false;
    }

    // assume the next instruction should be the next "program counter"
    s.pc = in->next;

    // mostly a convenience; unsafe to use unless used in the right context!
    simple_reg *from_reg(in->u.base.src1);
    simple_reg *from_reg_alt(in->u.base.src2);
    simple_reg *to_reg(in->u.base.dst);

    switch(in->opcode) {
    case NOP_OP:
        return true;

    case RET_OP:
        if(0 != in->u.base.src1) {
            s.has_return_val = true;
            s.return_val = s.regs[in->u.base.src1];
        }
        s.has_returned = true;
        s.return_type = in->type;
        s.ret_pc = in;

        fprintf(stderr, "\n\n");
        return false;

    case STR_OP: assert(false);  return false;
    case MCPY_OP: assert(false); return false;

    case CPY_OP:
        s.regs[in->u.base.dst] = s.regs[in->u.base.src1];
        return true;

    case CVT_OP: {
        simple_type *from_type(from_reg->var->type);
        simple_type *to_type(to_reg->var->type);

        switch(from_type->base) {
        case SIGNED_TYPE:
            switch(to_type->base) {
            case SIGNED_TYPE:
                s.regs[to_reg] = s.regs[from_reg];
                return true;

            case UNSIGNED_TYPE:
                s.regs[to_reg].type = value::UNSIGNED;
                s.regs[to_reg].val.as_uint = (unsigned) s.regs[from_reg].val.as_int;
                return true;

            case FLOAT_TYPE:
                s.regs[to_reg].type = value::FLOAT;
                s.regs[to_reg].val.as_float = (double) s.regs[from_reg].val.as_float;
                return true;

            default: // abort
                return false;
            }
        case UNSIGNED_TYPE:
            switch(to_type->base) {
            case SIGNED_TYPE:
                s.regs[to_reg].type = value::INT;
                s.regs[to_reg].val.as_int = (int) s.regs[from_reg].val.as_uint;
                return true;

            case UNSIGNED_TYPE:
                s.regs[to_reg] = s.regs[from_reg];
                return true;

            case FLOAT_TYPE:
                s.regs[to_reg].type = value::FLOAT;
                s.regs[to_reg].val.as_float = (double) s.regs[from_reg].val.as_uint;
                return true;

            default: // abort
                return false;
            }
        case FLOAT_TYPE:
            switch(to_type->base) {
            case SIGNED_TYPE:
                s.regs[to_reg].type = value::INT;
                s.regs[to_reg].val.as_int = (int) s.regs[from_reg].val.as_float;
                return true;

            case UNSIGNED_TYPE:
                s.regs[to_reg].type = value::UNSIGNED;
                s.regs[to_reg].val.as_uint = (int) s.regs[from_reg].val.as_float;
                return true;

            case FLOAT_TYPE:
                s.regs[to_reg] = s.regs[from_reg];
                return true;

            default: // abort
                return false;
            }
        default: // abort
            return false;
        }
        break;
    }

    case NEG_OP:
        s.regs[to_reg] = s.regs[from_reg];
        switch(s.regs[from_reg].type) {
        case value::INT: case value::UNSIGNED:
            s.regs[to_reg].val.as_int *= -1;
            break;
        case value::FLOAT:
            s.regs[to_reg].val.as_float *= -1.0;
            break;
        }
        return true;

    case NOT_OP:
        s.regs[to_reg] = s.regs[from_reg];
        s.regs[to_reg].val.as_uint = ~(s.regs[to_reg].val.as_uint);
        return true;

    case LOAD_OP: assert(false);  return false;

    case ADD_OP:
        fprintf(stderr, "add ");
        s.regs[to_reg] = eval_binary_operator<op::add>(s, from_reg, from_reg_alt);
        return true;

    case SUB_OP:
        fprintf(stderr, "sub ");
        s.regs[to_reg] = eval_binary_operator<op::subtract>(s, from_reg, from_reg_alt);
        return true;

    case MUL_OP:
        fprintf(stderr, "mul ");
        s.regs[to_reg] = eval_binary_operator<op::multiply>(s, from_reg, from_reg_alt);
        return true;

    case DIV_OP:
        fprintf(stderr, "div ");
        s.regs[to_reg] = eval_binary_operator<op::divide>(s, from_reg, from_reg_alt);
        return true;

    case REM_OP:
        fprintf(stderr, "rem ");
        s.regs[to_reg] = eval_binary_operator<op::modulo>(s, from_reg, from_reg_alt);
        return true;

    case MOD_OP:
        s.regs[to_reg].type = s.regs[from_reg].type;
        s.regs[to_reg].val.as_int = op::mod(s.regs[from_reg].val.as_int, s.regs[from_reg_alt].val.as_int);
        return true;

    case AND_OP:
        fprintf(stderr, "and ");
        s.regs[to_reg] = eval_binary_operator<op::bitwise_and>(s, from_reg, from_reg_alt);
        return true;

    case IOR_OP:
        fprintf(stderr, "ior ");
        s.regs[to_reg] = eval_binary_operator<op::bitwise_or>(s, from_reg, from_reg_alt);
        return true;

    case XOR_OP:
        fprintf(stderr, "xor ");
        s.regs[to_reg] = eval_binary_operator<op::bitwise_xor>(s, from_reg, from_reg_alt);
        return true;

    case ASR_OP:
        s.regs[to_reg].type = s.regs[from_reg].type;
        s.regs[to_reg].val.as_int = op::asr(s.regs[from_reg].val.as_int, s.regs[from_reg_alt].val.as_int);
        return true;

    case LSL_OP:
        s.regs[to_reg].type = s.regs[from_reg].type;
        s.regs[to_reg].val.as_int = op::lsl(s.regs[from_reg].val.as_int, s.regs[from_reg_alt].val.as_int);
        return true;

    case LSR_OP:
        s.regs[to_reg].type = s.regs[from_reg].type;
        s.regs[to_reg].val.as_int = op::lsr(s.regs[from_reg].val.as_int, s.regs[from_reg_alt].val.as_int);
        return true;

    case ROT_OP:
        s.regs[to_reg].type = s.regs[from_reg].type;
        s.regs[to_reg].val.as_int = op::rot(s.regs[from_reg].val.as_int, s.regs[from_reg_alt].val.as_int);
        return true;

    case SEQ_OP:
        fprintf(stderr, "seq ");
        s.regs[to_reg] = eval_binary_operator<op::equal>(s, from_reg, from_reg_alt);
        return true;

    case SNE_OP:
        fprintf(stderr, "sne ");
        s.regs[to_reg] = eval_binary_operator<op::not_equal>(s, from_reg, from_reg_alt);
        return true;

    case SL_OP:
        fprintf(stderr, "sl ");
        s.regs[to_reg] = eval_binary_operator<op::less_than>(s, from_reg, from_reg_alt);
        return true;

    case SLE_OP:
        fprintf(stderr, "sle ");
        s.regs[to_reg] = eval_binary_operator<op::less_than_equal>(s, from_reg, from_reg_alt);
        return true;

    case JMP_OP:
        s.pc = s.branch_targets[in->u.label.lab];
        return true;

    case BTRUE_OP:
        from_reg = in->u.bj.src;
        if(s.regs[from_reg].val.as_int) {
            s.pc = s.branch_targets[in->u.bj.target];
        }
        return true;

    case BFALSE_OP:
        from_reg = in->u.bj.src;
        if(!(s.regs[from_reg].val.as_int)) {
            s.pc = s.branch_targets[in->u.bj.target];
        }
        return true;

    case LDC_OP:
        to_reg = in->u.ldc.dst;
        switch(in->u.ldc.value.format) {
        case IMMED_INT:
            s.regs[to_reg].type = value::INT;
            s.regs[to_reg].val.as_int = in->u.ldc.value.u.ival;
            return true;
        case IMMED_FLOAT:
            s.regs[to_reg].type = value::FLOAT;
            s.regs[to_reg].val.as_float = in->u.ldc.value.u.fval;
            return true;
        case IMMED_SYMBOL:
            assert(false);
            return false;
        }
        break;

    case CALL_OP: assert(false);  return false;

    case MBR_OP: {
        from_reg = in->u.mbr.src;
        int64_t result(0);
        if(UNSIGNED_TYPE == from_reg->var->type->base) {
            result = s.regs[from_reg].val.as_uint;
        } else {
            result = s.regs[from_reg].val.as_int;
        }
        result -= in->u.mbr.offset;

        if(result < 0 || in->u.mbr.ntargets < result) {
            s.pc = s.branch_targets[in->u.mbr.deflab];
        } else {
            s.pc = s.branch_targets[in->u.mbr.targets[result]];
        }

        return true;
    }

    case LABEL_OP:
        return true;

    default:
        break;
    }

    return false;
}

/// evaluate functions that are pure and make no use of their actual arguments
void eval_pure_function(optimizer &o, cfg &flow) throw() {
#ifndef ECE540_DISABLE_EVAL
    simple_instr *first_instr(o.first_instruction());

    if(is_interpretable(first_instr)) {
        eval_state state;
        state.has_return_val = false;
        state.has_returned = false;
        state.pc = first_instr;
        state.return_type = 0;

        get_branch_targets(flow, state.branch_targets);

        for(; interpret_instruction(state); ) {
            // ...
        }

        // weird
        if(!state.has_returned) {
            return;
        }

        o.changed_block();
        o.changed_def();
        o.changed_use();

        // function should just be a single return op
        if(!state.has_return_val) {
            first_instr->opcode = RET_OP;
            first_instr->type = state.return_type;
            first_instr->u.base.dst = 0;
            first_instr->u.base.src1 = 0;
            first_instr->u.base.src2 = 0;
        }

        first_instr->type = state.return_type;
        first_instr->opcode = LDC_OP;

        // we have a return val; let's inject it in
        simple_instr *new_ret(new_instr(RET_OP, state.return_type));
        simple_instr *ret(state.ret_pc);

        assert(0 != ret);
        assert(RET_OP == ret->opcode);

        simple_reg *returned_reg(ret->u.base.src1);
        simple_reg *new_ret_reg(new_register(returned_reg->var->type, TEMP_REG));

        first_instr->type = returned_reg->var->type;

        switch(state.regs[returned_reg].type) {
        case value::INT: case value::UNSIGNED:
            first_instr->u.ldc.value.format = IMMED_INT;
            first_instr->u.ldc.value.u.ival = state.regs[returned_reg].val.as_int;
            break;
        case value::FLOAT:
            first_instr->u.ldc.value.format = IMMED_FLOAT;
            first_instr->u.ldc.value.u.fval = state.regs[returned_reg].val.as_float;
            break;
        }

        first_instr->u.ldc.dst = new_ret_reg;
        new_ret->u.base.src1 = new_ret_reg;

        instr::insert_after(new_ret, first_instr);
    }
#endif
}


