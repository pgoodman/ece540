/*
 * licm.h
 *
 *  Created on: Mar 29, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_LICM_H_
#define project_LICM_H_

#include "include/data_flow/dom.h"

class optimizer;
class cfg;
class loop_map;

void hoist_loop_invariant_code(optimizer &, use_def_map &, dominator_map &, loop_map &) throw();

#endif /* project_LICM_H_ */
