/*
 * use_def.cc
 *
 *  Created on: Mar 24, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include "include/use_def.h"
#include "include/cfg.h"
#include "include/basic_block.h"

struct ud_state {
public:
    std::map<simple_instr *, var_def_set> *ud;
    var_def_map *defs;
    basic_block *bb;
    var_def_set *reaching_defs;
    var_def_set *in_def_set;
};

/// go collect each definition that could possibly be used by this variable
/// use.
static void add_defs_for_use(
    simple_reg *reg_being_used,
    simple_reg **,
    simple_instr *in,
    ud_state &s
) throw() {

    var_def_set::iterator curr(s.reaching_defs->find(reg_being_used))
                        , end(s.reaching_defs->end());

    for(; curr != end && curr->reg == reg_being_used; ++curr) {
        s.in_def_set->insert(*curr);
    }
}

/// find all definitions that are used by each instruction
static bool find_defs_reaching_instr(simple_instr *in, ud_state &s) throw() {
    var_def_set &ds((*(s.ud))[in]);
    s.in_def_set = &ds;

    // add the uses of a register in
    for_each_var_use(&add_defs_for_use, in, s);

    // kill a defined register; a bit of a re-implementation of reaching
    // definitions
    simple_reg *defd_reg(0);
    if(for_each_var_def(in, defd_reg)) {
        s.reaching_defs->erase(defd_reg);

        var_def def;
        def.in = in;
        def.bb = s.bb;
        def.reg = defd_reg;
        s.reaching_defs->insert(def);
    }

    s.in_def_set = 0;
    return true;
}

/// find all defintions reaching each instruction in the basic block
static bool find_defs_in_bb(basic_block *bb, ud_state &s) throw() {
    var_def_set reaching_defs;
    s.bb = bb;
    s.reaching_defs = &reaching_defs;

    // iterate over predecessors and build up a set of reaching definitions
    const std::set<basic_block *> &preds(bb->predecessors());
    std::set<basic_block *>::const_iterator it(preds.begin()), it_end(preds.end());
    for(; it != it_end; ++it) {
        const var_def_set &pred_defs((*(s.defs))(*it));
        reaching_defs.insert(pred_defs.begin(), pred_defs.end());
    }

    // find the def sets for each instruction
    bb->for_each_instruction(&find_defs_reaching_instr, s);

    s.bb = 0;
    s.reaching_defs = 0;
    return true;
}

/// find all definitions reaching each use of a variable in each instruction
void find_defs_reaching_uses(cfg &flow, var_def_map &defs, use_def_map &ud) throw() {
    ud_state state;
    state.defs = &defs;
    state.ud = &(ud.ud_map);

    ud.ud_map.clear();
    flow.for_each_basic_block(&find_defs_in_bb, state);

    state.ud = 0;
    state.defs = 0;
}

/// get all definitions that are potentially used by a specific instruction
const var_def_set &use_def_map::operator()(simple_instr *in) throw() {
    return ud_map[in];
}
