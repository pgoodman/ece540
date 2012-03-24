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

class cfg;

void propagate_copies(optimizer &, cfg &) throw();

#endif /* project_CP_H_ */
