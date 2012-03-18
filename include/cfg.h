/*
 * cfg.hpp
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_CFG_HPP_
#define asn1_CFG_HPP_

extern "C" {
#   include <simple.h>
}

#include "include/basic_block.h"

/// represents a control-flow graph
class cfg {
private:

    /// first and last basic blocks
    basic_block *entry_;
    basic_block *exit_;
    basic_block *last_allocated;

    /// the next basic block id
    unsigned next_block_id;

    simple_instr *instr_list;

    /// create a new basic block
    basic_block *make_bb(simple_instr *, simple_instr *, unsigned) throw();

    static void connect_bbs(basic_block *, basic_block *) throw();

public:

    /// create a control-flow graph from a sequence of instructions
    cfg(simple_instr *) throw();

    ~cfg(void) throw();

    /// number of basic blocks in this CFG
    unsigned size(void) const throw();

    basic_block_iterator begin(void) throw();
    const basic_block_iterator end(void) const throw();

    basic_block *entry(void) const throw();
    basic_block *exit(void) const throw();

    basic_block *unsafe_insert_block(basic_block *, basic_block *, simple_instr *, simple_instr *) throw();

    void relink(void) throw();

    bool for_each_basic_block(bool (*callback)(basic_block *));

    // allow passing of some info to the callback
    template <typename T0>
    bool for_each_basic_block(bool (*callback)(basic_block *, T0 &), T0 &t0) {
        for(basic_block *bb(entry_); 0 != bb; bb = bb->next) {
            if(!callback(bb, t0)) {
                return false;
            }
        }
        return true;
    }

    template <typename T0, typename T1>
    bool for_each_basic_block(bool (*callback)(basic_block *, T0 &, T1 &), T0 &t0, T1 &t1) {
        for(basic_block *bb(entry_); 0 != bb; bb = bb->next) {
            if(!callback(bb, t0, t1)) {
                return false;
            }
        }
        return true;
    }
};


#endif /* asn1_CFG_HPP_ */
