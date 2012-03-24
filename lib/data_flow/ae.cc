/*
 * ae.cc
 *
 *  Created on: Mar 5, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cassert>

#include "include/data_flow/problem.h"
#include "include/data_flow/var_def.h"
#include "include/data_flow/var_use.h"
#include "include/data_flow/ae.h"

#include "include/cfg.h"
#include "include/basic_block.h"
#include "include/instr.h"
#include "include/set.h"
#include "include/operator.h"
#include "include/unsafe_cast.h"

/// return true if a register contains a variable of floating point type
static bool is_float_type(const simple_reg *reg) throw() {
    return FLOAT_TYPE == reg->var->type->base;
}

namespace detail {

    /// initialize an available expression; this might re-order arguments (depending
    /// on operator type and operand types)
    available_expression_impl::available_expression_impl(const simple_instr *in) throw()
        : op_code(in->opcode)
    {

        // look at associative operators for re-ordering opportunities
        switch(op_code) {

        // commutative, float-aware
        case ADD_OP: case MUL_OP:
            if(is_float_type(in->u.base.src1) || is_float_type(in->u.base.src2)) {
                op_left = in->u.base.src1;
                op_right = in->u.base.src2;
                break;
            }

            // fall-through to assign and potentially re-order

        // commutative, float-ignorant
        case AND_OP: case IOR_OP: case XOR_OP:
            op_left = in->u.base.src1;
            op_right = in->u.base.src2;

            if(op_right < op_left) {
                op_left = in->u.base.src2;
                op_right = in->u.base.src1;
            }

            break;

        // unary expressions
        case NEG_OP: case NOT_OP:
            op_left = in->u.base.src1;
            op_right = 0;
            break;

        // call; considered a special case where two calls can never be
        // considered equivalent; this is an ugly hack, the assignment
        // reference should not have used calls as that implies one should
        // be able to check the purity of C functions
        case CALL_OP:
            op_left = unsafe_cast<const simple_reg *>(in);
            op_right = 0;
            break;

        // type conversion, considered a special case, the type of the
        // destination register is recorded as a formal parameter
        case CVT_OP:
            op_left = unsafe_cast<const simple_reg *>(in->u.base.dst->var->type);
            op_right = in->u.base.src1;
            break;

        // non-reorderable
        default:
            op_left = in->u.base.src1;
            op_right = in->u.base.src2;
            break;
        }
    }

    /// define a strict weak ordering of available expressions in terms of a
    /// lexicographic order of the operator, the addresses of the operands, and the
    /// basic block id.
    ///
    /// the basic block id is considered as the least significant so that equivalent
    /// expressions are ordered beside eachother
    bool available_expression_impl::operator<(const available_expression_impl &expr) const throw() {
        if(op_code < expr.op_code) {
            return true;
        } else if(op_code > expr.op_code) {
            return false;
        }

        if(op_left < expr.op_left) {
            return true;
        } else if(op_left > expr.op_left) {
            return false;
        }

        if(op_right < expr.op_right) {
            return true;
        } else if(op_right > expr.op_right) {
            return false;
        }

        return false; // equivalent
    }

    /// structural equivalence of expressions
    bool available_expression_impl::operator==(const available_expression_impl &expr) const throw() {
        return op_code == op_code
            && op_left == expr.op_left
            && op_right == expr.op_right;
    }
}

/// initialize an available expression
available_expression::available_expression(unsigned id_, simple_instr *in_) throw()
    : id(id_)
    , in(in_)
{ }

/// strict weak ordering for an available expression based on its id
bool available_expression::operator<(const available_expression &that) const throw() {
    return id < that.id;
}

/// equivalence of two available expressions
bool available_expression::operator==(const available_expression &that) const throw() {
    return id == that.id;
}

/// erase any expression in this set that uses a particular value
struct find_reg {
    const simple_reg *to_find;
    bool found;
};
static void find_register(simple_reg *reg, simple_reg **, simple_instr *, find_reg &finder) throw() {
    if(reg == finder.to_find) {
        finder.found = true;
    }
}
void available_expression_set::erase(const simple_reg *reg) throw() {
    assert(0 != reg);

    if(PSEUDO_REG != reg->kind) {
        return;
    }

    iterator it(this->begin()), prev;
    const iterator end(this->end());
    find_reg finder;
    finder.to_find = reg;

    for(; it != end; ) {
        finder.found = false;
        for_each_var_use(find_register, it->in, finder);

        // the register is used in this expression, kill it
        if(finder.found) {
            prev = it;
            ++it;
            this->std::set<available_expression>::erase(prev);
        } else {
            ++it;
        }
    }
}

/// go add in an expression to the expression map if it is not already in the
/// expression map
bool available_expression_map::find_expression(simple_instr *in, available_expression_map &self) throw() {
    if(instr::is_expression(in)) {
        detail::available_expression_impl expr(in);

        if(0U == self.expression_ids.count(expr)) {

            unsigned &id(self.expression_ids[expr]);
            id = (self.next_expression_id)++;

            available_expression ae(
                id,
                in
            );

            self.expressions.push_back(ae);
        }
    }

    return true;
}

/// go find all expressions in a basic block
bool available_expression_map::find_expression(basic_block *bb, available_expression_map &self) throw() {
    bb->for_each_instruction(find_expression, self);
    return true;
}

/// map all expressions to ids
available_expression_map::available_expression_map(void) throw()
    : next_expression_id(0U)
{ }

/// apply a function to a copy of each unique expression in the available
/// expression map
bool available_expression_map::for_each_expression(bool (*callback)(available_expression)) throw() {
    for(unsigned i(0); i < next_expression_id; ++i) {
        if(!callback(expressions[i])) {
            return false;
        }
    }
    return true;
}

/// look up an expression using its instruction
available_expression available_expression_map::operator()(const simple_instr *in) throw() {
    detail::available_expression_impl expr_impl(in);
    return expressions[expression_ids[expr_impl]];
}

/// get a set of available expressions by the basic block
available_expression_set &
available_expression_map::operator()(const basic_block *bb) throw() {
    assert(0 != bb);
    return expression_sets[bb];
}

/// clear out all sets of available expressions for each basic block
void available_expression_map::clear(void) throw() {
    expression_sets.clear();
    expressions.clear();
    expression_ids.clear();
    next_expression_id = 0;
}

namespace {

    /// compute the set of evaluated expressions for a basic block; this
    /// applies the kill set incrementally
    static bool compute_evaluated_expressions(
        IN      basic_block *bb,
        IN      available_expression_map &all_expressions,
        INOUT   available_expression_set &bb_expressions
    ) throw() {
        if(0 == bb->first) {
            return true;
        }
        for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {

            // kill stuff that comes before
            simple_reg *reg(0);
            if(for_each_var_def(in, reg)) {
                bb_expressions.erase(reg);
            }

            // add something new in
            if(instr::is_expression(in)) {
                available_expression expr(all_expressions(in));
                bb_expressions.insert(expr);
            }
        }
        return true;
    }

    /// initialize the problem with the local eval set of each basic block
    class init_function {
    public:

        /// initialize the basic blocks
        static bool init_bb_eval_set(
            IN      basic_block *bb,
            INOUT   available_expression_map &expressions
        ) throw() {
            return compute_evaluated_expressions(bb, expressions, expressions(bb));
        }

        /// make sure unreachable basic blocks are given all expressions as
        /// available
        static bool init_unreachable_bb_eval_set(
            IN      basic_block *bb,
            IN      available_expression_set &all_expressions,
            INOUT   available_expression_map &expressions
        ) throw() {
            if(!bb->entry_reachable && bb->predecessors().empty()) {
                expressions(bb) = all_expressions;
            }

            return true;
        }

        void operator()(
            IN      cfg &flow_graph,
            INOUT   available_expression_map &expressions
        ) throw() {
            flow_graph.for_each_basic_block(&init_bb_eval_set, expressions);
        }
    };

    /// compute the intersection of several sets
    class meet_function {
    public:

        /// only care about intersecting when the reachability of the incoming
        /// block agrees with the reachability of the block for which we are
        /// trying to compute the available expressions
        bool operator()(
            IN      basic_block *source,
            IN      basic_block *incoming
        ) throw() {
            return source->entry_reachable == incoming->entry_reachable;
        }

        /// compute the intersection of all incoming expression sets
        void operator()(
            IN      std::set<available_expression_set> &incoming_expression_sets,
            INOUT   available_expression_set &outgoing_expressions
        ) throw() {

            available_expression_set empty_set;
            outgoing_expressions = set_accumulate<
                available_expression_set,
                available_expression_set,
                unary_operator<available_expression_set>::copy,
                set_intersection
            >(
                incoming_expression_sets,
                empty_set
            );
        }
    };

    /// find the expressions thar make it to the end of this basic block
    class transfer_function {
    private:

        available_expression_map &expressions;
        available_expression_set all_expressions;

    public:

        transfer_function(transfer_function &that) throw()
            : expressions(that.expressions)
            , all_expressions(that.all_expressions)
        { }

        transfer_function(available_expression_map &expressions_) throw()
            : expressions(expressions_)
        {
            expressions.for_each_expression(&add_expression_to_set, all_expressions);
        }

        /// used to collect the set of all available expressions
        static bool add_expression_to_set(
            IN      available_expression expr,
            INOUT   available_expression_set &all_expressions_
        ) throw() {
            all_expressions_.insert(expr);
            return true;
        }

        /// re-evaluate the evaluation expressions, given the incoming one.
        /// if empty set is given, this would have computed the local available
        /// expressions
        void operator()(
            IN      basic_block *bb,
            IN      available_expression_set &incoming_exprs, // from predecessors
            INOUT   available_expression_set &outgoing_exprs // to successors
        ) throw() {

            // if the basic block is not reachable, then assume that all
            // expressions are available at the entry to the basic block
            if(!bb->entry_reachable) {
                outgoing_exprs = all_expressions;

            // reachable, take the incoming expressions from predecessors
            } else {
                outgoing_exprs = incoming_exprs;
            }
            compute_evaluated_expressions(bb, expressions, outgoing_exprs);
        }
    };
}

/// compute all available expressions
void find_available_expressions(cfg &flow, available_expression_map &ae) throw() {

    transfer_function transfer(ae);

    data_flow_problem<
        forward_data_flow,
        available_expression_set, // domain
        meet_function,
        transfer_function,
        init_function,
        available_expression_map
    > compute_available_expressions(transfer);

    ae.clear();
    flow.for_each_basic_block(&available_expression_map::find_expression, ae);

    compute_available_expressions(flow, ae);
}

