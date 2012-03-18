/*
 * dom.h
 *
 *  Created on: Jan 21, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_DOM_H_
#define asn1_DOM_H_

#include <set>

#include "include/partial_function.h"
#include "include/basic_block.h"

/// a set of dominators, possibly needing more refinement
typedef std::set<basic_block *> dominator_set;

/// a mapping between basic blocks and its set of dominators
typedef partial_function<basic_block *, dominator_set> dominator_map;

/// compute the dominators set
void find_dominators(cfg &flow, dominator_map &dominators) throw();

/// given the a dominator map, find the immediate dominator of some basic
/// block
basic_block *find_immediate_dominator(dominator_map &, basic_block *) throw();

#endif /* asn1_DOM_H_ */
