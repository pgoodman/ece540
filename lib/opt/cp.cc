/*
 * cp.cc
 *
 *  Created on: Mar 18, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cassert>
#include <cstdlib>

#include "include/opt/cp.h"
#include "include/cfg.h"
#include "include/optimizer.h"
#include "include/use_def.h"
#include "include/diag.h"

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
    if(0 == reg) {
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
    if(0 != getenv("ECE540_DISABLE_CP")) {
        return;
    }

    cp_state state;
    state.o = &o;
    state.ud = &ud;
    flow.for_each_basic_block(&propagate_in_bb, state);
}

