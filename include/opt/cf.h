/*
 * cf.h
 *
 *  Created on: Mar 18, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_CF_H_
#define project_CF_H_

#include "include/optimizer.h"

class cfg;

/// apply the constant folding optimization to a control flow graph. returns
/// true if the graph was updated.
void fold_constants(optimizer &, cfg &) throw();

#endif /* project_CF_H_ */
