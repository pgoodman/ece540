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
#include <exception>

#include "include/optimizer.h"
#include "include/operator.h"
#include "include/instr.h"
#include "include/unsafe_cast.h"

/// filters out specific operations that cannot be performed on floating point
/// numbers
template <
    template <typename L, typename R, typename O> class OpFunctor,
    typename L0
>
class binary_op_functor_filter {
public:
    typedef OpFunctor<L0,L0,L0> functor;
};

template <
    template <typename L, typename O> class OpFunctor,
    typename L0
>
class unary_op_functor_filter {
public:
    typedef OpFunctor<L0,L0> functor;
};

class useless_double_functor {
public:
    double operator()(const double &, const double &) throw() {
        return 0.0;
    }
    double operator()(const double &) throw() {
        return 0.0;
    }
};

/// perform a unary type cast from one type to another
template <typename SourceType, typename>
struct cast_to_int {
    int operator()(const SourceType &ii) throw() {
        return static_cast<int>(ii);
    }
};
template <typename SourceType, typename>
struct cast_to_unsigned {
    unsigned operator()(const SourceType &ii) throw() {
        return static_cast<unsigned>(ii);
    }
};
template <typename SourceType, typename>
struct cast_to_double {
    double operator()(const SourceType &ii) throw() {
        return static_cast<double>(ii);
    }
};

/// various SUIF-specific ops that are annoying
template <typename SourceType, typename, typename>
struct suif_mod {
    int operator()(const SourceType &ll, const SourceType &rr) throw() {
        return op::mod((int) ll, (int) rr);
    }
};

template <typename SourceType, typename, typename>
struct suif_lsr {
    int operator()(const SourceType &ll, const SourceType &rr) throw() {
        return op::lsr((int) ll, (unsigned) rr);
    }
};

template <typename SourceType, typename, typename>
struct suif_lsl {
    int operator()(const SourceType &ll, const SourceType &rr) throw() {
        return op::lsl((int) ll, (unsigned) rr);
    }
};

template <typename SourceType, typename, typename>
struct suif_asr {
    int operator()(const SourceType &ll, const SourceType &rr) throw() {
        return op::asr((int) ll, (unsigned) rr);
    }
};

template <typename SourceType, typename, typename>
struct suif_rot {
    int operator()(const SourceType &ll, const SourceType &rr) throw() {
        return op::rot((int) ll, (unsigned) rr);
    }
};

#define USELESS_DOUBLE_FUNCTOR(filter, the_op) \
    template <> \
    class filter<the_op, double> { \
    public: \
        typedef useless_double_functor functor; \
    };

USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, op::modulo)
USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, op::bitwise_and)
USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, op::bitwise_or)
USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, op::bitwise_xor)
USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, suif_mod)
USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, suif_lsr)
USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, suif_lsl)
USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, suif_asr)
USELESS_DOUBLE_FUNCTOR(binary_op_functor_filter, suif_rot)
USELESS_DOUBLE_FUNCTOR(unary_op_functor_filter, op::bitwise_not)
USELESS_DOUBLE_FUNCTOR(unary_op_functor_filter, op::negate)
#undef USELESS_DOUBLE_FUNCTOR

/// represents the base of a runtime value in the abstract interpreter
struct abstract_value {
public:
    int ref_count;
    simple_instr *instr;

    enum {
        INT, UNSIGNED, FLOAT, UNKNOWN
    } type;

    enum {
        VALUE, SYMBOL, EXPRESSION
    } kind;

    abstract_value(simple_instr *in) throw()
        : ref_count(1)
        , instr(in)
    { }

    virtual void union_symbols(std::set<simple_reg *> &) throw() = 0;
};

/// reference counting
static void inc_ref(abstract_value *val) throw() {
    ++(val->ref_count);
}

static void dec_ref(abstract_value *val) throw() {
    if(0 == --(val->ref_count)) {
        delete val;
    }
}

/// represents an abstract expression that depends on some symbolic values. this
/// is guaranteed to be a DAG by virtue of dependent_regs, which is the union
/// of all symbolic registers in all sub-expressions. If at any point one
/// assigns to symbolic register a value that depends on that register, the
/// abstract interpreter will bail out, thus maintaining the correct invariant.
struct symbolic_expression : public abstract_value {
public:

    enum {
        UNARY, BINARY
    } arity;

    std::set<simple_reg *> dependent_regs;

    abstract_value *left;
    abstract_value *right;

    /// constructor for binary expression
    symbolic_expression(simple_instr *in, abstract_value *a0, abstract_value *a1) throw()
        : abstract_value(in)
        , arity(BINARY)
        , left(a0)
        , right(a1)
    {
        a0->union_symbols(dependent_regs);
        a1->union_symbols(dependent_regs);

        inc_ref(a0);
        inc_ref(a1);

        this->type = a0->type;
        this->kind = EXPRESSION;
    }

    /// constructor for unary expression
    symbolic_expression(simple_instr *in, abstract_value *a0) throw()
        : abstract_value(in)
        , arity(UNARY)
        , left(a0)
        , right(0)
    {
        a0->union_symbols(dependent_regs);

        inc_ref(a0);

        this->type = a0->type;
        this->kind = EXPRESSION;
    }

    virtual ~symbolic_expression(void) throw() {
        dec_ref(left);
        left = 0;

        if(0 != right) {
            dec_ref(right);
            right = 0;
        }

        dependent_regs.clear();
    }

    virtual void union_symbols(std::set<simple_reg *> &syms) throw() {
        syms.insert(dependent_regs.begin(), dependent_regs.end());
    }
};

/// represents a single value. The value is known at compile time.
struct concrete_value : public abstract_value {
public:

    union {
        int as_int;
        unsigned as_uint;
        double as_float;
    } value;

    concrete_value(simple_instr *in, int i) throw()
        : abstract_value(in)
    {
        value.as_int = i;
        this->type = INT;
        this->kind = VALUE;
    }

    concrete_value(simple_instr *in, unsigned u) throw()
        : abstract_value(in)
    {
        value.as_uint = u;
        this->type = UNSIGNED;
        this->kind = VALUE;
    }

    concrete_value(simple_instr *in, double f) throw()
        : abstract_value(in)
    {
        value.as_float = f;
        this->type = FLOAT;
        this->kind = VALUE;
    }

    virtual ~concrete_value(void) throw() { }

    virtual void union_symbols(std::set<simple_reg *> &) throw() { }
};

/// represents a symbolic value stored in a register
struct symbolic_value : public abstract_value {
public:

    simple_reg *reg;

    symbolic_value(simple_reg *r) throw()
        : abstract_value(0)
        , reg(r)
    {
        this->type = UNKNOWN;
        this->kind = SYMBOL;
    }

    virtual ~symbolic_value(void) throw() { }

    virtual void union_symbols(std::set<simple_reg *> &syms) throw() {
        syms.insert(reg);
    }
};

/// an exception thrown when something illegal is done, i.e. something that
/// cannot be interpreted abstractly.
struct stop_interpreter { };

/// check if an abstract value uses a particular register; this is how cycles
/// are conservatively detected.
static bool uses_register(const abstract_value *val, simple_reg *reg) throw() {

    if(abstract_value::EXPRESSION == val->kind) {
        const symbolic_expression *expr(unsafe_cast<const symbolic_expression *>(val));
        return !!expr->dependent_regs.count(reg);

    } else if(abstract_value::SYMBOL == val->kind) {
        const symbolic_value *sym(unsafe_cast<const symbolic_value *>(val));
        return sym->reg == reg;

    } else {
        return false;
    }
}

/// exception value that is thrown if interpretation should stop
const static stop_interpreter STOP_INTERPRETER;

/// perform a binary operation on two abstract values
template <template <typename L, typename R, typename O> class OpFunctor>
abstract_value *apply_binary(
    simple_instr *instr,        // instruction being executed
    simple_reg *dest_reg,       // what is the reg's old value?
    abstract_value *a0,         // left param of binary operator
    abstract_value *a1          // right param of binary operator
) throw(stop_interpreter) {

    // filter out any invalid operations and instantiate the operator templates
    typedef typename binary_op_functor_filter<OpFunctor, int>::functor int_functor;
    typedef typename binary_op_functor_filter<OpFunctor, unsigned>::functor unsigned_functor;
    typedef typename binary_op_functor_filter<OpFunctor, double>::functor double_functor;

    // both compile-time values; simple case
    if(abstract_value::VALUE == a0->kind && a0->kind == a1->kind) {

        concrete_value *p0(unsafe_cast<concrete_value *>(a0));
        concrete_value *p1(unsafe_cast<concrete_value *>(a1));

        switch(a0->type) {
        case abstract_value::INT:
            return new concrete_value(instr, int_functor()(p0->value.as_int, p0->value.as_int));

        case abstract_value::UNSIGNED:
            return new concrete_value(instr, unsigned_functor()(p0->value.as_uint, p0->value.as_uint));

        case abstract_value::FLOAT:
            return new concrete_value(instr, double_functor()(p0->value.as_float, p0->value.as_float));

        default:
            assert(false);
            break;
        }

    // need to compute an expression;
    } else {
        if(uses_register(a0, dest_reg) || uses_register(a1, dest_reg)) {
            throw STOP_INTERPRETER;
        }

        return new symbolic_expression(instr, a0, a1);
    }
}

/// perform a unary operation on two abstract values
template <template <typename L, typename O> class OpFunctor>
abstract_value *apply_unary(
    simple_instr *instr,        // instruction being executed
    simple_reg *dest_reg,       // what is the reg's old value?
    abstract_value *a0          // left param of binary operator
) throw(stop_interpreter) {

    // filter out any invalid operations and instantiate the operator templates
    typedef typename unary_op_functor_filter<OpFunctor, int>::functor int_functor;
    typedef typename unary_op_functor_filter<OpFunctor, unsigned>::functor unsigned_functor;
    typedef typename unary_op_functor_filter<OpFunctor, double>::functor double_functor;

    // both compile-time values; simple case
    if(abstract_value::VALUE == a0->kind) {

        concrete_value *p0(unsafe_cast<concrete_value *>(a0));

        switch(a0->type) {
        case abstract_value::INT:
            return new concrete_value(instr, int_functor()(p0->value.as_int));

        case abstract_value::UNSIGNED:
            return new concrete_value(instr, unsigned_functor()(p0->value.as_uint));

        case abstract_value::FLOAT:
            return new concrete_value(instr, double_functor()(p0->value.as_float));

        default:
            assert(false);
            break;
        }

    // need to compute an expression;
    } else {
        if(uses_register(a0, dest_reg)) {
            throw STOP_INTERPRETER;
        }

        return new symbolic_expression(instr, a0);
    }
}

/// maps label symbols to the instructions following the labels
typedef std::map<simple_sym *, simple_instr *> branch_map;

/// maps registers to their abstract values
class symbol_map : public std::map<simple_reg *, abstract_value *> {
public:

    abstract_value *&operator[](simple_reg *reg) throw() {
        assert(0 != reg);
        return this->std::map<simple_reg *, abstract_value *>::operator[](reg);
    }
};

/// the state of the abstract interpreter
struct interpreter_state {
    branch_map branch_targets;
    symbol_map registers;

    simple_instr *pc;

    bool did_return;
    abstract_value *return_val;
};


/// build up a table of all registers
static void assign_symbolic_value(
    simple_reg *reg,
    simple_reg **,
    simple_instr *,
    symbol_map &symbols
) throw() {
    if(0U == symbols.count(reg)) {
        assert(0 != reg);
        symbols[reg] = new symbolic_value(reg);
    }
}

/// try to set up the interpreter
static bool setup_interpreter(interpreter_state &s) throw() {
    for(simple_instr *in(s.pc); 0 != in; in = in->next) {

        // assign symbolic values to all registers
        for_each_var_use(assign_symbolic_value, in, s.registers);
        for_each_var_def(assign_symbolic_value, in, s.registers);

        switch(in->opcode) {
        case LABEL_OP:
            s.branch_targets[in->u.label.lab] = in->next;
            continue;

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

#define LOOKUP_ARGS \
    src1 = s.registers[src1_reg]; \
    src2 = s.registers[src2_reg]; \
    dst = &(s.registers[dst_reg]);

static bool assign(abstract_value **dst, abstract_value *src) throw() {
    dec_ref(*dst);
    *dst = src;
    return true;
}

/// attempt to interpret an instruction. interpretation will fail for one of a
/// few reasons:
///     1) a control branch depends on a symbolic value/expression
//      2) there is a cyclic dependency in with some symbolic value, e.g. a new
//         instance of a register depends on a previous instance, where the
//         previous instance was symbolic
//      3)
static bool interpret_instruction(interpreter_state &s) throw(stop_interpreter) {
    if(0 == s.pc) {
        return false;
    }

    simple_instr *in(s.pc);

    // common case: next instruction is the next pc
    s.pc = in->next;

    abstract_value *src1(0);
    abstract_value *src2(0);
    abstract_value **dst(0);

    // conveniences for the common case, unsafe for other cases, unless manually
    // assigned to!
    simple_reg *src1_reg(in->u.base.src1);
    simple_reg *src2_reg(in->u.base.src2);
    simple_reg *dst_reg(in->u.base.dst);

    concrete_value *val(0);

    switch(in->opcode) {
    case NOP_OP: return true;

    case RET_OP:
        s.did_return = true;
        if(0 != in->u.base.src1) {
            s.return_val = s.registers[in->u.base.src1];
        }
        return false;

    case STR_OP: assert(false); return false;
    case MCPY_OP: assert(false); return false;

    // copy from one register into another; making sure to update ref counts
    // accordingly
    case CPY_OP:
        if(src1_reg == dst_reg) {
            return true;

        // load src first, just in case src and *dst are the same
        } else {
            src1 = s.registers[src1_reg];
            inc_ref(src1);

            dst = &(s.registers[dst_reg]);
            dec_ref(*dst);

            *dst = src1;
        }
        return true;

    // convert a value of one type to another type
    case CVT_OP:

        src1 = s.registers[src1_reg];
        dst = &(s.registers[dst_reg]);

        switch(dst_reg->var->type->base) {
        case SIGNED_TYPE:
            return assign(dst, apply_unary<cast_to_int>(in, dst_reg, src1));

        case UNSIGNED_TYPE:
            return assign(dst, apply_unary<cast_to_unsigned>(in, dst_reg, src1));

        case FLOAT_TYPE:
            return assign(dst, apply_unary<cast_to_double>(in, dst_reg, src1));

        default: // abort
            assert(false);
            return false;
        }
        break;

    case NEG_OP: LOOKUP_ARGS
        return assign(dst, apply_unary<op::negate>(in, dst_reg, src1));

    case NOT_OP: LOOKUP_ARGS
        return assign(dst, apply_unary<op::bitwise_not>(in, dst_reg, src1));

    case LOAD_OP: assert(false);  return false;

    case ADD_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::add>(in, dst_reg, src1, src2));

    case SUB_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::subtract>(in, dst_reg, src1, src2));

    case MUL_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::multiply>(in, dst_reg, src1, src2));

    case DIV_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::divide>(in, dst_reg, src1, src2));

    case REM_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::modulo>(in, dst_reg, src1, src2));

    case MOD_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<suif_mod>(in, dst_reg, src1, src2));

    case AND_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::bitwise_and>(in, dst_reg, src1, src2));

    case IOR_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::bitwise_or>(in, dst_reg, src1, src2));

    case XOR_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::bitwise_xor>(in, dst_reg, src1, src2));

    case ASR_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<suif_asr>(in, dst_reg, src1, src2));

    case LSL_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<suif_lsl>(in, dst_reg, src1, src2));

    case LSR_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<suif_lsr>(in, dst_reg, src1, src2));

    case ROT_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<suif_rot>(in, dst_reg, src1, src2));

    case SEQ_OP: LOOKUP_ARGS
        if(src1 == src2) {
            return assign(dst, new concrete_value(in, (int) 1));
        }
        return assign(dst, apply_binary<op::equal>(in, dst_reg, src1, src2));

    case SNE_OP: LOOKUP_ARGS
        return assign(dst, apply_binary<op::not_equal>(in, dst_reg, src1, src2));

    case SL_OP: LOOKUP_ARGS
        if(src1 == src2) {
            return assign(dst, new concrete_value(in, (int) 0));
        }
        return assign(dst, apply_binary<op::less_than>(in, dst_reg, src1, src2));

    case SLE_OP: LOOKUP_ARGS
        if(src1 == src2) {
            return assign(dst, new concrete_value(in, (int) 1));
        }
        return assign(dst, apply_binary<op::less_than_equal>(in, dst_reg, src1, src2));

    case JMP_OP:
        s.pc = s.branch_targets[in->u.label.lab];
        return true;

    case BTRUE_OP:
        src1 = s.registers[in->u.bj.src];
        if(abstract_value::VALUE != src1->kind) {
            throw STOP_INTERPRETER;
        }

        val = unsafe_cast<concrete_value *>(src1);
        if(val->value.as_int) {
            s.pc = s.branch_targets[in->u.bj.target];
        }
        return true;

    case BFALSE_OP:
        src1 = s.registers[in->u.bj.src];
        if(abstract_value::VALUE != src1->kind) {
            throw STOP_INTERPRETER;
        }

        val = unsafe_cast<concrete_value *>(src1);
        if(!val->value.as_int) {
            s.pc = s.branch_targets[in->u.bj.target];
        }
        return true;

    // create a concrete value :D note: this does not use the normal assign
    // as abstract_values initialize with refcount=1
    case LDC_OP:
        dst = &(s.registers[in->u.ldc.dst]);
        dec_ref(*dst);

        switch(in->u.ldc.value.format) {
        case IMMED_INT:
            if(SIGNED_TYPE == in->u.ldc.dst->var->type->base) {
                *dst = new concrete_value(in, in->u.ldc.value.u.ival);
            } else {
                *dst = new concrete_value(in,
                    static_cast<unsigned>(in->u.ldc.value.u.ival)
                );
            }
            return true;
        case IMMED_FLOAT:
            *dst = new concrete_value(in, in->u.ldc.value.u.fval);
            return true;
        default:
            assert(false);
            return false;
        }

    case CALL_OP: assert(false);  return false;

    // multi-way branch
    case MBR_OP: {
        src1 = s.registers[in->u.mbr.src];
        if(abstract_value::VALUE != src1->kind) {
            throw STOP_INTERPRETER;
        } else {
            val = unsafe_cast<concrete_value *>(src1);

            int result(val->value.as_int);
            result -= in->u.mbr.offset;

            if(result < 0 || in->u.mbr.ntargets < result) {
                s.pc = s.branch_targets[in->u.mbr.deflab];
            } else {
                s.pc = s.branch_targets[in->u.mbr.targets[result]];
            }
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

/// attempt to perform an abstract interpretation of a function
void abstract_evaluator(optimizer &o) throw() {
#ifndef ECE540_DISABLE_EVAL

    interpreter_state s;
    s.pc = o.first_instruction();
    s.did_return = false;
    s.return_val = 0;

    if(setup_interpreter(s)) {
        try {
            while(interpret_instruction(s)) { /* loop a doop */ }
        } catch(stop_interpreter &) {
            goto cleanup;
        }
    }

    if(!s.did_return) {
        goto cleanup;
    }

    // okay, we can do code gen now!
    fprintf(stderr, "successfully abstractly interpreted function!\n");

    // time to clean things up
cleanup:

    // clear out the symbolic values
    s.branch_targets.clear();
    symbol_map::iterator reg_it(s.registers.begin())
                       , reg_end(s.registers.end());

    for(; reg_it != reg_end; ++reg_it) {
        dec_ref(reg_it->second);
        reg_it->second = 0;
    }

    s.registers.clear();

#endif
}

