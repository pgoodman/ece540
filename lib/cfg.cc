/*
 * cfg.cc
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <map>
#include <set>
#include <cassert>
#include <cstdio>

#include "include/cfg.h"
#include "include/instr.h"
#include "include/diag.h"
#include "include/data_flow/closure.h"

/// go find all labels in the instruction stream
static void find_labels(
    simple_instr *in,
    std::map<simple_sym *, simple_instr *> &labels
) throw() {
    for(; 0 != in; in = in->next) {
        if(!instr::is_label(in)) {
            continue;
        }

        labels[in->u.label.lab] = in;
    }
}

/// find the end of a basic block, starting from the input instruction
static simple_instr *find_bb_end(simple_instr *in, unsigned &num) throw() {
    simple_instr *prev(0);
    for(simple_instr *curr(in); 0 != curr; prev = curr, curr = curr->next) {
        ++num;

        if(instr::is_local_control_flow_transfer(curr)
        || instr::is_return(curr)) {
            return curr;

        // some branch targets this instruction, so the basic block should
        // end before this instruction iff this isn't the first instruction
        // in the basic block that we're trying to find
        } else if(instr::is_label(curr) && in != curr) {
            --num;
            return prev;
        }
    }

    return prev;
}

/// connect two basic blocks
void cfg::connect_bbs(basic_block *pred, basic_block *succ) throw() {
    if(0 == pred || 0 == succ) {
        return;
    }

    pred->successors_.insert(succ);
    succ->predecessors_.insert(pred);
}

/// given a symbol, look up the instruction and then block that this symbol
/// should bring us to. Checks for usage of unknown symbols, and if its a
/// good use, then it removes from the set of unused symbols.
static basic_block *lookup_and_use_label(
    simple_sym *sym,
    std::map<simple_sym *, simple_instr *> &sym_to_instr,
    std::map<simple_instr *, basic_block *> &instr_to_block
) throw() {
    if(0 == sym) {
        diag::error("Found a null symbol.");
        return 0;
    }

    if(0U == sym_to_instr.count(sym)) {
        diag::error("Attempting to jump/branch to an unknown label '%s'.", sym->name);
        return 0;
    }

    simple_instr *inst(sym_to_instr[sym]);

    // shouldn't be possible given above checks
    if(0U == instr_to_block.count(inst)) {
        diag::error("The label '%s' is not a leader for any basic block.", sym->name);
        return 0;
    }

    return instr_to_block[inst];
}

/// (re)compute the successor/predecessor relationship in the control-flow
/// graph.
void cfg::relink(void) throw() {

    std::map<simple_sym *, simple_instr *> labels;
    find_labels(instr_list, labels);
    std::map<simple_instr *, basic_block *> block_by_leader;

    // clear out all basic blocks and find the block leaders
    for(basic_block *bb(entry_); 0 != bb; bb = bb->next) {
        bb->successors_.clear();
        bb->predecessors_.clear();

        if(0 != bb->first) {
            block_by_leader[bb->first] = bb;
        }
    }

    connect_bbs(entry_, entry_->next);

    for(basic_block *bb(entry_->next); 0 != bb; bb = bb->next) {
        if(0U == bb->num_instructions) {
            continue;
        }

        simple_instr *last(bb->last);
        switch(last->opcode) {
        case BTRUE_OP:
        case BFALSE_OP:
            connect_bbs(bb, bb->next);
            // fall-through
        case JMP_OP:
            connect_bbs(bb, lookup_and_use_label(
                last->u.bj.target,
                labels,
                block_by_leader
            ));
            break;

        case MBR_OP:
            connect_bbs(bb, lookup_and_use_label(
                last->u.mbr.deflab,
                labels,
                block_by_leader
            ));

            for(unsigned i(0); i < last->u.mbr.ntargets; ++i) {
                connect_bbs(bb, lookup_and_use_label(
                    last->u.mbr.targets[i],
                    labels,
                    block_by_leader
                ));
            }
            break;

        // connect returns to the exit node
        case RET_OP:
            connect_bbs(bb, exit_);
            break;

        default: // fall-through
            connect_bbs(bb, bb->next);
            break;
        }
    }

    // find the transitive closure of the entry and exit nodes
    find_closure(*this);
}

/// initialize the control-flow graph with a sequence of instructions. this
/// will create the basic blocks as it goes.
///
/// First, all labels are found. Then, all basic blocks are found. Finally,
/// the successor/predecessory relation is filled out by looking for fall-
/// throughs, branches, and jumps.
cfg::cfg(simple_instr *instr_list_) throw()
    : entry_(0)
    , exit_(0)
    , last_allocated(0)
    , instr_list(instr_list_)
{
    entry_ = make_bb(0, 0, 0U);

    // no instructions for this procedure; keep us consistent
    if(0 == instr_list) {
        exit_ = make_bb(0, 0, 0U);
        entry_->successors_.insert(exit_);
        exit_->predecessors_.insert(entry_);
        return;
    }

    // get all basic blocks
    for(simple_instr *begin(instr_list); 0 != begin; ) {
        unsigned num_instructions(0);
        simple_instr *end(find_bb_end(begin, num_instructions));
        make_bb(begin, end, num_instructions);
        begin = end->next;
    }

    // add in the exit basic block, and start off the predecessors/successors
    // relation
    exit_ = make_bb(0, 0, 0U);

    relink();
}

/// destroy all basic blocks
cfg::~cfg(void) throw() {
    for(basic_block *bb(entry_), *next_bb(0); 0 != bb; bb = next_bb) {
        next_bb = bb->next;
        delete bb;
    }

    entry_ = 0;
    exit_ = 0;
    last_allocated = 0;
}

/// make a basic block and automatically assign that block a unique id
basic_block *cfg::make_bb(
    simple_instr *first,
    simple_instr *last,
    unsigned num
) throw() {

    // ensure that every basic block begins with a label; convenient
    // normalization
    if(0 == first || LABEL_OP != first->opcode) {
        simple_instr *new_first(new_instr(LABEL_OP, 0));
        new_first->u.label.lab = new_label();
        instr::insert_before(new_first, first);

        if(0 == first) {
            assert(0 == last);
            last = new_first;
        }

        first = new_first;
        ++num;
    }

    basic_block *bb(new basic_block(num, first, last));

    if(0 != last_allocated) {
        last_allocated->next = bb;
    }

    bb->prev = last_allocated;
    last_allocated = bb;
    bb->next = 0;

    return bb;
}

/// inject the basic block into the stream after prev and before next
static void unsafe_inject_bb(
    basic_block *prev,
    basic_block *curr,
    basic_block *next
) throw() {
    // connect instructions and basic blocks

    curr->next = next;
    curr->prev = prev;

    curr->first->prev = 0;
    curr->last->next = 0;

    if(0 != next) {
        assert(next->prev == prev);

        next->prev = curr;

        if(0 != next->first) {
            curr->last->next = next->first;
            if(0 != next->first) {
                next->first->prev = curr->last;
            }
        }
    }

    if(0 != prev) {
        assert(prev->next == next);

        prev->next = curr;

        if(0 != prev->last) {
            prev->last->next = curr->first;
        }

        curr->first->prev = prev->last;
    }
}

/// create and inject a basic block into the control-flow graph between two
/// other basic blocks
///
/// !!! this does not update the predecessor/successor relations.
basic_block *cfg::unsafe_insert_block(
    basic_block *prev,
    basic_block *next,
    simple_instr *first,
    simple_instr *last
) throw() {

    assert(0 != first);
    assert(0 != last);

    // figure out how many instructions this bb has
    unsigned num_instrs(0U);
    simple_instr *last_(find_bb_end(first, num_instrs));

    if(last_ != last) {
        diag::error(
            "Attempting to insert a block before block failed "
            "because the specified last instruction does not end "
            "a basic block beginning with the first instruction.\n"
        );
        return 0;
    }

    // ensure that every basic block begins with a label; convenient
    // normalization
    if(LABEL_OP != first->opcode) {
        simple_instr *new_first(new_instr(LABEL_OP, 0));
        new_first->u.label.lab = new_label();
        instr::insert_before(new_first, first);

        if(0 == first) {
            assert(0 == last);
            last = new_first;
        }

        first = new_first;
        ++num_instrs;
    }

    basic_block *curr(new basic_block(num_instrs, first, last));

    unsafe_inject_bb(prev, curr, next);

    return curr;
}

/// return an iterator to the first basic block
basic_block_iterator cfg::begin(void) throw() {
    return basic_block_iterator(entry_);
}

/// return an iterator to a non-existant basic block
const basic_block_iterator cfg::end(void) const throw() {
    return basic_block_iterator(0);
}

basic_block *cfg::entry(void) const throw() {
    return entry_;
}

basic_block *cfg::exit(void) const throw() {
    return exit_;
}

bool cfg::for_each_basic_block(bool (*callback)(basic_block *)) {
    for(basic_block *bb(entry_); 0 != bb; bb = bb->next) {
        if(!callback(bb)) {
            return false;
        }
    }
    return true;
}

