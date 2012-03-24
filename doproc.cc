

#define MAIN static simple_instr *print_dot(simple_instr *inlist, char *proc_name)
#   include "dot.cc"
#undef MAIN

#include "include/optimizer.h"
#include "include/opt/cf.h"
#include "include/opt/cp.h"
#include "include/opt/dce.h"

static optimizer::pass CF, CP, DCE;

/// set up and run the optimizer
///
/// the optimizer is organized in terms of groups of single pass optimizations,
/// where each group is treated as an optimization. The distinction exists to
/// allow cycles among optimization groups but not optimizations.
simple_instr *do_procedure(simple_instr *in_list, char *proc_name) {

    optimizer o(in_list);

    CF = o.add_pass(fold_constants);
    CP = o.add_pass(propagate_copies);
    DCE = o.add_pass(eliminate_dead_code);

    //    1
    //  .---.   2      4
    // -`-> CP --> CF --> DCE -->
    //       ^-----'
    //          3
    o.cascade(CP, CF);
    o.cascade_if(CP, CP, true);
    o.cascade_if(CF, CP, true);
    o.cascade_if(CF, DCE, false);

    // loop invariant code motion
    // common subexpression elimination
    // local value numbering

    o.run(CP);

    return print_dot(in_list, proc_name);
}
