/*
 * licm.cc
 *
 *  Created on: Mar 29, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

extern "C" {
#   include <simple.h>
}

#include <cassert>
#include <vector>
#include <set>
#include <map>
#include <cstdlib>

#include "include/optimizer.h"
#include "include/cfg.h"
#include "include/loop.h"
#include "include/instr.h"

#include "include/use_def.h"

#include "include/opt/licm.h"
#include "include/opt/eval.h"

/// go find the exit nodes of the loop
static void find_loop_exit_bbs(
    loop &loop,
    std::vector<basic_block *> &loop_exits
) throw() {
    std::set<basic_block *>::const_iterator it(loop.body.begin())
                                          , end(loop.body.end());

    for(; it != end; ++it) {
        const basic_block *bb(*it);

        if(0 == bb->last) {
            continue;
        }

        switch(bb->last->opcode) {

        // we only care if one of their successors is not in the loop's body
        case BTRUE_OP: case BFALSE_OP: case MBR_OP: {
            const std::set<basic_block *> &succ(bb->successors());
            std::set<basic_block *>::const_iterator succ_it(succ.begin())
                                                  , succ_end(succ.end());

            for(; succ_it != succ_end; ++succ_it) {
                if(0U == loop.body.count(*succ_it)) {
                    loop_exits.push_back(*succ_it);
                }
            }

            break;
        }

        // note: by definition, a fall-through or direct jump cannot
        //       be an exit of the loop, as each only has one successor
        default:
            assert(1U == bb->successors().size());
            break;
        }
    }
}

/// imposes a total ordering on loops; this takes advantage of the fact that
/// nested loops must strictly have fewer basic blocks than their enclosing
/// loops
struct loop_less {
public:
    bool operator()(const loop *a, const loop *b) const throw() {
        if(a == b) {
            return false;
        }

        const size_t a_size(a->body.size() + a->tails.size());
        const size_t b_size(b->body.size() + b->tails.size());

        if(a_size < b_size) {
            return true;
        } else if(b_size < a_size) {
            return false;
        } else {
            return a < b;
        }
    }
};

/// order the loops so that we visit nested loops before we visit their
/// enclosing loops; this will make it so that hopefully stuff can be hoisted
/// out of multiple loops at once. this is tricky because the loop_map totally
/// orders the loops by their basic block points, but the loops themselves only
/// respect a partial order, and so must be re-ordered. The re-ordered is based
/// on lexicographic order of loop size and loop head pointer.
static bool add_loop_to_set(loop &l, std::set<loop *, loop_less> &ordered_loops) throw() {
    ordered_loops.insert(&l);
    return true;
}
static void order_loops(
    loop_map &loops,
    std::vector<loop *> &ordered_loops
) throw() {
    const unsigned num_loops(static_cast<unsigned>(loops.size()));
    ordered_loops.reserve(num_loops);
    std::set<loop *, loop_less> loop_set;
    loops.for_each_loop(add_loop_to_set, loop_set);
    ordered_loops.insert(ordered_loops.begin(),
        loop_set.begin(),
        loop_set.end()
    );
}

/// apply a function to each basic blook in a loop
template <typename T0>
static void for_each_basic_block(
    loop &ll,
    void (*func)(basic_block *, T0 &),
    T0 &t0
) throw() {
    std::set<basic_block *>::iterator it(ll.body.begin())
                                    , end(ll.body.end());

    for(; it != end; ++it) {
        func(*it, t0);
    }
}

/// type used for counting defs/uses of variables
struct variable_counter {
public:
    std::map<simple_reg *, unsigned> *counter;
    bool (*count_what)(
        void (*)(
            simple_reg *,
            simple_reg **,
            simple_instr *,
            std::map<simple_reg *, unsigned> &
        ),
        simple_instr *,
        std::map<simple_reg *, unsigned> &
    );
    void (*do_counting_func)(
        simple_reg *,
        simple_reg **,
        simple_instr *,
        std::map<simple_reg *, unsigned> &
    );
};

/// increment the counter for a variable
static void increment_var_counter(
    simple_reg *var,
    simple_reg **,
    simple_instr *,
    std::map<simple_reg *, unsigned> &counter
) throw() {
    counter[var] += 1U;
}
static void initialize_var_counter(
    simple_reg *var,
    simple_reg **,
    simple_instr *,
    std::map<simple_reg *, unsigned> &counter
) throw() {
    counter[var] = 0U;
}

/// count the variables
static void count_variables(basic_block *bb, variable_counter &counter) throw() {
    if(0 == bb->last) {
        return;
    }

    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
        counter.count_what(counter.do_counting_func, in, *(counter.counter));
    }
}

struct invariant_instr {
public:
    simple_instr *in;
    basic_block *bb;
    simple_reg *reg; // variable defined

    bool operator<(const invariant_instr &that) const throw() {
        return in < that.in;
    }
};

/// type used for keeping track of invariant instructions
struct invariant_tracker {
public:
    std::set<invariant_instr> *invariant_ins;
    std::vector<invariant_instr> *ordered_iins;

    std::map<simple_reg *, unsigned> *num_defs;
    std::set<simple_reg *> *invariant_regs;
    std::set<basic_block *> *dominating_blocks;
    std::vector<basic_block *> *exit_blocks;

    std::map<simple_reg *, simple_reg *> *temp_reg_remap;

    dominator_map *doms;
    bool instruction_is_invariant;
};

static void check_used_var_invariant(
    simple_reg *var,
    simple_reg **,
    simple_instr *,
    invariant_tracker &it
) throw() {
    if(!it.invariant_regs->count(var)) {
        it.instruction_is_invariant = false;
    }
}
/*
static const char *r(simple_reg *reg) throw() {
    char *str(new char[10]);
    sprintf(str, "%c%d", reg->kind == PSEUDO_REG ? 'r' : 't', reg->num);
    return str;
}*/

static void find_invariant_ins(basic_block *bb, invariant_tracker &it) throw() {
    if(0 == bb->last) {
        return;
    }

    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {

        invariant_instr iin;
        iin.bb = bb;
        iin.in = in;

        // already marked or it's a function call; continue. note: function call
        // is possibly a var def, but functions can have side-effects so we want
        // to avoid them
        if(it.invariant_ins->count(iin)
        || CALL_OP == in->opcode) {
            continue;
        }

        // this is a variable definition
        simple_reg *defd_var(0);
        if(for_each_var_def(in, defd_var)) {

            iin.reg = defd_var;

            // it's defined multiple times in the loop; ignore
            const unsigned num_defs((*(it.num_defs))[defd_var]);
            if(1U < num_defs) {
                continue;
            }

            // if we're loading from memory, that might mean there's a store
            // elsewhere that could potentially affect this
            if(LOAD_OP == in->opcode) {
                continue;
            }

            // no arguments used; need to watch out, e.g. if we're doing a
            // non-constant load
            it.instruction_is_invariant = true;
            if(for_each_var_use(check_used_var_invariant, in, it)) {

                // not all used vars are invariant
                if(!it.instruction_is_invariant) {
                    continue;
                }
            }

            // note: later on we will check that the definition dominates all
            //       its uses and dominates all exits

            // by now this is an invariant definition
            it.invariant_regs->insert(defd_var);
            it.invariant_ins->insert(iin);
        }
    }
}

/// for each basic block (bb; in the loop), go over all exit blocks (exit_bb;
/// in the loop) and make sure that bb dominates each exit_bb.
static void find_dominating_block(basic_block *bb, invariant_tracker &it) throw() {
    std::vector<basic_block *> &loop_exits(*(it.exit_blocks));
    const unsigned num_loop_exits(static_cast<unsigned>(loop_exits.size()));

    for(unsigned i(0U); i < num_loop_exits; ++i) {
        basic_block *exit_bb(loop_exits[i]);
        dominator_set &exit_doms((*(it.doms))(exit_bb));

        // there exists at least one exit block that bb doesn't dominate
        if(!exit_doms.count(bb)) {
            return;
        }
    }

    // yay, this block dominates all exits
    it.dominating_blocks->insert(bb);
}

/// remove instructions marked as invariant whose blocks don't dominate all
/// the loop exits
static void keep_dominating_ins(invariant_tracker &it) throw() {
    std::set<invariant_instr> keep_set;
    std::set<invariant_instr>::iterator iin_it(it.invariant_ins->begin())
                                      , iin_end(it.invariant_ins->end());

    for(; iin_it != iin_end; ++iin_it) {
        if(it.dominating_blocks->count(iin_it->bb)) {
            keep_set.insert(*iin_it);
        } else {
            it.invariant_regs->erase(iin_it->reg);
        }
    }

    // note: this is necessary but not sufficient; there might be instructions
    //       that depend on the variables defined by instructions removed in this
    //       step that haven't been removed yet, but need to be removed.

    it.invariant_ins->swap(keep_set);
}

/// remove instructions marked as invariant that don't dominate their uses
static void keep_defs_dominating_uses(
    invariant_tracker &it,
    def_use_map &dum,
    dominator_map &dom,
    std::set<basic_block *> &loop_body
) throw() {
    std::set<invariant_instr> keep_set;
    std::set<invariant_instr>::iterator iin_it(it.invariant_ins->begin())
                                      , iin_end(it.invariant_ins->end());

    for(; iin_it != iin_end; ++iin_it) {
        basic_block *iin_bb(iin_it->bb);
        simple_instr *iin(iin_it->in);
        const var_use_set &uses(dum(iin));
        var_use_set::const_iterator u_it(uses.begin()), u_end(uses.end());

        bool keep(true);
        for(; u_it != u_end; ++u_it) {

            // there is a use of a variable in the same block as it is defined;
            // need to check through the next instructions to make sure we don't
            // find in_bb
            if(u_it->bb == iin_bb) {

                // walk to the end of the block; make sure we *don't* find the
                // def
                simple_instr *sin(u_it->in);
                for(simple_instr *end(iin_bb->last->next);
                    sin != end && sin != iin;
                    sin = sin->next) {
                    assert(0 != sin);
                }

                if(sin == iin) {
                    keep = false;
                    break;
                }

            // there is a use of a variable outside of the block in which it's
            // defined; make sure the def's block dominates the use's block iff
            // the use's block is inside the loop
            } else if(loop_body.count(u_it->bb)
                   && 0U == dom(u_it->bb).count(iin_bb)) {
                keep = false;
                break;
            }
        }

        if(keep) {
            keep_set.insert(*iin_it);
        }
    }

    it.invariant_ins->swap(keep_set);
}

/// remove instructions marked as invariant that don't dominate the exit block
static void keep_defs_dominating_exit(
    invariant_tracker &it,
    dominator_set &exit_doms
) throw() {
    std::set<invariant_instr> keep_set;
    std::set<invariant_instr>::iterator iin_it(it.invariant_ins->begin())
                                      , iin_end(it.invariant_ins->end());

    for(; iin_it != iin_end; ++iin_it) {
        if(exit_doms.count(iin_it->bb)) {
            keep_set.insert(*iin_it);
        }
    }

    it.invariant_ins->swap(keep_set);
}

/// keep instructions marked as invariant whose used variables are also still
/// seen as invariant
static void remove_disproved_invariant_ins(invariant_tracker &it) throw() {
    std::set<invariant_instr> keep_set;
    std::set<invariant_instr>::iterator iin_it(it.invariant_ins->begin())
                                      , iin_end(it.invariant_ins->end());

    for(; iin_it != iin_end; ++iin_it) {
        simple_instr *iin(iin_it->in);

        // no arguments used; need to watch out, e.g. if we're doing a
        // non-constant load
        it.instruction_is_invariant = true;
        for_each_var_use(check_used_var_invariant, iin, it);

        // not all used vars are invariant
        if(it.instruction_is_invariant) {
            keep_set.insert(*iin_it);
        }
    }

    it.invariant_ins->swap(keep_set);
}

/// order the invariant instructions in depth-first search order
static void order_iins_dfs(
    invariant_tracker &it,
    std::set<basic_block *> &seen,
    const std::set<basic_block *> &loop_body,
    basic_block *curr
) {
    if(seen.count(curr)) {
        return;
    }

    seen.insert(curr);

    // go over each instruction; if any are invariant, then take them
    // and put them into the ordered list
    for(simple_instr *in(curr->first); in != curr->last->next; in = in->next) {
        assert(0 != in);

        invariant_instr iin;
        iin.bb = curr;
        iin.in = in;
        iin.reg = 0;
        std::set<invariant_instr>::iterator iin_it(it.invariant_ins->find(iin));

        if(it.invariant_ins->end() != iin_it) {
            it.ordered_iins->push_back(*iin_it);
        }
    }

    // handle all successors
    const std::set<basic_block *> &succs(curr->successors());
    std::set<basic_block *>::const_iterator succ_it(succs.begin())
                                          , succ_end(succs.end());

    for(; succ_it != succ_end; ++succ_it) {
        basic_block *bb(*succ_it);

        // make sure we never re-visit a basic block, and only stay within the
        // loop
        if(!loop_body.count(bb) || seen.count(bb)) {
            continue;
        }

        // descend further
        order_iins_dfs(it, seen, loop_body, bb);
    }
}

/// replace a temporary register with a re-mapped version of itself
static void replace_temp_reg(
    simple_reg *reg,
    simple_reg **pos,
    simple_instr *,
    std::map<simple_reg *, simple_reg *> &temp_reg_remap
) throw() {
    if(temp_reg_remap.count(reg)) {
        *pos = temp_reg_remap[reg];
    }
}

/// propagate a remapped register to an instruction
static void propagate_remapped_temp_reg(
    simple_reg *reg,
    simple_reg **pos,
    simple_instr *,
    std::map<simple_reg *, simple_reg *> &temp_reg_remap
) throw() {
    simple_reg *remapped_reg(temp_reg_remap[reg]);
    if(0 != remapped_reg
    && PSEUDO_REG == remapped_reg->kind) {
        *pos = remapped_reg;
    }
}

/// propagate all remapped registers to all instructions in a basic block
static void propagate_remapped_temp_regs(
    basic_block *bb,
    std::map<simple_reg *, simple_reg *> &temp_reg_remap
) throw() {
    for(simple_instr *in(bb->first); in != bb->last->next; in = in->next) {
        assert(0 != in);

        for_each_var_use(
            propagate_remapped_temp_reg,
            in,
            temp_reg_remap
        );
    }
}

/// move (by copying) instructions up into a loop's pre-header; this is tricky
/// because we need to make sure the instructions remain in the correct order
void copy_instructions_to_preheader(
    invariant_tracker &it,
    basic_block *pre_header,
    const std::set<basic_block *> &loop_body
) throw() {

    std::set<basic_block *> seen;
    std::vector<invariant_instr> ordered_iins;

    const size_t num_iins(it.invariant_ins->size());

    ordered_iins.reserve(num_iins);
    it.ordered_iins = &ordered_iins;

    order_iins_dfs(it, seen, loop_body, pre_header);
    //printf("/* e: num ordered iins = %lu */\n", ordered_iins.size());
    assert(ordered_iins.size() == num_iins);

    // do an in-order copy; only need to modify temp vars
    for(size_t i(0U); i < num_iins; ++i) {
        invariant_instr &iin(ordered_iins[i]);

        // re-map a register
        if(TEMP_REG == iin.reg->kind) {
            simple_reg *new_var(new_register(
                iin.reg->var->type,
                LDC_OP == iin.in->opcode ? TEMP_REG : PSEUDO_REG
            ));
            (*it.temp_reg_remap)[iin.reg] = new_var;
        }

        // copy the instruction
        simple_instr *copy(new_instr(NOP_OP, 0));
        memcpy(copy, iin.in, sizeof *copy);
        copy->prev = copy->next = 0;

        // replace temp registers in the copy
        for_each_var_def(replace_temp_reg, copy, *it.temp_reg_remap);
        for_each_var_use(replace_temp_reg, copy, *it.temp_reg_remap);

        // put it in place at the end of the preheader
        instr::insert_after(copy, pre_header->last);
        pre_header->last = copy;
    }
}

/// try to find the longest "straight line" of execution
static simple_instr *straight_line_begin(basic_block *bb) throw() {
    simple_instr *begin(0);

    for(; ;) {
        begin = bb->first;

        if(bb->predecessors().empty()) {
            break;

        } else if(1U < bb->predecessors().size()) {
            break;
        } else {
            basic_block *prev_bb(*(bb->predecessors().begin()));

            // the successor doesn't flow directly to bb
            if(1U < prev_bb->successors().size()) {
                break;
            }

            bb = prev_bb;
        }
    }

    return begin;
}

/// try to find the first condition of the loop that causes the loop to exit;
/// in this case, we only work with idealized for loop-like things generated by
/// C and defer a slightly less robust version of moving things in the event
/// that nothing can be proved
static basic_block *find_first_branch_bb(
    loop &loop,
    size_t &num_seen_blocks
) throw() {
    std::set<basic_block *> seen_blocks;
    basic_block *bb(loop.head);

    num_seen_blocks = seen_blocks.size();

    for(;;) {
        assert(0 != bb->last);

        seen_blocks.insert(bb);

        switch(bb->last->opcode) {

        // multiple successors
        case MBR_OP: case BFALSE_OP: case BTRUE_OP:
            num_seen_blocks = seen_blocks.size();
            return bb;

        // only one successor
        default:

            // infinite loop detected because the block we just reached hasn't
            // grown the set of seen blocks
            if(num_seen_blocks == seen_blocks.size()) {
                num_seen_blocks = loop.body.size();
                return 0;
            }

            num_seen_blocks = seen_blocks.size();
            bb = *(bb->successors().begin());
            continue;
        }
    }

    return bb;
}

/// try to prove that a loop will execute its body
static bool try_prove_loop_will_run(optimizer &o, loop &loop) throw() {

    // try to prove that this loop will execute
    bool will_provably_run(false);

    // case 1: self-loop
    if(1U == loop.body.size()) {
        return true;
    }

    // not a self-loop; look to see if the first branch block has a successor
    // that exits the loop,

    size_t num_non_exit_bbs(0U);
    basic_block *first_branch_block(find_first_branch_bb(loop, num_non_exit_bbs));

    // all blocks in the loop can be executed
    if(num_non_exit_bbs == loop.body.size()) {
        return true;

    } else if(0 == first_branch_block) {
        return false; // shouldn't happen
    }

    simple_instr *breakpoint(first_branch_block->last);

    // MBR's are too tricky to follow
    if(MBR_OP == breakpoint->opcode) {
        return false;
    }

    std::set<basic_block *>::const_iterator succs(first_branch_block->successors().begin());
    basic_block *bb1(*succs++);
    basic_block *bb2(*succs);

    // hrmm, both in the loop; let's bail out, this loop is weirdly
    // structured
    if(loop.body.count(bb1) && loop.body.count(bb2)) {
        return false;

    // find the next instruction inside the loop
    } else if(loop.body.count(bb1)) {
        breakpoint = bb1->first;
    } else {
        breakpoint = bb2->first;
    }

    if(0 == breakpoint) {
        return false; // shouldn't happen
    }

    // so why should this test be sufficient? the only instructions
    // that can be invariant are those that dominate all exits. thus
    // we focus on the first easily reachable exit. If we can reach
    // the branch that does the exit, and move past it (thus stay
    // in the loop), that we can know that *every* invariant
    // instruction will be okay to move because they all dominate
    // this particular exit

    // try to reach the breakpoint by symbollically evaluating
    // a prefix of blocks to the loop up until the breakpoint
    return eval::REACHED_BREAKPOINT == abstract_evaluator_bp(
        o,
        straight_line_begin(loop.head),
        breakpoint
    );
}

/// hoist code out of an individual loop
static bool hoist_code(optimizer &o, cfg &flow, def_use_map &dum, dominator_map &dm, loop &loop) throw() {

    // get all exits of the loop; we need to make sure the definitions of variables
    // dominate the exits
    std::vector<basic_block *> loop_exits;
    find_loop_exit_bbs(loop, loop_exits);

    // go identify how many definitions of each variable there are
    variable_counter counter;
    std::map<simple_reg *, unsigned> num_defs;
    counter.counter = &num_defs;
    counter.count_what = for_each_var_use;
    counter.do_counting_func = initialize_var_counter;
    for_each_basic_block(loop, count_variables, counter);
    counter.count_what = for_each_var_def;
    counter.do_counting_func = increment_var_counter;
    for_each_basic_block(loop, count_variables, counter);

    // go identify invariant instructions; this will try to grow the set of
    // invariant instructions until we can't
    std::set<invariant_instr> invariant_ins;
    std::set<simple_reg *> invariant_regs;
    invariant_tracker it;
    it.num_defs = &num_defs;
    //it.num_uses = &num_uses;
    it.doms = &dm;
    it.invariant_ins = &invariant_ins;
    it.invariant_regs = &invariant_regs;
    size_t old_num_ins(0U), old_num_regs(0U);

    // any variable that isn't defined inside the loop is considered
    // loop invariant
    std::map<simple_reg *, unsigned>::iterator zd_it(num_defs.begin())
                                             , zd_end(num_defs.end());
    for(; zd_it != zd_end; ++zd_it) {
        if(0U == zd_it->second) {
            //printf("/* %s is invariant on entering */\n", r(zd_it->first));
            invariant_regs.insert(zd_it->first);
        }
    }

    // loop until we can't find any new invariant instructions or registers
    do {
        old_num_ins = invariant_ins.size();
        old_num_regs = invariant_regs.size();
        for_each_basic_block(loop, find_invariant_ins, it);
    } while(old_num_ins < invariant_ins.size()
         || old_num_regs < invariant_regs.size());

    if(invariant_ins.empty()) {
        return false;
    }

    // keep only those instructions that dominate the exits
    std::set<basic_block *> dominating_blocks;
    it.exit_blocks = &loop_exits;
    it.dominating_blocks = &dominating_blocks;

    for_each_basic_block(loop, find_dominating_block, it);
    keep_dominating_ins(it);
    if(invariant_ins.empty()) {
        return false;
    }

    // keep only those instructions that dominate their uses within
    // the loop; this makes it impossible for an invariant variable that would
    // cause the loop to not be executed to be moved outside of the loop
    keep_defs_dominating_uses(it, dum, dm, loop.body);
    if(invariant_ins.empty()) {
        return false;
    }

    // if we can't prove that this loop will always do at least one iteration
    // then kill any invariant instruction that:
    //  a) doesn't dominate the exit of the CFG; or
    //  b) has no uses outside after the loop
    if(!try_prove_loop_will_run(o, loop)) {
        // case a)
        keep_defs_dominating_exit(it, dm(flow.exit()));

        // case b) too lazy to handle :-P

        if(invariant_ins.empty()) {
            return false;
        }
    }

    // clear out all instructions that use variables now no longer marked
    // as invariant
    remove_disproved_invariant_ins(it);
    if(invariant_ins.empty()) {
        return false;
    }

    // copy the instructions up into the pre-header
    o.changed_def();
    o.changed_use();
    std::map<simple_reg *, simple_reg *> temp_reg_remap;
    it.temp_reg_remap = &temp_reg_remap;
    copy_instructions_to_preheader(it, loop.pre_header, loop.body);

    // clear (to a NOP) all instructions that don't define temporary
    // variables
    std::set<invariant_instr>::iterator iin_it(invariant_ins.begin())
                                      , iin_end(invariant_ins.end());

    for(; iin_it != iin_end; ++iin_it) {

        // this defined a pseudo reg, so it must have been moved and can easily
        // be nulled
        if(TEMP_REG != iin_it->reg->kind) {
            iin_it->in->opcode = NOP_OP;

        // defines a temp reg
        } else {
            simple_reg *remaped_reg(temp_reg_remap[iin_it->reg]);

            // this was an instruction that defines a temp var that now should
            // be propagated
            if(0 != remaped_reg && PSEUDO_REG == remaped_reg->kind) {
                iin_it->in->opcode = NOP_OP;

            // this instruction might use a variable that should be propagated
            } else {
                for_each_var_use(
                    propagate_remapped_temp_reg,
                    iin_it->in,
                    *it.temp_reg_remap
                );
            }
        }
    }

    for_each_basic_block(
        loop,
        propagate_remapped_temp_regs,
        *it.temp_reg_remap
    );

    return true;
}

/// hoist loop-invariant code out of all loops in a CFG
void hoist_loop_invariant_code(optimizer &o, cfg &flow, loop_map &lm) throw() {
    if(0 != getenv("ECE540_DISABLE_LICM")) {
        return;
    }

    std::vector<loop *> loops;
    order_loops(lm, loops);

    for(unsigned i(0U); i < loops.size(); ++i) {
        loop *ll(loops[i]);
    }

    dominator_map &dm(o.force_get<dominator_map>());
    bool updated(false);
    for(unsigned i(0U); i < loops.size(); ++i) {
        def_use_map &dum(o.force_get<def_use_map>());
        if(hoist_code(o, flow, dum, dm, *(loops[i]))) {
            updated = true;
        }
    }

    if(updated) {
        o.changed_block();
    }
}
