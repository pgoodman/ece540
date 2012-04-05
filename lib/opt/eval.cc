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
#include <cstdlib>

#include "include/optimizer.h"
#include "include/operator.h"
#include "include/instr.h"
#include "include/unsafe_cast.h"

#include "include/data_flow/var_def.h"
#include "include/data_flow/var_use.h"

#include "include/opt/eval.h"

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
    int depth;
    simple_instr *instr;
    simple_reg *emitted_reg;

    enum {
        INT, UNSIGNED, FLOAT, UNKNOWN
    } type;

    enum {
        VALUE       = 1 << 0,
        SYMBOL      = 1 << 1,
        EXPRESSION  = 1 << 2
    } kind;

    enum {
        KIND = VALUE | SYMBOL | EXPRESSION
    };

    abstract_value(simple_instr *in) throw()
        : ref_count(1)
        , instr(in)
        , emitted_reg(0)
        , depth(1)
    { }

    virtual void union_symbols(std::set<simple_reg *> &) throw() = 0;
};

/// reference counting
static abstract_value *inc_ref(abstract_value *val) throw() {
    ++(val->ref_count);
    return val;
}

static void dec_ref(abstract_value *val) throw() {
    if(0 == --(val->ref_count)) {
        delete val;
    }
}

static void unsafe_dec_ref(abstract_value *val) throw() {
    --(val->ref_count);
}

static int max_depth(int a, int b) throw() {
    return a < b ? b : a;
}

/// represents an abstract expression that depends on some symbolic values. this
/// is guaranteed to be a DAG by virtue of dependent_regs, which is the union
/// of all symbolic registers in all sub-expressions. If at any point one
/// assigns to symbolic register a value that depends on that register, the
/// abstract interpreter will bail out, thus maintaining the correct invariant.
struct symbolic_expression : public abstract_value {
public:

    enum {
        KIND = EXPRESSION
    };

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
        this->depth = max_depth(a0->depth, a1->depth) + 1;
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
        this->depth = a0->depth + 1;
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

    enum {
        KIND = VALUE
    };

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

    enum {
        KIND = SYMBOL
    };

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
struct stop_interpreter {
public:
    stop_interpreter(int) throw() { }
};

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

/// pattern match over a binary expression
template <typename L, typename R>
bool match_binary(abstract_value *val, L *&a0, R *&a1) throw() {
    if(abstract_value::EXPRESSION != val->kind) {
        return false;
    }

    symbolic_expression *expr(unsafe_cast<symbolic_expression *>(val));

    if(symbolic_expression::UNARY == expr->arity) {
        return false;
    }

    abstract_value *left(expr->left)
                 , *right(expr->right);

    if((L::KIND & left->kind) && (R::KIND & right->kind)) {
        a0 = unsafe_cast<L *>(left);
        a1 = unsafe_cast<R *>(right);
    } else if((L::KIND & right->kind) && (R::KIND & left->kind)) {
        a0 = unsafe_cast<L *>(right);
        a1 = unsafe_cast<R *>(left);
    } else {
        return false;
    }

    return true;
}

/// attempt to combine expressions of the form C1 + (C2 + ?) into (C1 + C2) + ?
static abstract_value *combine_constant_adds(
    simple_instr *instr,
    abstract_value *a0,
    abstract_value *a1
) throw() {
    if(ADD_OP != instr->opcode) {
        return 0;
    }

    concrete_value *val(0);
    abstract_value *expr(0);

    // we're pattern matching for V op (V op ?)
    if(abstract_value::VALUE == a0->kind) {
        val = unsafe_cast<concrete_value *>(a0);
        expr = a1;
    } else if(abstract_value::VALUE == a1->kind) {
        val = unsafe_cast<concrete_value *>(a1);
        expr = a0;
    } else {
        return 0;
    }

    concrete_value *sub_val(0);
    abstract_value *sub_expr(0);

    if(match_binary(expr, sub_val, sub_expr)) {

        // okay, lets flatten this
        concrete_value *new_sub_val(new concrete_value(
            sub_val->instr,
            val->value.as_int + sub_val->value.as_int
        ));

        unsafe_dec_ref(new_sub_val); // make sure ref count on the sub-value is 1, not 2

        return new symbolic_expression(instr, new_sub_val, sub_expr);
    }

    return 0;
}

/// exception value that is thrown if interpretation should stop
const static stop_interpreter STOP_INTERPRETER(0);

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
            return new concrete_value(instr, int_functor()(p0->value.as_int, p1->value.as_int));

        case abstract_value::UNSIGNED:
            return new concrete_value(instr, unsigned_functor()(p0->value.as_uint, p1->value.as_uint));

        case abstract_value::FLOAT:
            return new concrete_value(instr, double_functor()(p0->value.as_float, p1->value.as_float));

        default:
            assert(false);
            break;
        }

    // need to compute an expression;
    } else {
        if(uses_register(a0, dest_reg) || uses_register(a1, dest_reg)) {
            throw STOP_INTERPRETER;
        }

        abstract_value *ret(combine_constant_adds(instr, a0, a1));

        if(0 == ret) {
            ret = new symbolic_expression(instr, a0, a1);
        }

        return ret;
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

    simple_instr *pc;       // current program counter
    simple_instr *ret;      // instruction used to return a value
    simple_instr *error;    // instruction where an error occurred
    simple_instr *bp;       // breakpoint, i.e. instruction before which to stop interpreting

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
    bool ret(true);
    for(simple_instr *in(s.pc); 0 != in; in = in->next) {

        // assign symbolic values to all registers
        for_each_var_use(assign_symbolic_value, in, s.registers);
        for_each_var_def(assign_symbolic_value, in, s.registers);

        switch(in->opcode) {
        case LABEL_OP:
            s.branch_targets[in->u.label.lab] = in->next;
            continue;

        case CALL_OP: case LOAD_OP: case STR_OP: case MCPY_OP:
            ret = false;
            continue;

        case LDC_OP:
            if(IMMED_SYMBOL == in->u.ldc.value.format) {
                ret = false;
            }
            continue;
        default:
            continue;
        }
    }
    return ret;
}

#define LOOKUP_ARGS \
    src1 = s.registers[src1_reg]; \
    src2 = s.registers[src2_reg]; \
    dst = &(s.registers[dst_reg]);

/// this is to limit crazy amounts of loop unrolling
enum {
    MAX_DEPTH = 300
};

static bool assign(abstract_value **dst, abstract_value *src) throw(stop_interpreter) {
    dec_ref(*dst);
    *dst = src;

    if(MAX_DEPTH < src->depth) {
        throw STOP_INTERPRETER;
    }

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

    // stop the interpreter if we hit a breakpoint
    if(in == s.bp) {
        s.error = 0;
        throw STOP_INTERPRETER;
    }

    // common case: next instruction is the next pc
    s.error = in;
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
        s.ret = in;
        s.did_return = true;
        s.error = 0;
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
            src1 = inc_ref(s.registers[src1_reg]);

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
        if(1 == val->value.as_int) {
            s.pc = s.branch_targets[in->u.bj.target];
        }
        return true;

    case BFALSE_OP:
        src1 = s.registers[in->u.bj.src];
        if(abstract_value::VALUE != src1->kind) {
            throw STOP_INTERPRETER;
        }

        val = unsafe_cast<concrete_value *>(src1);
        if(1 != val->value.as_int) {
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
        break;

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

static simple_reg *emit_dispatch(simple_instr **prev, abstract_value *val) throw();
static simple_reg *emit(simple_instr **prev, concrete_value *val) throw();
static simple_reg *emit(simple_instr **prev, symbolic_value *val) throw();
static simple_reg *emit(simple_instr **prev, symbolic_expression *val) throw();

static simple_reg *emit_dispatch(simple_instr **prev, abstract_value *val) throw() {
    if(abstract_value::VALUE == val->kind) {
        return emit(prev, unsafe_cast<concrete_value *>(val));
    } else if(abstract_value::SYMBOL == val->kind) {
        return emit(prev, unsafe_cast<symbolic_value *>(val));
    } else {
        return emit(prev, unsafe_cast<symbolic_expression *>(val));
    }
}

/// emit the instructions for loading a constant
static simple_reg *emit(simple_instr **prev, concrete_value *val) throw() {

    simple_instr *orig_ldc(val->instr);
    simple_reg *orig_dest(val->instr->u.ldc.dst);

    // it will be used multiple times, and codegen has already happened
    if(0 != val->emitted_reg) {
        return val->emitted_reg;
    }

    // load constant. note: orig_ldc isn't actually guranteed to be a LDC, but
    // should have approximately the correct type
    simple_instr *in(new_instr(LDC_OP, orig_ldc->type));
    val->emitted_reg = new_register(orig_dest->var->type, TEMP_REG);

    in->next = in->prev = 0;
    in->u.ldc.dst = val->emitted_reg;

    // put the right value in
    switch(val->type) {
    case abstract_value::INT:
        in->u.ldc.value.format = IMMED_INT;
        in->u.ldc.value.u.ival = val->value.as_int;
        break;
    case abstract_value::UNSIGNED:
        in->u.ldc.value.format = IMMED_INT;
        in->u.ldc.value.u.ival = val->value.as_uint;
        break;
    case abstract_value::FLOAT:
        in->u.ldc.value.format = IMMED_FLOAT;
        in->u.ldc.value.u.fval = val->value.as_float;
        break;
    default: assert(false); break;
    }

    instr::insert_after(in, *prev);
    *prev = in;

    // copy iff this constant will be used more than once
    if(1 < val->ref_count) {
        in = new_instr(CPY_OP, orig_ldc->type);
        in->u.base.dst = new_register(orig_dest->var->type, PSEUDO_REG);
        in->u.base.src1 = val->emitted_reg;
        val->emitted_reg = in->u.base.dst;
        instr::insert_after(in, *prev);

        assert((*prev)->next == in);

        *prev = in;
    }

    return val->emitted_reg;
}

/// emit the register being used
static simple_reg *emit(simple_instr **, symbolic_value *val) throw() {
    return val->reg;
}

struct register_injector {
    unsigned accessor;
    simple_reg *regs[3];
};

/// inject registers into an instruction
void inject_register(
    simple_reg *,
    simple_reg **loc,
    simple_instr *,
    register_injector &inj
) throw() {
    *loc = inj.regs[inj.accessor++];
}

/// emit instructions for evaluating unary and binary expressions
static simple_reg *emit(simple_instr **prev, symbolic_expression *val) throw() {

    if(0 != val->emitted_reg) {
        return val->emitted_reg;
    }

    unsigned i(0);

    register_injector inj;
    inj.accessor = 0U;
    inj.regs[i++] = emit_dispatch(prev, val->left);

    if(symbolic_expression::BINARY == val->arity) {
        inj.regs[i++] = emit_dispatch(prev, val->right);
    }

    // both operands have been evaluated; now make the instruction to perform
    // the computation
    simple_instr *orig_in(val->instr);
    simple_instr *in(new_instr(orig_in->opcode, orig_in->type));

    assert(CPY_OP != orig_in->opcode);
    assert(NOP_OP != orig_in->opcode);

    memcpy(in, orig_in, sizeof *in);
    in->next = in->prev = 0;

    simple_reg *orig_reg(0);
    simple_reg *out(0);

    // create the register being assigned to
    if(for_each_var_def(orig_in, orig_reg)) {
        inj.regs[i++] = out = new_register(orig_reg->var->type, PSEUDO_REG);
    } else {
        assert(false);
    }

    // inject the registers
    for_each_var_use(inject_register, in, inj);
    for_each_var_def(inject_register, in, inj);

    // chain the instruction in
    instr::insert_after(in, *prev);
    *prev = in;

    return out;
}

/// cleanup the registers in memory
static void cleanup_state(interpreter_state &s) throw() {
    s.branch_targets.clear();
    symbol_map::iterator reg_it(s.registers.begin())
                       , reg_end(s.registers.end());

    for(; reg_it != reg_end; ++reg_it) {
        dec_ref(reg_it->second);
        reg_it->second = 0;
    }

    s.registers.clear();
}

/// try to interpret some "straight line" of code, up until the first branch
/// that goes more than one way. Return the next instruction if it is known
///
/// because this isn't as concerned about values, we can be less strict about
/// how we're interpreting
eval::breakpoint_status abstract_evaluator_bp(
    optimizer &o,
    simple_instr *first_instr,
    simple_instr *break_point
) throw() {
    interpreter_state s;
    s.pc = first_instr;
    s.did_return = false;
    s.return_val = 0;
    s.error = 0;
    s.bp = break_point;
    eval::breakpoint_status status(eval::UNKNOWN);

    setup_interpreter(s);

    for(;;) {
        try {
            while(interpret_instruction(s)) { /* loop a doop */ }
            status = eval::RETURNED;
        } catch(stop_interpreter &) {

            // we've hit a breakpoint or a return
            if(0 == s.error) {
                break;
            }

            // we've hit a different type of error

            simple_reg *defd_var(0);
            switch(s.error->opcode) {

            /// we've hit a branch that we can't walk through
            case BTRUE_OP: case BFALSE_OP: case MBR_OP:
                break;

            /// we've hit a CALL/LOAD op; force a value to be unknown if it sets to
            /// a register
            case CALL_OP: case LOAD_OP:
                if(for_each_var_def(s.error, defd_var)) {
                    dec_ref(s.registers[defd_var]);
                    s.registers[defd_var] = new symbolic_value(defd_var);
                }
                continue;

            /// we've hit a store op; continue
            case STR_OP: continue;

            /// not sure what happened; let's just give up
            default: break;
            }
        }
    }

    cleanup_state(s);

    // we've reached a breakpoint
    if(s.pc == s.bp) {
        return eval::REACHED_BREAKPOINT;
    }

    return status;
}

/// attempt to perform an abstract interpretation of a function
void abstract_evaluator(optimizer &o) throw() {
    if(0 != getenv("ECE540_DISABLE_EVAL")) {
        return;
    }

    simple_instr *first_instr(o.first_instruction());
    simple_instr *ret_instr(0);
    simple_instr dummy_first;
    simple_instr *last(&dummy_first);

    interpreter_state s;
    s.pc = first_instr;
    s.did_return = false;
    s.return_val = 0;
    s.error = 0;
    s.bp = 0;

    if(setup_interpreter(s)) {
        try {
            while(interpret_instruction(s)) { /* loop a doop */ }
        } catch(stop_interpreter &) {
            cleanup_state(s);
            return;
        }
    }

    if(!s.did_return) {
        cleanup_state(s);
        return;
    }

    // notify the optimizer that we've done some substantial things
    o.changed_block();
    o.changed_def();
    o.changed_use();

    // create the return instruction
    ret_instr = new_instr(RET_OP, s.ret->type);
    ret_instr->prev = 0;
    ret_instr->next = 0;

    // a return with no val; clear out the first instruction, add the thing in
    if(0 == s.return_val) {
        first_instr->opcode = NOP_OP;
        instr::insert_after(ret_instr, first_instr);
        cleanup_state(s);
        return;
    }

    // put all constants on an even footing in terms of the refcount telling
    // use how many times they are used
    inc_ref(s.return_val);
    cleanup_state(s);

    // okay, we can do code gen now!
    memset(last, 0, sizeof *last);
    ret_instr->u.base.src1 = emit_dispatch(&last, s.return_val);
    dec_ref(s.return_val);

    // clear out the first instruction
    first_instr->opcode = NOP_OP;
    first_instr->next = first_instr->prev = 0;

    if(&dummy_first == last) {
        instr::insert_after(ret_instr, first_instr);
    } else {

        first_instr->next = dummy_first.next->next;
        instr::insert_after(dummy_first.next, first_instr);
        instr::insert_after(ret_instr, last);
    }
}

