/*
 * def_use.cc
 *
 *  Created on: Mar 30, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <utility>

#include "include/def_use.h"
#include "include/use_def.h"
#include "include/basic_block.h"
#include "include/cfg.h"

struct du_state {
public:
    std::map<simple_instr *, var_use_set> *du;
    var_use_map *uses;
    basic_block *bb;
    var_use_set *reached_uses;
};

/// go collect each use that a definition might reach
static void add_use_for_def(
    simple_reg *reg,
    simple_reg **reg_loc,
    simple_instr *in,
    du_state &s
) throw() {
    var_use use;
    use.reg = reg;
    use.usage = reg_loc;
    use.in = in;
    use.bb = s.bb;
    s.reached_uses->insert(use);
}

/// find all definitions that are used by each instruction
static bool find_uses_reached_by_def(simple_instr *in, du_state &s) throw() {
    simple_reg *defd_var(0);
    var_use_set &use_set((*(s.du))[in]);

    // not a var def
    if(!for_each_var_def(in, defd_var)) {
        return true;
    }

    // add every use of this variable to the set of uses reached by this
    // instruction
    var_use_set::iterator curr(s.reached_uses->find(defd_var))
                        , end(s.reached_uses->end());
    for(; curr != end && curr->reg == defd_var; ++curr) {
        use_set.insert(*curr);
    }

    // kill a defined register; a bit of a re-implementation of reaching
    // definitions
    s.reached_uses->erase(defd_var);

    // add in uses of any variable used
    for_each_var_use(add_use_for_def, in, s);

    return true;
}

/// for each variable definition in a block, find all of the uses that the
/// definition reaches
static bool find_defs_in_bb(basic_block *bb, du_state &s) throw() {
    var_use_set reached_uses;
    s.bb = bb;
    s.reached_uses = &reached_uses;

    // iterate over successors and build up a set of live variables
    const std::set<basic_block *> &succs(bb->successors());
    std::set<basic_block *>::const_iterator it(succs.begin()), it_end(succs.end());
    for(; it != it_end; ++it) {
        const var_use_set &succ_uses((*(s.uses))(*it));
        reached_uses.insert(succ_uses.begin(), succ_uses.end());
    }

    // find the use sets for each instruction
    bb->for_each_instruction_reverse(&find_uses_reached_by_def, s);

    s.bb = 0;
    s.reached_uses = 0;
    return true;
}

/// compute the definitions that reach each variable use
void find_uses_reaching_defs(
    cfg &flow,
    var_use_map &uses,
    def_use_map &du
) throw() {
    du_state state;
    state.uses = &uses;
    state.du = &(du.du_map);

    du.du_map.clear();
    flow.for_each_basic_block(&find_defs_in_bb, state);

    state.du = 0;
    state.uses = 0;
}

/// get all definitions that are potentially used by a specific instruction
const var_use_set &def_use_map::operator()(simple_instr *in) throw() {
    return du_map[in];
}
