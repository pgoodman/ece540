/*
 * var_def.cc
 *
 *  Created on: Feb 11, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cassert>

#include "include/cfg.h"
#include "include/basic_block.h"
#include "include/instr.h"

#include "include/data_flow/var_def.h"
#include "include/data_flow/problem.h"

/// group definitions by register, tie-break using the register
bool compare_var_defs::operator()(
    const simple_instr *a,
    const simple_instr *b
) const throw() {
    const simple_reg *ar(a->u.base.dst), *br(b->u.base.dst);
    if(ar == br) {
        return a < b;
    }
    return ar < br;
}

namespace {

    /// compute the incremental reaching definitions of a single instruction
    static bool update_reaching_defs(
        IN      simple_instr *in,
        INOUT   var_def_set &incoming_defs
    ) throw() {
        if(instr::is_var_def(in)) {
            incoming_defs.erase(in);
            incoming_defs.insert(in);
        }
        return true;
    }

    /// compute the set of variable definitions that reach the end of a basic
    /// block given the set that are incoming.
    static bool compute_reaching_defs(
        IN      basic_block *bb,
        INOUT   var_def_set &incoming_defs
    ) throw() {
        return bb->for_each_instruction(&update_reaching_defs, incoming_defs);
    }

    /// initialize the problem with the local gen set of each basic block
    class init_function {
    public:

        static bool init_bb_reaching_defs(
            IN      basic_block *bb,
            INOUT   var_def_map &defd_vars
        ) throw() {
            return compute_reaching_defs(bb, defd_vars(bb));
        }

        void operator()(
            IN      cfg &flow_graph,
            INOUT   var_def_map &defd_vars
        ) throw() {
            flow_graph.for_each_basic_block(init_bb_reaching_defs, defd_vars);
        }
    };

    /// find the reaching definitions that make it to the end of a basic block,
    /// given those that made it to the beginning
    class transfer_function {
    public:
        void operator()(
            IN      basic_block *bb,
            IN      var_def_set &incoming_defs,
            INOUT   var_def_set &outgoing_defs
        ) throw() {
            outgoing_defs = incoming_defs;
            compute_reaching_defs(bb, outgoing_defs);
        }
    };
}

/// find the variable definitions that reach the end of each basic block
void find_var_defs(cfg &flow, var_def_map &var_defs) throw() {
    data_flow_problem<
        forward_data_flow,
        var_def_set, // domain
        powerset_union_meet_function<var_def_set>,
        transfer_function,
        init_function,
        var_def_map
    > compute_reaching_defs;

    var_defs.clear();
    compute_reaching_defs(flow, var_defs);
}

/// find the set of local variable definitions for each basic block; this does
/// not do data-flow analysis
void find_local_var_defs(cfg &flow, var_def_map &var_defs) throw() {
    init_function init;
    init(flow, var_defs);
}

/// return true if the input instruction defines and variable and assign to
/// the input register the variable assigned by the instruction
static void get_def_register(simple_reg *dst_, simple_reg **, simple_instr *, simple_reg *&dst) throw() {
    dst = dst_;
}
bool for_each_var_def(simple_instr *in, simple_reg *&reg) throw() {
    return for_each_var_def(&get_def_register, in, reg);
}

