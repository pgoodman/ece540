/*
 * var_def.h
 *
 *  Created on: Feb 20, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn3_VAR_DEF_H_
#define asn3_VAR_DEF_H_

#include <set>

extern "C" {
#   include <simple.h>
}

#include "include/partial_function.h"

class cfg;
class basic_block;

/// variable definition type
struct var_def {
public:
    simple_reg *reg;
    simple_instr *in;
    basic_block *bb;

    bool operator<(const var_def &) const throw();
    bool operator==(const var_def &) const throw();
};

/// compare two simple instructions according to the addresses of their
/// destination registers.
struct compare_var_defs {
public:
    bool operator()(const var_def &a, const var_def &b) const throw();
};

/// domain of the problem: each basic block outputs a variable definition
/// set containing those definitions that reach the end of the basic block.
class var_def_set : public std::set<var_def> {
public:

    typedef std::set<var_def>::iterator iterator;
    typedef std::set<var_def>::const_iterator const_iterator;

    void erase(simple_reg *) throw();
    iterator find(simple_reg *) throw();
    const_iterator find(simple_reg *) const throw();
};

/// mapping of basic blocks to their variable definition sets
typedef partial_function<basic_block *, var_def_set> var_def_map;

/// find variable definitions
void find_var_defs(cfg &flow, var_def_map &var_defs) throw();
void find_local_var_defs(cfg &flow, var_def_map &var_defs) throw();

/// apply a function to each def of a variable in an instruction; returns true
/// if the function was called
template <typename T0>
bool for_each_var_def(
    void (*def_register)(simple_reg *, simple_reg **, simple_instr *, T0 &),
    simple_instr *in,
    T0 &data
) throw() {
    if(0 == in) {
        return false;
    }
    switch(in->opcode) {
    case CPY_OP: case CVT_OP: case NEG_OP: case NOT_OP: case LOAD_OP:
        def_register(in->u.base.dst, &(in->u.base.dst), in, data);
        break;
    case ADD_OP: case SUB_OP: case MUL_OP: case DIV_OP: case REM_OP:
    case MOD_OP: case AND_OP: case IOR_OP: case XOR_OP: case ASR_OP:
    case LSL_OP: case LSR_OP: case ROT_OP: case SEQ_OP: case SNE_OP:
    case SL_OP: case SLE_OP:
        def_register(in->u.base.dst, &(in->u.base.dst), in, data);
        break;
    case LDC_OP:
        def_register(in->u.ldc.dst, &(in->u.ldc.dst), in, data);
        break;
    case CALL_OP:
        if(0 != in->u.call.dst) {
            def_register(in->u.call.dst, &(in->u.call.dst), in, data);
            break;
        }
        // fall-through
    default: return false;
    }
    return true;
}

/// return true if the input instruction defines and variable and assign to
/// the input register the variable assigned by the instruction
bool for_each_var_def(simple_instr *, simple_reg *&) throw();

#endif /* asn3_VAR_DEF_H_ */
