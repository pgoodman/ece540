/*
 * loop.cc
 *
 *  Created on: Jan 28, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include <map>
#include <cassert>

#include "include/loop.h"
#include "include/set.h"
#include "include/instr.h"
#include "include/cfg.h"

loop_bounds_type::loop_bounds_type(void) throw()
    : tail(0)
    , head(0)
{ }

loop_bounds_type::loop_bounds_type(basic_block *tail_, basic_block *head_) throw()
    : tail(tail_)
    , head(head_)
{}

/// go find all back edges in a loop; this works by intersection the set of
/// successors of a basic block with its set of dominators. If a back edge
/// exists then the intersection is non-empty.
static void get_back_edges(
    cfg &flow_graph,
    dominator_map &dominators,
    std::set<loop_bounds_type> &back_edges
) throw() {
    basic_block_iterator blocks_it(flow_graph.begin());
    const basic_block_iterator blocks_end(flow_graph.end());
    std::set<basic_block *>::iterator header_it, header_end;

    for(; blocks_it != blocks_end; ++blocks_it) {

        basic_block *tail(*blocks_it);
        std::set<basic_block *> headers(set_intersection(
            tail->successors(),
            dominators(tail)
        ));

        header_it = headers.begin();
        header_end = headers.end();

        for(; header_it != header_end; ++header_it) {
            loop_bounds_type back_edge(tail, *header_it);
            back_edges.insert(back_edge);
        }
    }
}

/// get the set of all blocks in a loop; if an entry point into the loop is
/// found that does not go throug the loop header, (i.e. some block in the
/// loop body is not dominated by the loop header) then false is returned,
/// with the meaning that the (head, tail) pair are invalid loop bounds.
static bool get_loop_body(
    dominator_map &dominators,
    basic_block *head,
    basic_block *tail,
    std::set<basic_block *> &body
) throw() {
    body.insert(head);
    std::vector<basic_block *> stack;
    stack.push_back(tail);

    for(; !stack.empty(); ) {
        basic_block *bb(stack.back());
        stack.pop_back();

        // already found this block as part of the loop
        if(0U != body.count(bb)) {
            continue;
        }

        // invalid loop (head, tail) pair
        if(0U == dominators(bb).count(head)) {
            body.clear();
            return false;
        }

        body.insert(bb);

        // add all predecessors to the stack
        stack.insert(stack.end(),
            bb->predecessors().begin(), bb->predecessors().end()
        );
    }

    return true;
}

// before anything, lets look to see if adding in the pre-header will
// change the behavior of the code. E.g. we have a fall through case, where
// the back edge of the loop goes forward in the instruction stream, by
// means of a fall-through.
static basic_block *patch_loop_tail(
    cfg &flow_graph,
    basic_block *head,
    basic_block *tail
) throw() {

    if(tail->next != head
    || !instr::can_default_fall_through(tail->last)) {
        return tail;
    }

    simple_sym *head_label(0);

    // annoying case: add a label to the header :(
    if(!instr::is_label(head->first)) {
        head_label = new_label();
        simple_instr *label_inst(new_instr(LABEL_OP, 0));
        label_inst->u.label.lab = head_label;

        instr::insert_before(label_inst, head->first);
        head->first = label_inst;
        ++(head->num_instructions);

    // loop header already begins with a label
    } else {
        head_label = head->first->u.label.lab;
    }

    // pathological case: loop tail ends in a btrue/bfalse, and the default
    // fall-through is the back edge; this is an annoying case because we
    // need to inject a new tail block
    if(instr::is_local_control_flow_transfer(tail->last)) {

        simple_instr *first_instr(0);

        // the default fall-through and branch targets are the same; add a
        // label to our patch block
        if(instr::jumps_to(tail->last, head_label)) {
            first_instr = new_instr(LABEL_OP, 0);
            simple_sym *tail_label(new_label());
            first_instr->u.label.lab = tail_label;
            instr::replace_symbol(tail->last, head_label, tail_label);
        }

        simple_instr *last_instr(new_instr(JMP_OP, 0));
        last_instr->u.bj.target = head_label;
        last_instr->u.bj.src = 0;

        // bounds for our new bb
        if(0 == first_instr) {
            first_instr = last_instr;
        } else {
            first_instr->next = last_instr;
            last_instr->prev = first_instr;
        }

        // new tail!
        tail = flow_graph.unsafe_insert_block(
            tail, head, first_instr, last_instr
        );

    // simple case: basic fall through; add in a jmp to the end of the tail
    // basic block
    } else {

        // add in the jump
        simple_instr *jmp_inst(new_instr(JMP_OP, 0));
        jmp_inst->u.bj.src = 0;
        jmp_inst->u.bj.target = head_label;
        instr::insert_after(jmp_inst, tail->last);
        tail->last = jmp_inst;
        ++(tail->num_instructions);

    }

    return tail;
}

// replace all jumps to the loop header in the instruction stream,
// except for jumps in the last instruction of the loop
static void update_jumps_to_head_label(
    cfg &flow_graph,
    std::set<basic_block *> &ignore_set,
    simple_sym *head_label,
    simple_sym *pre_header_label
) throw() {

    for(basic_block *bb(flow_graph.entry()); 0 != bb; bb = bb->next) {
        if(0 == bb->first) {
            continue;
        }

        // should we ingore the last instruction in this basic block?
        simple_instr *ignore_in_bb(0);
        if(0U != ignore_set.count(bb)) {
            ignore_in_bb = bb->last;
        }

        for(simple_instr *in(bb->first);
            in != bb->last->next;
            in = in->next) {

            if(in == ignore_in_bb
            || 0U == instr::replace_symbol(in, head_label, pre_header_label)) {
                continue;
            }
        }
    }
}

/// add in a loop pre-header
///
/// !!! this will patch the CFG, possibly in the following ways:
///      - re-target som branches/jumps to the pre-header
static void add_pre_header(
    cfg &flow_graph,
    dominator_map &dominators,
    basic_block *head,
    basic_block *tail,
    std::set<basic_block *> &ignore_set
) throw() {

    simple_sym *pre_header_label(new_label());
    simple_instr *label_inst(new_instr(LABEL_OP, 0));
    label_inst->u.label.lab = pre_header_label;

    // add in the loop pre-header
    basic_block *pre_header(flow_graph.unsafe_insert_block(
        head->prev, head, label_inst, label_inst
    ));

    // add in dominators for the pre-header
    std::set<basic_block *> head_doms(dominators(head));
    head_doms.erase(head);
    head_doms.insert(pre_header);
    dominators(pre_header).swap(head_doms);

    // no labels to patch
    if(!instr::is_label(head->first)) {
        return;
    }

    simple_sym *head_label(head->first->u.label.lab);

    update_jumps_to_head_label(
        flow_graph, ignore_set, head_label, pre_header_label
    );
}

/// (re)initialize the loop map by first finding potential loop bounds (back edges)
/// and then trying to fill out the bodies of those loops given their bounds
void find_loops(
    cfg &flow_graph,
    dominator_map &dominators,
    loop_map &lm
) throw() {

    lm.clean_up();

    // maps loop heads to tails; this is used on a basic block granularity
    // instead of instruction granularity so that it remains flexible to late
    // additions to the instruction stream, e.g. in the case that we need to
    // add jmp instructions to loop heads.
    std::map<basic_block *, std::set<basic_block *> > tails;

    // identify all back edges
    std::set<loop_bounds_type> back_edges;
    get_back_edges(flow_graph, dominators, back_edges);

    // go over back edges looking for loop bodies; this discards the loop
    // bodies found as when we start adding in loop pre-headers, these would
    // change loop bodies, so it's simplest to figure out what stuff needs
    // to be ignored when patching the graph, and then adding pre-headers and
    // patching incrementally
    std::set<loop_bounds_type>::iterator back_edge(back_edges.begin());
    for(; back_edge != back_edges.end(); ++back_edge) {

        std::set<basic_block *> body;
        basic_block *tail(back_edge->tail);
        basic_block *head(back_edge->head);

        if(!get_loop_body(dominators, head, tail, body)) {
            continue;
        }

        // keep track of the back-edge instructions for loops; we *don't* want
        // to update their target labels; but we want to update any other
        // instructions that target the loop head to target the pre-header
        tails[head].insert(tail);
    }

    // go add in all pre-headers; need to do this before we re-compute the
    // loop bodies so that the loop bodies properly contain sub-loop pre-
    // headers
    std::set<basic_block *> heads;
    for(back_edge = back_edges.begin(); back_edge != back_edges.end(); ++back_edge) {
        basic_block *tail(back_edge->tail);
        basic_block *head(back_edge->head);

        if(0U == tails.count(head)) {
            continue;
        }

        // make sure to patch loop tails *before* considering if we've already
        // added a pre-header to this loop's head.
        basic_block *new_tail(patch_loop_tail(flow_graph, head, tail));
        std::set<basic_block *> &ignore_set(tails[head]);

        // we injected a new tail in; need to update the back_edges set and
        // also re-position the back_edge iterator
        if(new_tail != tail) {
            back_edges.erase(*back_edge);
            ignore_set.erase(tail);
            ignore_set.insert(new_tail);

            tail = new_tail;

            const loop_bounds_type new_back_edge(tail, head);
            back_edges.insert(new_back_edge);
            back_edge = back_edges.find(new_back_edge);
        }

        // don't add multiple pre-headers to the same head
        if(0U < heads.count(head)) {
            continue;
        }

        add_pre_header(flow_graph, dominators, head, tail, ignore_set);

        heads.insert(head);
        ++lm.num_loops;
    }

    // recompute the successor/predecessors and dominators after adding in
    // pre-headers
    flow_graph.relink();
    find_dominators(flow_graph, dominators);

    lm.loops = new loop[lm.num_loops];
    std::map<basic_block *, loop *> loops_by_head;
    unsigned curr_loop(0U);

    // go collect the loop bodies, now that pre-headers are in; nice invariat:
    // the back edges are already sorted, so we just need to push back to the
    // end of the tails to accumulate the right info for basic blocks with
    // multiple tails
    for(back_edge = back_edges.begin();
        back_edge != back_edges.end();
        ++back_edge) {

        basic_block *tail(back_edge->tail);
        basic_block *head(back_edge->head);

        if(0U == tails.count(head)) {
            continue;
        }

        std::set<basic_block *> body;
        get_loop_body(dominators, head, tail, body);

        loop *loop_info(0);

        // this is already a loop header; merge it in; this loop appears after
        // the other one in the set so its tail is "further away"; keep it.
        if(loops_by_head.count(head)) {
            loop_info = loops_by_head[head];
        } else {
            loop_info = &(lm.loops[curr_loop++]);
        }

        assert(curr_loop <= lm.num_loops);

        loop_info->head = head;
        loop_info->pre_header = head->prev;
        loop_info->tails.push_back(tail);
        loop_info->body.insert(body.begin(), body.end());
        loops_by_head[head] = loop_info;
    }

    assert(curr_loop == lm.num_loops);

    // order the loops
    for(curr_loop = 0; curr_loop < lm.num_loops; ++curr_loop) {
        lm.ordered_loops.insert(&(lm.loops[curr_loop]));
    }
}

void loop_map::clean_up(void) throw() {
    num_loops = 0;
    ordered_loops.clear();
    if(0 != loops) {
        delete [] loops;
        loops = 0;
    }
}

loop_map::~loop_map(void) throw() {
    clean_up();
}

loop::loop(void) throw()
    : pre_header(0)
    , head(0)
    , body()
    , tails()
{ }

loop::~loop(void) throw() {
    pre_header = 0;
    head = 0;
    body.clear();
    tails.clear();
}

unsigned loop_map::size(void) const throw() {
    return num_loops;
}

/// apply a callback to each loop
bool loop_map::for_each_loop(
    bool (*callback)(basic_block *, basic_block *, std::vector<basic_block *> &, std::set<basic_block *> &)
) throw() {
    std::set<loop *>::iterator loop_mapping(ordered_loops.begin());
    for(; loop_mapping != ordered_loops.end(); ++loop_mapping) {
        loop *loop_(*loop_mapping);

        if(!callback(loop_->pre_header, loop_->head, loop_->tails, loop_->body)) {
            return false;
        }
    }
    return true;
}

