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

void eliminate_dead_code(optimizer &, cfg &) throw();


#endif /* project_DCE_H_ */
