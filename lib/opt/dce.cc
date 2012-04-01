/*
 * dce.cc
 *
 *  Created on: Mar 23, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <cassert>
#include <vector>

#include "include/opt/dce.h"
#include "include/cfg.h"
#include "include/optimizer.h"

/// a work item in our DCE work list
struct dce_work_item {
public:
    basic_block *bb;
    simple_instr *in;

    dce_work_item(basic_block *bb_, simple_instr *in_) throw()
        : bb(bb_)
        , in(in_)
    { }
};

/// NOP out all unreachable basic blocks
static bool clear_unreachable_bbs(basic_block *bb, optimizer &o) throw() {
    if(bb->entry_reachable || 0 == bb->first) {
        return true;
    }

    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
        in->opcode = NOP_OP;
    }

    o.changed_block();
    return true;
}

/// kill useless JMPs; after a few cycles through the pipeline, this might
/// give us the opportunity to merge basic blocks. This is actually a form of
/// peephole optimization
static void kill_jmps(simple_instr *in, optimizer &o) throw() {
    for(simple_instr *next(0); 0 != in; in = next) {
        next = in->next;
        if(0 != next
        && JMP_OP == in->opcode
        && LABEL_OP == next->opcode
        && in->u.bj.target == next->u.label.lab) {
            o.changed_block();
            in->opcode = NOP_OP;
        }
    }
}

/// kill all NOPs
static void kill_nops(simple_instr *in, optimizer &o) throw() {

    for(simple_instr *next(0); 0 != in; in = next) {
        if(NOP_OP == in->opcode) {

            // find the next non-NOP
            for(next = in->next;
                0 != next && NOP_OP == next->opcode;
                next = next->next) {
                // loop :-P
            }

            // unlink, if possible
            if(0 != in->prev) {
                simple_instr *prev(in->prev);
                prev->next = next;

                if(0 != next) {
                    next->prev = prev;
                }
            }
        } else {
            next = in->next;
        }
    }
}

typedef std::vector<dce_work_item> dce_work_list;

/// go find all instructions that allow variables to escape the function and
/// consider those instructions to be essential
static bool find_initial_essential_ins(basic_block *bb, dce_work_list &wl) throw() {
    if(0 == bb->first) {
        return true;
    }

    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
        switch(in->opcode) {
        case RET_OP: case CALL_OP: case STR_OP: case MCPY_OP:
        case LOAD_OP:
            wl.push_back(dce_work_item(bb, in));
        default:
            break;
        }
    }

    return true;
}

/// go find each label instruction
static bool find_label_essential_ins(
    basic_block *bb,
    std::set<simple_instr *> &essential_ins
) throw() {
    if(0 == bb->first) {
        return true;
    }

    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
        if(LABEL_OP == in->opcode) {
            essential_ins.insert(in);
        }
    }
    return true;
}

/// turn every non-essential instruction into a NOP for later cleaning up
static bool clear_non_essential_ins(
    basic_block *bb,
    std::set<simple_instr *> &essential_ins
) throw() {
    if(0 == bb->first) {
        return true;
    }

    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
        if(NOP_OP != in->opcode
        && 0U == essential_ins.count(in)) {
            in->opcode = NOP_OP;
        }
    }

    return true;
}

/// eliminate dead code
static void do_dce(
    optimizer &o,
    cfg &flow,
    use_def_map &ud
) throw() {
    std::set<simple_instr *> essential_ins;
    std::set<basic_block *> control_dep;
    dce_work_list work_list;

    // initialize the work list
    flow.for_each_basic_block(&find_initial_essential_ins, work_list);

    // initialize essential instructions with all labels; unused labels will
    // be destroyed the next time the cfg is initialized
    flow.for_each_basic_block(&find_label_essential_ins, essential_ins);

    // identify and remove deadcode
    for(bool updated(true); updated; ) {
        updated = false;
        control_dep.clear();

        while(!work_list.empty()) {
            dce_work_item item(work_list.back());
            work_list.pop_back();

            if(0 != essential_ins.count(item.in)) {
                continue;

            // add to the set of blocks that we will need to check for control
            // dependencies
            } else {
                const std::set<basic_block *> &preds(item.bb->predecessors());
                control_dep.insert(preds.begin(), preds.end());
            }

            essential_ins.insert(item.in);

            // all defs that reach this instruction are essential
            const var_def_set &rd(ud(item.in));
            var_def_set::const_iterator rd_it(rd.begin()), rd_end(rd.end());
            for(; rd_it != rd_end; ++rd_it) {
                work_list.push_back(dce_work_item(rd_it->bb, rd_it->in));
            }
        }

        // find control dependent instructions; only change updated in this loop
        // as work_list is known to be empty before here
        std::set<basic_block *> all_control_dep;
        std::set<basic_block *> next_control_dep;
        for(bool updated_control_dep(true); updated_control_dep; ) {
            updated_control_dep = false;

            std::set<basic_block *>::iterator cit(control_dep.begin())
                                            , cend(control_dep.end());

            for(; cit != cend; ++cit) {
                basic_block *bb(*cit);

                // entry/exit block, or already found this block as being
                // control dependeny
                if(0 == bb->last || 0 != all_control_dep.count(bb)) {
                    continue;
                }

                // track which basic blocks have been visited so we don't loop
                // infinitely in the case of cyclic cascading control
                // dependencies
                all_control_dep.insert(bb);

                switch(bb->last->opcode) {

                    // direct control dependencies
                    case MBR_OP: case BTRUE_OP: case BFALSE_OP: case JMP_OP: {
                        updated = true;
                        work_list.push_back(dce_work_item(bb, bb->last));
                        break;
                    }

                    // handle cascading control dependencies through fall-throughs
                    default: {
                        updated_control_dep = true;
                        const std::set<basic_block *> &preds(bb->predecessors());
                        next_control_dep.insert(preds.begin(), preds.end());
                        break;
                    }
                }
            }

            control_dep.swap(next_control_dep);
            next_control_dep.clear();

        }
    }

    // clear (to NOPs) every non-essential instruction
    flow.for_each_basic_block(&clear_non_essential_ins, essential_ins);
}

/// determine essential instructions and convert non-essential instructions
/// into NOPs. Do a final pass over the instructions to clear out NOPs.
void eliminate_dead_code(optimizer &o, cfg &flow, use_def_map &ud) throw() {
    //printf("running dce\n");
#ifndef ECE540_DISABLE_DCE

    // clear out all unreachable blocks; they will be cleaned up later by the
    // NOP killing pass
    flow.for_each_basic_block(&clear_unreachable_bbs, o);

    do_dce(o, flow, ud);

    // clean up useless things
    kill_jmps(o.first_instruction(), o);
    kill_nops(o.first_instruction(), o);

#endif
}
