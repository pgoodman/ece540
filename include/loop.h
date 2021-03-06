/*
 * loop.h
 *
 *  Created on: Jan 28, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn2_LOOP_H_
#define asn2_LOOP_H_

#include <set>
#include <vector>
#include <utility>
#include <functional>
#include <algorithm>

#include "include/data_flow/dom.h"

/// compare two basic blocks
extern bool basic_block_less(const basic_block *, const basic_block *) throw();

/// represents a back-edge that bounds a loop. the back edge goes from
/// tail -> head
struct loop_bounds_type {
public:
    basic_block *tail;
    basic_block *head;

    loop_bounds_type(void) throw();
    loop_bounds_type(basic_block *, basic_block *) throw();
};

/// represents a loop
struct loop {
public:

    basic_block *pre_header;
    basic_block *head;
    std::set<basic_block *> body;
    std::vector<basic_block *> tails;

    explicit loop(void) throw();
    ~loop(void) throw();
};

// comparison for pairs of basic blocks; natural comparison (lexicographic)
// as opposed to the default of pointer comparison.
namespace std {
    template <>
    struct less<loop_bounds_type> : public binary_function<loop_bounds_type, loop_bounds_type, bool> {
    public:
        bool operator()(const loop_bounds_type &a, const loop_bounds_type &b) const throw() {

            // a.head < b.head
            if(a.head < b.head) {
                return true;

            // b.head < a.head
            } else if(b.head < a.head) {
                return false;

            // a.head == b.head
            } else {
                return a.tail < b.tail;
            }
        }
    };

    template <>
    struct less<loop *> : public binary_function<loop *, loop *, bool> {
    public:
        bool operator()(const loop *a, const loop *b) const throw() {
            if(a->head < b->head) {
                return true;

            } else if(b->head < a->head) {
                return false;

            // a.head == b.head
            } else {
                return lexicographical_compare(
                    a->tails.begin(), a->tails.end(),
                    b->tails.begin(), b->tails.end()
                );
            }
        }
    };
}

/// represents a mapping of loops
class loop_map {
private:

    unsigned num_loops;
    loop *loops_;

    friend void find_loops(cfg &, dominator_map &, loop_map &) throw();

    void clean_up(void) throw();

public:

    loop_map(void) throw();
    ~loop_map(void) throw();

    unsigned size(void) const throw();

    template <typename T0>
    bool for_each_loop(bool (*func)(loop &, T0 &), T0 &t0) throw() {
        loop *it(loops_), *end(loops_ + num_loops);
        for(; it != end; ++it) {
            if(!func(*it, t0)) {
                return false;
            }
        }
        return true;
    }
};

/// find alll loops; allows us to re-initiliaze a loop map.
void find_loops(cfg &, dominator_map &, loop_map &) throw();

#endif /* asn2_LOOP_H_ */
