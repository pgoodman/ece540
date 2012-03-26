/*
 * cse.h
 *
 *  Created on: Mar 25, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_CSE_H_
#define project_CSE_H_

class cfg;
class optimizer;
class available_expression_map;

void eliminate_common_sub_expressions(optimizer &, cfg &, available_expression_map &) throw();


#endif /* project_CSE_H_ */
