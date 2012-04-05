/*
 * eval.h
 *
 *  Created on: Apr 1, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_EVAL_H_
#define project_EVAL_H_


extern "C" {
#   include <simple.h>
}

class optimizer;

namespace eval {
    typedef enum {
        RETURNED,
        REACHED_BREAKPOINT,
        UNKNOWN
    } breakpoint_status;
}

eval::breakpoint_status abstract_evaluator_bp(
    optimizer &o,
    simple_instr *first_instr,
    simple_instr *break_point
) throw();

void abstract_evaluator(optimizer &) throw();


#endif /* project_EVAL_H_ */
