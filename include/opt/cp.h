/*
 * cp.h
 *
 *  Created on: Mar 18, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_CP_H_
#define project_CP_H_

#include "include/optimizer.h"
#include "include/use_def.h"

class cfg;

/// propogate copies of non-temporary variables through the CFG
void propagate_copies(optimizer &, cfg &, use_def_map &) throw();

#endif /* project_CP_H_ */
