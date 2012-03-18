/*
 * instr.h
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_INSTR_H_
#define asn1_INSTR_H_

extern "C" {
#   include <simple.h>
}


namespace instr {

    bool is_local_control_flow_transfer(const simple_instr *) throw();
    bool is_label(const simple_instr *) throw();
    bool is_return(const simple_instr *) throw();
    unsigned replace_symbol(simple_instr *, const simple_sym *, simple_sym *) throw();
    bool jumps_to(const simple_instr *, const simple_sym *) throw();
    void insert_before(simple_instr *, simple_instr *) throw();
    void insert_after(simple_instr *, simple_instr *) throw();
    bool can_fall_through(const simple_instr *) throw();
    bool can_default_fall_through(const simple_instr *) throw();
    bool can_transfer(const simple_instr *, const simple_instr *) throw();
    bool is_var_def(const simple_instr *) throw();
    bool is_expression(const simple_instr *) throw();
}


#endif /* asn1_INSTR_H_ */
