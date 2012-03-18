/*
 * type.cc
 *
 *  Created on: Jan 14, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include "type.h"

namespace type {

    /// compare two types for structural equivalence
    bool compatible(const simple_type *a, const simple_type *b) throw() {
        if(equivalent(a, b)) {
            return true;
        }

        // differ in the size of their representations
        if(a->len != b->len) {
            return false;
        }

        // at least one of the types is a record type
        if(RECORD_TYPE == a->base || RECORD_TYPE == b->base) {
            return false;
        }

        return true;
    }

    /// compare two types for identity
    bool equivalent(const simple_type *a, const simple_type *b) throw() {
        return a == b;
    }

    /// check if a type is a scalar type
    bool is_scalar(const simple_type *a) throw() {
        return RECORD_TYPE != a->base;
    }
}
