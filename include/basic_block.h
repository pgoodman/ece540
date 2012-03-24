/*
 * basic_block.h
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_BASIC_BLOCK_H_
#define asn1_BASIC_BLOCK_H_

extern "C" {
#   include <simple.h>
}

#include <set>
#include <functional>

#include "include/linked_list_iterator.h"

class cfg;
class basic_block;

/// represents a basic block of instructions
class basic_block {
public:

    friend class cfg;
    template <typename> friend class linked_list_iterator;

    unsigned num_instructions;

    /// first and last instructions in this basic block, inclusive
    simple_instr *first;
    simple_instr *last;

    /// set of s predecessors and successors
    std::set<basic_block *> successors_;
    std::set<basic_block *> predecessors_;

    /// next in order of allocation; this should also be in order of original
    /// instructions
    basic_block *next;
    basic_block *prev;

private:

    /// create a basic block
    basic_block(unsigned, simple_instr *, simple_instr *) throw();

public:

    /// reachability (from entry/exit basic blocks)
    bool entry_reachable;
    bool exit_reachable;

    /// number of instructions
    unsigned size(void) const throw();

    /// getters
    const std::set<basic_block *> &predecessors(void) const throw();
    const std::set<basic_block *> &successors(void) const throw();

    /// apply a function to each instruction, where the instructions are visited
    /// in order.
    template <typename T>
    bool for_each_instruction(bool (*callback)(simple_instr *, T &), T &t) {
        simple_instr *after_last(0);
        if(0 != last) {
            after_last = last->next;
        }
        for(simple_instr *in(first);
            0 != in && in != after_last;
            in = in->next) {

            if(!callback(in, t)) {
                return false;
            }
        }
        return true;
    }

    /// apply a function to each instruction, where the instructions are visited
    /// in reverse order
    template <typename T>
    bool for_each_instruction_reverse(bool (*callback)(simple_instr *, T &), T &t) {
        simple_instr *before_first(0);
        if(0 != first) {
            before_first = first->prev;
        }
        for(simple_instr *in(last);
            0 != in && in != before_first;
            in = in->prev) {

            if(!callback(in, t)) {
                return false;
            }
        }
        return true;
    }
};

/// iterator for basic blocks
typedef linked_list_iterator<basic_block> basic_block_iterator;

#endif /* asn1_BASIC_BLOCK_H_ */
