/*
 * optimizer.cc
 *
 *  Created on: Mar 23, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cstring>

#include "include/optimizer.h"

optimizer::optimizer(simple_instr *in) throw()
    : instructions(in)
    , flow_graph(in)
{
    // make sure to get the potentially updated first instruction (forced to be
    // a label)
    instructions = flow_graph.entry()->next->first;

    memset(&dirty, ~0, sizeof dirty);

    dirty.cfg = false;
    dirty.padding_ = 0;
}

/// internal getters based on type tags; these handle getting references to
/// internal data structures and making sure that they are up to date.

cfg &optimizer::get(optimizer &self, tag<cfg>, bool is_forced) throw() {
    if(self.dirty.cfg || is_forced) {
        self.flow_graph.~cfg();
        new (&(self.flow_graph)) cfg(self.instructions);
        self.dirty.cfg = false;

        // propagate
        self.dirty.doms = true;
        self.dirty.ae = true;
        self.dirty.var_use = true;
        self.dirty.var_def = true;
        self.dirty.loops = true;
    }
    return self.flow_graph;
}

dominator_map &optimizer::get(optimizer &self, tag<dominator_map>, bool is_forced) throw() {
    get(self, tag<cfg>(), false);
    if(self.dirty.doms || is_forced) {
        find_dominators(self.flow_graph, self.dominators);
        self.dirty.doms = false;
        self.dirty.loops = true;
    }
    return self.dominators;
}

available_expression_map &optimizer::get(optimizer &self, tag<available_expression_map>, bool is_forced) throw() {
    get(self, tag<cfg>(), false);
    if(self.dirty.ae || is_forced) {
        find_available_expressions(self.flow_graph, self.available_expressions);
        self.dirty.ae = false;
    }
    return self.available_expressions;
}

var_def_map &optimizer::get(optimizer &self, tag<var_def_map>, bool is_forced) throw() {
    get(self, tag<cfg>(), false);
    if(self.dirty.var_def || is_forced) {
        find_var_defs(self.flow_graph, self.var_defs);
        self.dirty.var_def = false;
        self.dirty.du = true;
        self.dirty.ud = true;
    }
    return self.var_defs;
}

var_use_map &optimizer::get(optimizer &self, tag<var_use_map>, bool is_forced) throw() {
    get(self, tag<cfg>(), false);
    if(self.dirty.var_use || is_forced) {
        find_var_uses(self.flow_graph, self.var_uses);
        self.dirty.var_use = false;
        self.dirty.du = true;
        self.dirty.ud = true;
    }
    return self.var_uses;
}

loop_map &optimizer::get(optimizer &self, tag<loop_map>, bool is_forced) throw() {
    get(self, tag<dominator_map>(), false);
    if(self.dirty.loops || is_forced) {
        find_loops(self.flow_graph, self.dominators, self.loops);
        self.dirty.loops = false;
    }
    return self.loops;
}

use_def_map &optimizer::get(optimizer &self, tag<use_def_map>, bool is_forced) throw() {
    get(self, tag<var_def_map>(), false);
    if(self.dirty.ud || is_forced) {
        find_defs_reaching_uses(self.flow_graph, self.var_defs, self.ud_chain);
        self.dirty.ud = false;
    }
    return self.ud_chain;
}

def_use_map &optimizer::get(optimizer &self, tag<def_use_map>, bool is_forced) throw() {
    get(self, tag<var_use_map>(), false);
    if(self.dirty.du || is_forced) {
        find_uses_reaching_defs(self.flow_graph, self.var_uses, self.du_chain);
        self.dirty.du = false;
    }
    return self.du_chain;
}

/// dependency injector with no dependent arguments
void optimizer::inject0(void (*callback)(optimizer &), optimizer &self) throw() {
    callback(self);
}

/// add a pass with no dependent arguments
optimizer::pass optimizer::add_pass(void (*func)(optimizer &)) throw() {
    internal_pass p;
    p.untyped_func = func;
    p.unwrapper = &inject0;
    pass pass_id(static_cast<unsigned>(passes.size()));
    passes.push_back(p);
    return pass_id;
}

/// notify the dirty struct that some things have been changed

void optimizer::changed_def(void) throw() {
    dirty.ae = true;
    dirty.var_def = true;
    changed_something = true;
}

void optimizer::changed_use(void) throw() {
    dirty.ae = true;
    dirty.ud = true;
    dirty.var_use = true;
    changed_something = true;
}

void optimizer::changed_block(void) throw() {
    dirty.cfg = true;
    changed_something = true;
}

/// signal that a NOP has been removed; this is a special case to prevent
/// infinite loops between the CFG aggressively replacing unused labels with
/// NOPs and the deadcode eliminator removing NOPs. This dirties the CFG data
/// structure *without* reporting that we changed anything
void optimizer::removed_nop(void) throw() {
    dirty.cfg = true;
}

/// add an unconditional cascading relation between two optimization passes
void optimizer::cascade(pass &first, pass &second) throw() {
    cascade_if(first, second, true);
    cascade_if(first, second, false);
}

/// add a conditional cascading relationship between two optimization passes
void optimizer::cascade_if(pass &first, pass &second, bool first_succeeds) throw() {
    cascades[!!first_succeeds][first].insert(second);
}

/// run the optimizer
bool optimizer::run(pass &first) throw() {
    std::vector<pass> work_list;
    work_list.push_back(first);

    internal_pass thunk;

    bool ret(false);

    while(!work_list.empty()) {

        pass curr(work_list.back());
        work_list.pop_back();

        //fprintf(stderr, "pass %u\n", curr);

        thunk = passes[curr];
        changed_something = false;
        thunk.unwrapper(thunk.untyped_func, *this);

        // compare the dirty and prev_dirty structs for differences
        ret = ret || changed_something;

        // cascade, based on if there were any changes or not
        std::set<pass> &curr_cascade(cascades[changed_something][curr]);
        work_list.insert(work_list.end(), curr_cascade.begin(), curr_cascade.end());
    }

    return ret;
}

simple_instr *optimizer::first_instruction(void) throw() {
    return instructions;
}
