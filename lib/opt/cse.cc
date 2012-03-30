/*
 * cse.cc
 *
 *  Created on: Mar 25, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <set>
#include <cassert>

extern "C" {
#   include <simple.h>
}

#include "include/opt/cse.h"
#include "include/cfg.h"
#include "include/diag.h"
#include "include/instr.h"
#include "include/operator.h"
#include "include/optimizer.h"
#include "include/set.h"
#include "include/data_flow/ae.h"
#include "include/data_flow/var_def.h"
#include "include/data_flow/var_use.h"

struct cse_state {
public:
    available_expression_map *ae_exit;
    available_expression_map *ae_enter;
    optimizer *o;
};


/// figure out all available expressions at the beginning of this basic block;
/// essentially the same as the meet function of ae.cc
static bool collect_aes_per_bb(
    basic_block *bb,
    cse_state &s
) throw() {
    std::set<available_expression_set> incoming_expression_sets;

    const std::set<basic_block *> &preds(bb->predecessors());
    std::set<basic_block *>::const_iterator it(preds.begin())
                                          , end(preds.end());

    // collect expressions exiting bb's predecessors
    for(; it != end; ++it) {
        incoming_expression_sets.insert((*s.ae_exit)(*it));
    }

    // update
    available_expression_set empty_set;
    (*s.ae_enter)(bb) = set_accumulate<
        available_expression_set,
        available_expression_set,
        unary_operator<available_expression_set>::copy,
        available_expression_set_intersection
    >(
        incoming_expression_sets,
        empty_set
    );
    return true;
}

struct cse_reg_finder {
public:
    simple_reg *reg;
    simple_reg **loc;
    simple_reg *repl;
};

/// find and store location information about a register defined in an instruction
static void find_dest_reg(simple_reg *reg, simple_reg **loc, simple_instr *, cse_reg_finder &f) throw() {
    f.reg = reg;
    f.loc = loc;
}

/// replace a usage of one register with another one
static void replace_reg_use(
    simple_reg *reg,
    simple_reg **loc,
    simple_instr *,
    cse_reg_finder &f
) throw() {
    if(reg == f.reg) {
        *loc = f.repl;
    }
}

/// replace all uses and the definition of a specific temporary register with a
/// new pseudo register
static void replace_temp_register(
    basic_block *bb,
    cse_reg_finder &f,
    available_expression_map &ae
) throw() {
    simple_reg *repl(new_register(f.reg->var->type, PSEUDO_REG));
    f.repl = repl;

    // replace at all usage points
    for(simple_instr *in(bb->first), *end(bb->last->next);
        in != end;
        in = in->next) {
        available_expression old_expr(ae(in));
        for_each_var_use(&replace_reg_use, in, f);
        ae.unsafe_inject_expression(in, old_expr);
    }

    // replace at the def point
    *(f.loc) = repl;
    f.reg = repl;
    f.repl = 0;
}

/// find the common sub-expressions and eliminate them; again, very similar to
/// the transfer function of available expressions
static bool replace_common_sub_expressions(
    basic_block *bb,
    cse_state &s
) throw() {
    if(0 == bb->last) {
        return true;
    }
    available_expression_set ae((*s.ae_enter)(bb));

    for(simple_instr *in(bb->first), *next(0), *end(bb->last->next);
        in != end;
        in = next) {

        next = in->next; // just in case

        if(instr::is_expression(in)) {

            cse_reg_finder d_f; // defining reg for instruction
            cse_reg_finder ae_f; // defining reg for available expression instruction
            for_each_var_def(&find_dest_reg, in, d_f);

            // get the expression for this instruction, and force it to be a lower
            // bound w.r.t the available expression sets
            available_expression expr((*s.ae_exit)(in));
            expr.in = 0;
            expr.bb = bb;

            available_expression_set::iterator it(ae.upper_bound(expr))
                                             , it_end(ae.end());

            // found a common sub-expression; eliminate it
            if(it != it_end && it->id == expr.id) {

                simple_reg *temp(new_register(d_f.reg->var->type, PSEUDO_REG));

                // add in a copy to the temp register after each def of the
                // expression
                for(; it != it_end && it->id == expr.id; ++it) {
                    simple_instr *copy(new_instr(CPY_OP, d_f.reg->var->type));
                    for_each_var_def(&find_dest_reg, it->in, ae_f);

                    assert(it->in->opcode == in->opcode);

                    // make sure not to over-use a temporary register
                    if(TEMP_REG == ae_f.reg->kind) {
                        replace_temp_register(it->bb, ae_f, *(s.ae_exit));
                    }

                    // we've found at least one common sub-expression :D but it's
                    // a temporary register D: this is annoying because it will
                    // prevent copy propagation from being effective. Luckily,
                    // temporary registers are local to their basic blocks, so we
                    // can easily replace it
                    if(TEMP_REG == d_f.reg->kind) {
                        replace_temp_register(bb, d_f, *(s.ae_exit));
                    }

                    copy->u.base.dst = temp;
                    copy->u.base.src1 = ae_f.reg;
                    instr::insert_after(copy, it->in);
                    s.o->changed_def();
                }

                //fprintf(stderr, "  copying expression %u into %s\n", expr.id, r(d_f.reg));

                // replace the op with a copy
                s.o->changed_use();
                in->opcode = CPY_OP;
                in->u.base.dst = d_f.reg;
                in->u.base.src1 = temp;

            // didn't find a common sub-expression
            } else {
                expr.in = in;
                ae.insert(expr);
            }
        }

        // kill some expressions
        simple_reg *reg(0);
        if(for_each_var_def(in, reg)) {
            ae.erase(reg);
        }
    }

    // we might have changed what the available expressions are by fiddling
    // with things; make sure to update the available expressions at exit
    (*s.ae_exit)(bb) = ae;

    return true;
}

/// eliminate all common sub expressions
void eliminate_common_sub_expressions(
    optimizer &o,
    cfg &flow,
    available_expression_map &ae_exit
) throw() {
#ifndef ECE540_DISABLE_CSE

    // mapping of all available expressions; this is actually an incomplete
    // map insofar as it is unsuitable for expression lookup, as it hasn't been
    // built in the normal way
    available_expression_map ae_enter;
    cse_state state;
    state.ae_enter = &ae_enter;
    state.ae_exit = &ae_exit;
    state.o = &o;

    // get the available expressions entering into each basic block
    flow.for_each_basic_block(&collect_aes_per_bb, state);

    // update the graph
    flow.for_each_basic_block(&replace_common_sub_expressions, state);
#endif
}


