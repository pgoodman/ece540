/*
 * var_use.cc
 *
 *  Created on: Feb 20, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn3_VAR_USE_CC_
#define asn3_VAR_USE_CC_

#include "include/data_flow/problem.h"
#include "include/data_flow/var_use.h"
#include "include/data_flow/var_def.h"

/// compare two variable usages by the addresses of their registers; groups by
/// register, tie-breaks by instruction
bool var_use::operator<(const var_use &that) const throw() {
    if(reg == that.reg) {
        return in < that.in;
    }

    return reg < that.reg;
}

/// compare two var_use's for equivalence
bool var_use::operator==(const var_use &that) const throw() {
    return reg == that.reg && in == that.in && usage == that.usage;
}

/// delete all usages of a variable from a set by the register
void var_use_set::erase(simple_reg *reg) throw() {
    iterator first(find(reg));
    iterator last(first);
    for(iterator end(this->end()); last != end && last->reg == reg; ++last) {
        // loop :D
    }

    this->std::set<var_use>::erase(first, last);
}

/// find the first use of a register in a var use set
var_use_set::iterator var_use_set::find(simple_reg *reg) throw() {
    var_use use;
    use.in = 0; // guaranteed to be a lower bound :D
    use.reg = reg;
    use.usage = 0;
    iterator pos(this->upper_bound(use));
    if(pos->reg != reg) {
        pos = this->end();
    }
    return pos;
}

namespace {

    /// add a variable use to the uses set
    static void add_var_use(
        simple_reg *reg,
        simple_reg **reg_loc,
        simple_instr *in,
        var_use_set &uses
    ) throw() {
        var_use use;
        use.reg = reg;
        use.usage = reg_loc;
        use.in = in;
        uses.insert(use);
    }

    /// remove a variable use from the use set because of a variable definition
    static void remove_var_use(
        simple_reg *reg,
        simple_reg **,
        simple_instr *,
        var_use_set &uses
    ) throw() {
        uses.erase(reg);
    }

    /// compute the incremental used variables at this point
    static bool update_used_vars(
        IN      simple_instr *in,
        INOUT   var_use_set &incoming_uses
    ) throw() {
        for_each_var_def(remove_var_use, in, incoming_uses);
        for_each_var_use(add_var_use, in, incoming_uses);
        return true;
    }

    /// compute the set of variable usages that reach the beginning of each
    /// basic block.
    static bool compute_used_vars(
        IN      basic_block *bb,
        INOUT   var_use_set &incoming_uses
    ) throw() {
        return bb->for_each_instruction_reverse(&update_used_vars, incoming_uses);
    }

    /// initialize the problem with the local gen set of each basic block
    class init_function {
    public:

        static bool init_bb_used_vars(
            IN      basic_block *bb,
            INOUT   var_use_map &used_vars
        ) throw() {
            return compute_used_vars(bb, used_vars(bb));
        }

        void operator()(
            IN      cfg &flow_graph,
            INOUT   var_use_map &used_vars
        ) throw() {
            flow_graph.for_each_basic_block(&init_bb_used_vars, used_vars);
        }
    };

    /// find the reaching definitions that make it to the end of a basic block,
    /// given those that made it to the beginning
    class transfer_function {
    public:
        void operator()(
            IN      basic_block *bb,
            IN      var_use_set &incoming_uses, // from successors
            INOUT   var_use_set &outgoing_uses // to predecessors
        ) throw() {
            outgoing_uses = incoming_uses;
            compute_used_vars(bb, outgoing_uses);
        }
    };
}

/// find the set of live variables that enter each basic block
void find_var_uses(cfg &flow, var_use_map &var_uses) throw() {
    data_flow_problem<
        backward_data_flow,
        var_use_set, // domain
        powerset_union_meet_function<var_use_set>,
        transfer_function,
        init_function,
        var_use_map
    > compute_live_vars;

    var_uses.clear();
    compute_live_vars(flow, var_uses);
}

/// find the set of local variable uses for each basic block; this does
/// not do data-flow analysis
void find_local_var_uses(cfg &flow, var_use_map &var_uses) throw() {
    init_function init;
    init(flow, var_uses);
}

#endif /* asn3_VAR_USE_CC_ */
