/*
 * cf.h
 *
 *  Created on: Mar 18, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_CF_H_
#define project_CF_H_

class cfg;

/// apply the constant folding optimization to a control flow graph. returns
/// true if the graph was updated.
bool fold_constants(cfg &) throw();

#endif /* project_CF_H_ */
