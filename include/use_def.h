/*
 * use_def.h
 *
 *  Created on: Mar 24, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_USE_DEF_H_
#define project_USE_DEF_H_

extern "C" {
#   include <simple.h>
}

#include <set>
#include <map>

#include "include/data_flow/var_def.h"
#include "include/data_flow/var_use.h"

class use_def_map;
class def_use_map;

/// compute the definitions that reach each variable use
void find_defs_reaching_uses(cfg &, var_def_map &, use_def_map &) throw();

/// maps a use of a variable to all definitions that reach the use
class use_def_map {
private:

    friend class def_use_map;
    friend void find_defs_reaching_uses(cfg &, var_def_map &, use_def_map &) throw();

    std::map<simple_instr *, var_def_set> ud_map;

public:

    typedef std::map<simple_instr *, var_def_set>::iterator iterator;

    const var_def_set &operator()(simple_instr *) throw();

};

#endif /* project_USE_DEF_H_ */
