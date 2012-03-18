/*
 * closure.cc
 *
 *  Created on: Jan 24, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <set>

#include "include/set.h"
#include "include/operator.h"
#include "include/data_flow/problem.h"
#include "include/data_flow/closure.h"

namespace {

    class forward_output_function {
    public:
        bool &operator()(
            IN      basic_block *bb
        ) throw() {
            return bb->entry_reachable;
        }
    };

    class backward_output_function {
    public:
        bool &operator()(
            IN      basic_block *bb
        ) throw() {
            return bb->exit_reachable;
        }
    };

    class meet_function {
    public:

        // check everything
        bool operator()(
            IN      basic_block *,
            IN      basic_block *
        ) throw() {
            return true;
        }

        // the reachability of this from the entry/exit is the boolean or of the
        // reachability of this block's incoming blocks
        void operator()(
            IN      std::set<bool> &incoming_in_closure,
            INOUT   bool &in_closure
        ) throw() {
            bool no_incoming(false);
            in_closure = set_accumulate<
                bool,
                bool,
                unary_operator<bool>::copy,
                binary_operator<bool>::logical_or
            >(
                incoming_in_closure,
                no_incoming
            );
        }
    };

    class forward_init_function {
    public:
        void operator()(
            IN      cfg &flow_graph,
            INOUT   forward_output_function &out
        ) throw() {
            out(flow_graph.entry()) = true;
        }
    };

    class backward_init_function {
    public:
        void operator()(
            IN      cfg &flow_graph,
            INOUT   backward_output_function &out
        ) throw() {
            out(flow_graph.exit()) = true;
        }
    };

    class transfer_function {
    public:
        void operator()(
            IN      basic_block *bb,
            IN      bool in,
            INOUT   bool &out
        ) {
            out = in || out;
        }
    };
}

void find_closure(cfg &flow_graph) throw() {

    data_flow_problem<
        forward_data_flow,
        bool, // domain
        meet_function,
        transfer_function,
        forward_init_function,
        forward_output_function
    > compute_forward_closure;

    forward_output_function get_entry_reachable;
    compute_forward_closure(flow_graph, get_entry_reachable);
    /*
    data_flow_problem<
            backward_data_flow,
            bool, // domain
            meet_function,
            transfer_function,
            backward_init_function,
            backward_output_function
        > compute_backward_closure;
    backward_output_function get_exit_reachable;
    compute_backward_closure(flow_graph, get_exit_reachable);
    */
}
