/*
 * type.h
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_TYPE_H_
#define asn1_TYPE_H_

extern "C" {
#   include <simple.h>
}

namespace type {

    /// compare two types for structural equivalence
    bool compatible(const simple_type *, const simple_type *) throw();

    /// compare two types for identity
    bool equivalent(const simple_type *, const simple_type *) throw();

    /// check if a type is a scalar type
    bool is_scalar(const simple_type *) throw();
}


#endif /* asn1_TYPE_H_ */
