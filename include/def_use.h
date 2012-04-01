/*
 * def_use.h
 *
 *  Created on: Mar 30, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_DEF_USE_H_
#define project_DEF_USE_H_

extern "C" {
#   include <simple.h>
}

#include <map>

#include "include/data_flow/var_use.h"

class def_use_map;
class cfg;

/// compute the definitions that reach each variable use
void find_uses_reaching_defs(cfg &, var_use_map &, def_use_map &) throw();

/// maps a definition of a variable to all uses that the definition reaches
class def_use_map {
private:

    friend void find_uses_reaching_defs(cfg &, var_use_map &, def_use_map &) throw();

    std::map<simple_instr *, var_use_set> du_map;

public:

    const var_use_set &operator()(simple_instr *) throw();

};


#endif /* project_DEF_USE_H_ */
