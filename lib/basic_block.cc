/*
 * basic_block.cc
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cassert>

#include "include/basic_block.h"

basic_block::basic_block(unsigned id_, unsigned num_, simple_instr *first_, simple_instr *last_) throw()
    : id(id_)
    , num_instructions(num_)
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

/// compare two basic blocks by their ids
bool basic_block_less(const basic_block *x, const basic_block *y) throw() {
    if(0 == x && 0 == y) {
        return false;
    } else if(0 == x) {
        return true;
    } else if(0 == y) {
        return false;
    }
    return x->id < y->id;
}

