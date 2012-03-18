/*
 * operator.h
 *
 *  Created on: Jan 24, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_OPERATOR_H_
#define asn1_OPERATOR_H_

/// convenience functions for unary operators
template <typename I>
class unary_operator {
public:
    static I copy(const I &in) throw() {
        return in;
    }

    static I &identity(I &in) throw() {
        return in;
    }

    static I logical_not(const I &in) throw() {
        return !in;
    }

    static I bitwise_not(const I &in) throw() {
        return ~in;
    }
};

/// convenience functions for binary operators
template <typename L, typename R=L, typename O=L>
class binary_operator {
public:
    static O add(const L &ll, const R &rr) throw() {
        return ll + rr;
    }

    static O subtract(const L &ll, const R &rr) throw() {
        return ll - rr;
    }

    static O multiply(const L &ll, const R &rr) throw() {
        return ll * rr;
    }

    static O divide(const L &ll, const R &rr) throw() {
        return ll / rr;
    }

    static O modulo(const L &ll, const R &rr) throw() {
        return ll % rr;
    }

    static O logical_and(const L &ll, const R &rr) throw() {
        return ll && rr;
    }

    static O logical_or(const L &ll, const R &rr) throw() {
        return ll || rr;
    }

    static O equal(const L &ll, const R &rr) throw() {
        return ll == rr;
    }

    static O not_equal(const L &ll, const R &rr) throw() {
        return ll != rr;
    }

    static O less_than(const L &ll, const R &rr) throw() {
        return ll < rr;
    }

    static O less_than_equal(const L &ll, const R &rr) throw() {
        return ll <= rr;
    }

    static O bitwise_and(const L &ll, const R &rr) throw() {
        return ll & rr;
    }

    static O bitwise_or(const L &ll, const R &rr) throw() {
        return ll | rr;
    }

    static O bitwise_xor(const L &ll, const R &rr) throw() {
        return ll ^ rr;
    }
};

#endif /* asn1_OPERATOR_H_ */
