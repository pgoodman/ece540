/*
 * cp.cc
 *
 *  Created on: Mar 18, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cassert>

#include "include/opt/cp.h"
#include "include/diag.h"

#if 0
/// represents a set of non-temporary variable definitions that are copies
/// that reach the beginning of each basic block, where there is only one
/// definition of each register.
class reduced_var_def_set : var_def_set {
public:
    reduced_var_def_set(void) throw()
        : var_def_set()
    { }

    /// build a reduced var def set from a var def set
    reduced_var_def_set(const var_def_set &that) throw()
        : var_def_set()
    {
        var_def_set::iterator it(that.begin()), it_end(that.end()), forward_it;
        simple_reg *reg(0);
        for(; it != it_end; ) {

            reg = it->reg;
            var_def def(*it);
            bool skip(false);

            // make sure there is o
            for(++it; it != it_end; ++it) {
                skip = true;
                if(it->reg != def.reg) {
                    break;
                }
            }

            // not a copy, or there are multiple defs
            if(skip || CPY_OP != def.in->opcode) {
                continue;
            }

            // by this point: it's a copy op, there's only one of them
            this->insert(def);
        }
    }

    /// join two reduced var def sets; this doesn't necessarily result in a
    /// larger set.
    void join(const var_def_set &that) throw() {

    }

};
#endif

struct cp_state {
public:
    use_def_map *ud;
    optimizer *o;
    const var_def_set *rd;
};

/// propagate copies at the usage level
static void try_propagate_copy(
    simple_reg *reg,
    simple_reg **use_of_reg,
    simple_instr *in,
    cp_state &s
) throw() {
    if(0 == reg || PSEUDO_REG != reg->kind) {
        return;
    }

    var_def_set::const_iterator def(s.rd->find(reg))
                              , end(s.rd->end());

    // no defs reach this use; likely a parameter to a function call
    if(def == end) {
        return;
    }

    simple_reg *copied_reg(0);
    for(; def != end && def->reg == reg; ++def) {

        // at least one non-copy def reaches this
        if(CPY_OP != def->in->opcode) {
            return;
        }

        // multiple copies reach this use, and different registers are copied
        if(0 != copied_reg && def->in->u.base.src1 != copied_reg) {
            return;
        }

        copied_reg = def->in->u.base.src1;
    }

    assert(0 != copied_reg);

    if(PSEUDO_REG != copied_reg->kind) {
        return;
    }

    s.o->changed_use();
    *use_of_reg = copied_reg;
}

/// propagate copies at the basic block level
static bool propagate_in_bb(basic_block *bb, cp_state &s) {
    if(0 == bb->last) {
        return true;
    }
    const simple_instr *past_end(bb->last->next);
    for(simple_instr *in(bb->first); past_end != in; in = in->next) {
        s.rd = &((*(s.ud))(in));
        for_each_var_use(&try_propagate_copy, in, s);
        s.rd = 0;
    }

    return true;
}

void propagate_copies(optimizer &o, cfg &flow, use_def_map &ud) throw() {
    //fprintf(stderr, "// cp\n");
#ifndef ECE540_DISABLE_CP
    cp_state state;
    state.o = &o;
    state.ud = &ud;
    flow.for_each_basic_block(&propagate_in_bb, state);
#endif
}

