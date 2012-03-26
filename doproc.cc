

#define MAIN static simple_instr *print_dot(simple_instr *inlist, char *proc_name)
#   include "dot.cc"
#undef MAIN

#include "include/optimizer.h"
#include "include/opt/cf.h"
#include "include/opt/cp.h"
#include "include/opt/dce.h"
#include "include/opt/cse.h"

static optimizer::pass CF, CP, DCE, CSE;

/// set up and run the optimizer pipeline.
simple_instr *do_procedure(simple_instr *in_list, char *proc_name) {

    optimizer o(in_list);

    CP = o.add_pass(propagate_copies);
    CF = o.add_pass(fold_constants);
    DCE = o.add_pass(eliminate_dead_code);
    CSE = o.add_pass(eliminate_common_sub_expressions);

    //               5           7
    //    1 .--------<---------.-<--.
    //  .-<-.   2      4       |    |
    // -`-> CP ->- CF ->- DCE -'->- CSE ->-
    //       `--<--'             6       8
    //          3

    o.cascade_if(CP, CP, true);     // 1
    o.cascade_if(CP, CF, false);    // 2
    o.cascade_if(CF, CP, true);     // 3
    o.cascade_if(CF, DCE, false);   // 4
    o.cascade_if(DCE, CP, true);    // 5
    o.cascade_if(DCE, CSE, false);  // 6
    o.cascade_if(CSE, CP, true);    // 7

    // loop invariant code motion
    // common subexpression elimination
    // local value numbering

    o.run(CP);

    return o.first_instruction();
    //return print_dot(o.first_instruction(), proc_name);
}
