/*
 * closure.h
 *
 *  Created on: Jan 24, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_CLOSURE_H_
#define asn1_CLOSURE_H_

class cfg;

/// compute the transitive closure of the entry/exit basic blocks of the
/// control flow graph. this updates the graph in place.
void find_closure(cfg &flow) throw();

#endif /* asn1_CLOSURE_H_ */
