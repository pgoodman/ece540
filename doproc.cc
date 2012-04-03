

#define MAIN static simple_instr *print_dot(simple_instr *inlist, char *proc_name)
#   include "dot.cc"
#undef MAIN

#include "include/optimizer.h"
#include "include/opt/cf.h"
#include "include/opt/cp.h"
#include "include/opt/dce.h"
#include "include/opt/cse.h"
#include "include/opt/licm.h"
#include "include/opt/eval.h"

static optimizer::pass CF, CP, CP_2, DCE, CSE, LICM, EVAL;

/// set up and run the optimizer pipeline.
simple_instr *do_procedure(simple_instr *in_list, char *proc_name) {

    optimizer o(in_list);

    CP = o.add_pass(propagate_copies);
    CF = o.add_pass(fold_constants);
    DCE = o.add_pass(eliminate_dead_code);
    CSE = o.add_pass(eliminate_common_sub_expressions);
    LICM = o.add_pass(hoist_loop_invariant_code);
    EVAL = o.add_pass(abstract_evaluator);

    //               5           7
    //    1 .--------<---------.-<--.                 10
    //  .-<-.   2      4       |    |              .--->---.
    // -`-> CP ->- CF ->- DCE -'->- CSE ->- LICM -'- DCE ---`>-- EVAL
    //       `--<--'             6       8        9         11
    //          3

    o.cascade_if(CP, CP, true);         // 1
    o.cascade_if(CP, CF, false);        // 2
    o.cascade_if(CF, CP, true);         // 3
    o.cascade_if(CF, DCE, false);       // 4
    o.cascade_if(DCE, CP, true);        // 5
    o.cascade_if(DCE, CSE, false);      // 6
    o.cascade_if(CSE, CP, true);        // 7
    o.cascade_if(CSE, LICM, false);     // 8

    DCE = o.add_pass(eliminate_dead_code);

    o.cascade_if(LICM, DCE, true);      // 9

    o.cascade_if(LICM, EVAL, false);    // 10
    o.cascade(DCE, EVAL);               // 11

    //  -.                      17          19
    //   | 10,11    13 .--------<--------.--<--.
    //   |        .-<-.  14              |     |
    // EVAL -->---`-> CP ->- CF ->- DCE -'->- CSE -->-- DONE!
    //       12        `--<--'  16        18
    //                   15

    CP_2 = o.add_pass(propagate_copies);
    CF = o.add_pass(fold_constants);
    DCE = o.add_pass(eliminate_dead_code);
    CSE = o.add_pass(eliminate_common_sub_expressions);

    o.cascade(EVAL, CP_2);              // 12
    o.cascade_if(CP_2, CP_2, true);     // 13
    o.cascade_if(CP_2, CF, false);      // 14
    o.cascade_if(CF, CP_2, true);       // 15
    o.cascade_if(CF, DCE, false);       // 16
    o.cascade_if(DCE, CP_2, true);      // 17
    o.cascade_if(DCE, CSE, false);      // 18
    o.cascade_if(CSE, CP_2, true);      // 19

    o.run(CP);

    //return o.first_instruction();
    return print_dot(o.first_instruction(), proc_name);
}
