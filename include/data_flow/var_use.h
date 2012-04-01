/*
 * var_use.hpp
 *
 *  Created on: Feb 20, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn3_VAR_USE_HPP_
#define asn3_VAR_USE_HPP_

#include <set>
#include <utility>

extern "C" {
#   include <simple.h>
}

#include "include/partial_function.h"

class cfg;
class basic_block;

/// represents a usage of a single variable in an instruction.
struct var_use {
public:
    simple_reg *reg; // the variable being used
    simple_reg **usage; // the exact usage point in the instruction
    simple_instr *in; // the instruction in which the var is being used
    basic_block *bb;

    bool operator<(const var_use &) const throw();
    bool operator==(const var_use &) const throw();
};

/// domain of the problem: a set of variable usages
class var_use_set : public std::set<var_use> {
public:
    void erase(simple_reg *) throw();
    iterator find(simple_reg *) throw();
};

/// mapping of basic blocks to the variables they use
typedef partial_function<basic_block *, var_use_set> var_use_map;

/// find variable uses / live variables
void find_var_uses(cfg &flow, var_use_map &var_uses) throw();
void find_local_var_uses(cfg &flow, var_use_map &var_uses) throw();

/// apply a function to each use of a variable in an instruction. returns true
/// if the function was called.
template <typename T0>
bool for_each_var_use(
    void (*use_register)(simple_reg *, simple_reg **, simple_instr *, T0 &),
    simple_instr *in,
    T0 &data
) throw() {
    switch(in->opcode) {
    case RET_OP:
        if(0 != in->u.base.src1) {
            use_register(in->u.base.src1, &(in->u.base.src1), in, data);
        }
        break;
    case STR_OP: case MCPY_OP:
        use_register(in->u.base.src2, &(in->u.base.src2), in, data);
        use_register(in->u.base.src1, &(in->u.base.src1), in, data);
        break;
    case CPY_OP: case CVT_OP: case NEG_OP: case NOT_OP: case LOAD_OP:
        use_register(in->u.base.src1, &(in->u.base.src1), in, data);
        break;
    case ADD_OP: case SUB_OP: case MUL_OP: case DIV_OP: case REM_OP:
    case MOD_OP: case AND_OP: case IOR_OP: case XOR_OP: case ASR_OP:
    case LSL_OP: case LSR_OP: case ROT_OP: case SEQ_OP: case SNE_OP:
    case SL_OP: case SLE_OP:
        use_register(in->u.base.src1, &(in->u.base.src1), in, data);
        use_register(in->u.base.src2, &(in->u.base.src2), in, data);
        break;
    case BTRUE_OP: case BFALSE_OP:
        use_register(in->u.bj.src, &(in->u.bj.src), in, data);
        break;
    case CALL_OP:
        for(unsigned i(0); i < in->u.call.nargs; ++i) {
            use_register(in->u.call.args[i], &(in->u.call.args[i]), in, data);
        }
        use_register(in->u.call.proc, &(in->u.call.proc), in, data);
        break;
    case MBR_OP:
        use_register(in->u.mbr.src, &(in->u.mbr.src), in, data);
        break;
    default: // NOP_OP, JMP_OP, LABEL_OP, LDC_OP
        return false;
    }
    return true;
}

#endif /* asn3_VAR_USE_HPP_ */
