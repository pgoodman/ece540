/*
 * licm.cc
 *
 *  Created on: Mar 29, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

extern "C" {
#   include <simple.h>
}

#include <cassert>
#include <vector>
#include <set>
#include <map>

#include "include/optimizer.h"
#include "include/cfg.h"
#include "include/loop.h"

#include "include/use_def.h"

#include "include/opt/licm.h"

/// go find the exit nodes of the loop
static void find_loop_exit_bbs(
    loop &loop,
    std::vector<basic_block *> &loop_exits
) throw() {
    std::set<basic_block *>::const_iterator it(loop.body.begin())
                                          , end(loop.body.end());

    for(; it != end; ++it) {
        const basic_block *bb(*it);

        if(0 == bb->last) {
            continue;
        }

        switch(bb->last->opcode) {

        // we only care if one of their successors is not in the loop's body
        case BTRUE_OP: case BFALSE_OP: case MBR_OP:
            const std::set<basic_block *> &succ(bb->successors());
            std::set<basic_block *>::const_iterator succ_it(succ.begin())
                                                  , succ_end(succ.end());

            for(; succ_it != succ_end; ++succ_it) {
                if(0U == loop.body.count(*succ_it)) {
                    loop_exits.push_back(*succ_it);
                }
            }

            break;

        // note: by definition, a fall-through or direct jump cannot
        //       be an exit of the loop, as each only has one successor
        default:
            assert(1U == bb->successors().size());
            break;
        }
    }
}

/// imposes a total ordering on loops; this takes advantage of the fact that
/// nested loops must strictly have fewer basic blocks than their enclosing
/// loops
struct loop_less {
public:
    bool operator()(const loop *a, const loop *b) const throw() {
        if(a == b) {
            return false;
        }

        const size_t a_size(a->body.size() + a->tails.size());
        const size_t b_size(b->body.size() + b->tails.size());

        if(a_size < b_size) {
            return true;
        } else if(b_size < a_size) {
            return false;
        } else {
            return a < b;
        }
    }
};

/// order the loops so that we visit nested loops before we visit their
/// enclosing loops; this will make it so that hopefully stuff can be hoisted
/// out of multiple loops at once. this is tricky because the loop_map totally
/// orders the loops by their basic block points, but the loops themselves only
/// respect a partial order, and so must be re-ordered. The re-ordered is based
/// on lexicographic order of loop size and loop head pointer.
static bool add_loop_to_set(loop &l, std::set<loop *, loop_less> &ordered_loops) throw() {
    ordered_loops.insert(&l);
    return true;
}
static void order_loops(
    loop_map &loops,
    std::vector<loop *> &ordered_loops
) throw() {
    const unsigned num_loops(static_cast<unsigned>(loops.size()));
    ordered_loops.reserve(num_loops);
    std::set<loop *, loop_less> loop_set;
    loops.for_each_loop(add_loop_to_set, loop_set);
    ordered_loops.insert(ordered_loops.begin(),
        loop_set.begin(),
        loop_set.end()
    );
}

/// apply a function to each basic blook in a loop
template <typename T0>
static void for_each_basic_block(
    loop &ll,
    void (*func)(basic_block *, T0 &),
    T0 &t0
) throw() {
    std::set<basic_block *>::iterator it(ll.body.begin())
                                    , end(ll.body.end());

    for(; it != end; ++it) {
        func(*it, t0);
    }
}

/// type used for counting defs/uses of variables
struct variable_counter {
public:
    std::map<simple_reg *, unsigned> *counter;
    bool (*count_what)(
        void (*)(
            simple_reg *,
            simple_reg **,
            simple_instr *,
            std::map<simple_reg *, unsigned> &
        ),
        simple_instr *,
        std::map<simple_reg *, unsigned> &
    );
};

/// increment the counter for a variable
static void increment_var_counter(
    simple_reg *var,
    simple_reg **,
    simple_instr *,
    std::map<simple_reg *, unsigned> &counter
) throw() {
    counter[var] += 1U;
}

/// count the variables
static void count_variables(basic_block *bb, variable_counter &counter) throw() {
    if(0 == bb->last) {
        return;
    }

    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
        counter.count_what(increment_var_counter, in, *(counter.counter));
    }
}

/// type used for keeping track of invariant instructions
struct invariant_tracker {
public:
    std::set<simple_instr *> *invariant_ins;
    std::map<simple_reg *, unsigned> *num_uses;
    std::map<simple_reg *, unsigned> *num_defs;
    std::set<simple_reg *> *invariant_regs;
    dominator_map *doms;
};

static void find_invariant_ins(basic_block *bb, invariant_tracker &it) throw() {
    if(0 == bb->last) {
        return;
    }

    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {

        // already marked or it's a function call; continue. note: function call
        // is possibly a var def, but functions can have side-effects so we want
        // to avoid them
        if(it.invariant_ins->count(in)
        || CALL_OP == in->opcode) {
            continue;
        }

        // this is a variable definition
        simple_reg *defd_var(0);
        if(for_each_var_def(in, defd_var)) {

            // it's defined multiple times in the loop; ignore
            if(1U < (*(it.num_defs))[defd_var]) {
                continue;
            }

            // all of the arguments are loop invariable
        }
    }
}

/// hoist code out of an individual loop
static void hoist_code(optimizer &o, use_def_map &udm, dominator_map &dm, loop &loop) throw() {

    // get all exits of the loop; we need to make sure the definitions of variables
    // dominate the exits
    std::vector<basic_block *> loop_exits;
    find_loop_exit_bbs(loop, loop_exits);

    // go identify how many definitions of each variable there are
    variable_counter counter;
    std::map<simple_reg *, unsigned> num_defs;
    counter.count_what = for_each_var_def;
    counter.counter = &num_defs;
    for_each_basic_block(loop, count_variables, counter);

    // go identify how many uses of each variable there are in the loop
    std::map<simple_reg *, unsigned> num_uses;
    counter.count_what = for_each_var_use;
    counter.counter = &num_uses;
    for_each_basic_block(loop, count_variables, counter);

    // go identify invariant instructions; this will try to grow the set of
    // invariant instructions until we can't
    std::set<simple_instr *> invariant_ins;
    std::set<simple_reg *> invariant_regs;
    invariant_tracker it;
    it.num_defs = &num_defs;
    it.num_uses = &num_uses;
    it.doms = &dm;
    it.invariant_ins = &invariant_ins;
    size_t old_size(0U);
    do {
        old_size = invariant_ins.size();
        for_each_basic_block(loop, find_invariant_ins, it);
    } while(old_size < invariant_ins.size());
}

/// hoist loop-invariant code out of all loops in a CFG
void hoist_loop_invariant_code(optimizer &o, use_def_map &udm, dominator_map &dm, loop_map &lm) throw() {
#ifndef ECE540_DISABLE_LICM
    std::vector<loop *> loops;
    order_loops(lm, loops);

    for(unsigned i(0U); i < loops.size(); ++i) {
        hoist_code(o, udm, dm, *(loops[i]));
    }
#endif
}
