

#define MAIN static simple_instr *print_dot(simple_instr *inlist, char *proc_name)
#   include "dot.cc"
#undef MAIN

#include "include/opt/cf.h"

/// print out the basic blocks and immediate dominators for some
simple_instr *do_procedure(simple_instr *in_list, char *proc_name) {

    cfg flow_graph(in_list);

    fold_constants(flow_graph);

    return print_dot(in_list, proc_name);
}
