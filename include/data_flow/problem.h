/*
 * data_flow_problem.h
 *
 *  Created on: Jan 15, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_DATA_FLOW_PROBLEM_H_
#define asn1_DATA_FLOW_PROBLEM_H_

#define IN
#define OUT
#define INOUT

#include <map>
#include <set>
#include <cstdio>

#include "include/cfg.h"
#include "include/basic_block.h"
#include "include/partial_function.h"

class forward_data_flow { };
class backward_data_flow { };

/// having the direction as a typename ends up being ugly on this side of
/// things but self-documenting from the usage side :D
namespace support {
    template <typename>
    class direction_as_bool;

    template <>
    class direction_as_bool<forward_data_flow> {
    public:
        enum {
            IS_FORWARD = true
        };
    };

    template <>
    class direction_as_bool<backward_data_flow> {
    public:
        enum {
            IS_FORWARD = false
        };
    };

    template <typename Domain>
    class null_finalizer {
    public:
        void operator()(
            IN      basic_block *,
            INOUT   Domain &
        ) throw() { }
    };
}

/// general framework for solving a data-flow problems; this is just the
/// forward data-flow problem coded up, with a flag to make it operate like
/// a backward data-flow problem.
///
/// Init :: IN(cfg) -> INOUT(OutputFunction) -> void
/// Meet :: IN({Domain}) -> INOUT(Domain) -> void
///      :: IN(basic_block *) -> IN(basic_block *) -> OUT(bool)
/// TransferFunction :: IN(basic_block *) -> IN(Domain) -> INOUT(Domain) -> void
/// FinalizeFunction :: IN(basic_block *) -> INOUT(Domain) -> void
/// OutputFunction :: IN(basic_block *) -> INOUT(Domain)
template <
    typename Direction,
    typename Domain,
    typename Meet,
    typename TransferFunction,
    typename InitFunction,
    typename OutputFunction=partial_function<basic_block *, Domain>,
    typename FinalizeFunction=support::null_finalizer<Domain>
>
class data_flow_problem {
private:

    Meet merge;
    Meet &can_merge;
    TransferFunction update;
    FinalizeFunction finalize;
    InitFunction init;

    /// the type of the predecessors/successors method on basic blocks.
    typedef const std::set<basic_block *> &(basic_block::*incoming_method_pointer)() const;

    incoming_method_pointer incoming;

public:

    data_flow_problem(void) throw()
        : can_merge(merge)
        , incoming(support::direction_as_bool<Direction>::IS_FORWARD ?
                        &basic_block::predecessors :
                        &basic_block::successors)
    { }

    data_flow_problem(TransferFunction &trans) throw()
        : can_merge(merge)
        , update(trans)
        , incoming(support::direction_as_bool<Direction>::IS_FORWARD ?
                        &basic_block::predecessors :
                        &basic_block::successors)
    { }

    /// compute a data-flow problem; see p. 627 of Aho, Lam, Sethi, and
    /// Ullman; 2nd edition; generalized so that forward and backward
    /// data flow problems are solved by the same code. That is, "incoming"
    /// information can flow from predecessor or successor basic blocks.
    void operator()(
        IN      cfg &flow_graph,
        INOUT   OutputFunction &outgoing
    ) throw() {

        // set the initial and boundary conditions for the data-flow
        // problem
        init(flow_graph, outgoing);

        const basic_block_iterator blocks_begin(flow_graph.begin());
        const basic_block_iterator blocks_end(flow_graph.end());
        basic_block *bb(0);

        std::set<basic_block *>::const_iterator incoming_begin, incoming_end;

        // while there has been an update to the outgoing set
        for(bool updated_outgoing(true); updated_outgoing; ) {
            updated_outgoing = false;

            // for each basic block
            for(basic_block_iterator block_it(blocks_begin);
                block_it != blocks_end;
                ++block_it) {

                bb = *block_it;

                // collect all incoming outputs. they can be incoming in
                // either the forward or backward direction
                std::set<Domain> incoming_outputs;
                incoming_begin = (bb->*incoming)().begin();
                incoming_end = (bb->*incoming)().end();

                for(; incoming_begin != incoming_end; ++incoming_begin) {
                    if(can_merge(bb, *incoming_begin)) {
                        incoming_outputs.insert(outgoing(*incoming_begin));
                    }
                }

                // keep track of old output and prepare for a new output
                Domain &new_output(outgoing(bb));
                Domain merged_incoming_outputs(new_output);
                Domain old_output(new_output);

                // merge incoming outputs into a final output
                merge(incoming_outputs, merged_incoming_outputs);
                update(bb, merged_incoming_outputs, new_output);

                // check if we should continue
                updated_outgoing = updated_outgoing || (new_output != old_output);
            }
        }

        // finalize things; this is an extra step to the framework to allow for
        // the case where we want information to flow through the graph, even
        // if there appear to be mutliple entries (in the case of a completely
        // unreachable basic block that is the predecessor to other blocks and
        // isn't the entry block), but where we also don't want to keep around
        // that extra info to make the rest of the framework work
        for(basic_block_iterator block_it(blocks_begin);
            block_it != blocks_end;
            ++block_it) {

            bb = *block_it;
            finalize(bb, outgoing(bb));
        }
    }
};

/// generic any-path meet function where the domain is an iterable of some
/// sort.
template <typename Domain>
class powerset_union_meet_function {
public:

    bool operator()(
        IN      basic_block *, // curr
        IN      basic_block *  // incoming
    ) throw() {
        return true;
    }

    void operator()(
        IN      std::set<Domain> &incoming_sets,
        INOUT   Domain &merged_set
    ) throw() {
        typename std::set<Domain>::iterator it(incoming_sets.begin())
                                          , end(incoming_sets.end());

        for(merged_set.clear(); it != end; ++it) {
            merged_set.insert(it->begin(), it->end());
        }
    }
};

#endif /* asn1_DATA_FLOW_PROBLEM_H_ */
