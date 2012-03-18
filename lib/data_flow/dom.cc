/*
 * dom.cc
 *
 *  Created on: Jan 21, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

extern "C" {
#   include <simple.h>
}

#include <set>
#include <map>

#include "include/data_flow/problem.h"
#include "include/data_flow/dom.h"
#include "include/set.h"
#include "include/operator.h"

namespace {

    /// intersect sets of dominators; for the entry basic block, the check for no
    /// incoming flow has the effect of keeping the outgoing dominators the same,
    /// i.e. maintaining the boundary condition
    class meet_function {
    public:

        /// this is about the only interesting thing in here. this condition
        /// allows for reachability/non-reachability to propagate, but not
        /// too far. e.g. if you have:
        ///    1 ---> 2 --------.
        ///           3 ---> 4 --`> 5
        /// then this condition stops the overly constrained dominator set of
        /// 4 from being intersected with the dominator set of 2, by saying
        /// that we only intersect those dominator sets of the predecessors of
        /// 5 that share the same reachability.
        bool operator()(
            IN      basic_block *source,
            IN      basic_block *incoming
        ) throw() {
            return source->entry_reachable == incoming->entry_reachable;
        }

        void operator()(
            IN      std::set<dominator_set> &incoming_dominators,
            INOUT   dominator_set &outgoing_dominators
        ) throw() {

            dominator_set empty_set;
            outgoing_dominators = set_accumulate<
                dominator_set,
                dominator_set,
                unary_operator<dominator_set>::copy,
                set_intersection
            >(
                incoming_dominators,
                empty_set
            );
        }
    };

    /// include the basic block in its own dominator set
    class transfer_function {
    public:
        void operator()(
            IN      basic_block *bb,
            IN      dominator_set &bb_set,
            INOUT   dominator_set &out
        ) throw() {
            out = bb_set;
            out.insert(bb);
        }
    };

    /// initialize the dominator sets.
    class init_function {
    public:
        void operator()(
            IN      cfg &flow,
            INOUT   dominator_map &outgoing
        ) throw() {
            basic_block_iterator bb_it(flow.begin());
            basic_block_iterator bb_end(flow.end());

            dominator_set all_bbs;
            dominator_set no_bbs;

            outgoing.clear();
            all_bbs.insert(bb_it, bb_end);

            for(; ++bb_it != bb_end; ) {
                basic_block *bb(*bb_it);
                if(bb->predecessors().empty()) {
                    outgoing(bb) = no_bbs; // boundary condition
                } else {
                    outgoing(bb) = all_bbs;
                }
            }
        }
    };
}

/// find all dominators
void find_dominators(cfg &flow, dominator_map &dominators) throw() {
    data_flow_problem<
        forward_data_flow,
        dominator_set, // domain
        meet_function,
        transfer_function,
        init_function
    > compute_dominator;

    dominators.clear();
    compute_dominator(flow, dominators);
}

/// remove elements from the set of dominators until only one remains, which
/// is the least upper bound, i.e. immediate dominator of some node
basic_block *find_immediate_dominator(
    dominator_map &dominators,
    basic_block *block
) throw() {

    dominator_set &doms_(dominators(block));

    // zero or one dominators, i.e. error or the block dominates itself and is
    // unreachable or the entry node
    if(1U >= doms_.size()) {
        return 0;
    }

    dominator_set doms(doms_);
    doms.erase(block);

    for(; 1U < doms.size(); ) {
        dominator_set::iterator it(doms.begin());

        basic_block *first(*it);
        basic_block *second(*++it);

        // the second basic block does not dominate the first; by partial
        // order, the first must dominate the second, so it cannot be the
        // immediate dominator
        if(0U == dominators(first).count(second)) {
            doms.erase(first);
        } else {
            doms.erase(second);
        }
    }

    return *(doms.begin());
}
