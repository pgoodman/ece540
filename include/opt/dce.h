/*
 * dce.h
 *
 *  Created on: Mar 23, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_DCE_H_
#define project_DCE_H_

#include "include/optimizer.h"

class cfg;

/// eliminate all dead and unreachable code
void eliminate_dead_code(optimizer &, cfg &, use_def_map &) throw();

#endif /* project_DCE_H_ */
