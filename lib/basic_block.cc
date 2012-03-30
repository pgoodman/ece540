/*
 * basic_block.cc
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cassert>

#include "include/basic_block.h"
#include "include/data_flow/var_def.h"
#include "include/data_flow/var_use.h"

basic_block::basic_block(unsigned num_, simple_instr *first_, simple_instr *last_) throw()
    : num_instructions(num_)
    , first(first_)
    , last(last_)
    , successors_()
    , predecessors_()
    , next(0)
    , entry_reachable(false)
    , exit_reachable(false)
{
    if(0 != first) {
        assert(0 != last && "If the first instruction is non-null then the last instruction must be non-null");
    }
}

unsigned basic_block::size(void) const throw() {
    return num_instructions;
}

const std::set<basic_block *> &basic_block::predecessors(void) const throw() {
    return predecessors_;
}

const std::set<basic_block *> &basic_block::successors(void) const throw() {
    return successors_;
}

/// forward declare two needed functions that will come up later

struct bb_reg_finder {
public:
    simple_reg *pseudo_reg;
    simple_reg *temp_reg;
    int seen_count;
};

// do the replacement
void replace_reg(simple_reg *r, simple_reg **r_loc, simple_instr *, bb_reg_finder &f) throw() {
    if(r == f.temp_reg) {
        ++(f.seen_count);
        *r_loc = f.pseudo_reg;
    }
}

/// replace a temporary register
void basic_block::replace_temp_reg(simple_reg *temp_reg) throw() {
    assert(0 != temp_reg);
    assert(TEMP_REG == temp_reg->kind);

    if(0 == last) {
        return;
    }

    simple_reg *pseudo_reg(new_register(temp_reg->var->type, PSEUDO_REG));
    bb_reg_finder f;
    f.pseudo_reg = pseudo_reg;
    f.temp_reg = temp_reg;
    f.seen_count = 0;

    // replace at all usage points

    for(simple_instr *in(first), *end(last->next);
        in != end;
        in = in->next) {

        // replace the definition
        if(0 == f.seen_count) {
            for_each_var_def(&replace_reg, in, f);

        // replace the use
        } else if(1 == f.seen_count) {
            for_each_var_use(&replace_reg, in, f);

        // we're done
        } else {
            break;
        }
    }
}
