/*
 * set.h
 *
 *  Created on: Jan 22, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_SET_H_
#define asn1_SET_H_

#include <set>
#include <functional>
#include <memory>

/// intersect two sets
template <typename SetT>
SetT set_intersection(
    const SetT &a,
    const SetT &b
) throw() {
    SetT out;
    typename SetT::iterator a_it(a.begin()), b_it(b.begin());
    const typename SetT::iterator a_end(a.end()), b_end(b.end());
    typename SetT::key_compare less_than(a.key_comp());

    for(; a_it != a_end && b_it != b_end; ) {
        if(less_than(*a_it, *b_it)) {
            ++a_it;
        } else if(less_than(*b_it, *a_it)) {
            ++b_it;
        } else {
            out.insert(*a_it);
            ++a_it;
            ++b_it;
        }
    }

    return out;
}

/// union two sets
template <typename SetT>
SetT set_union(
    const SetT &a,
    const SetT &b
) throw() {
    SetT out(a.begin(), a.end());
    out.insert(b.begin(), b.end());
    return out;
}

/// accumulate/(left)fold the elements of a set into some thing
template <
    typename Domain,
    typename Range,
    Range (*Unary)(const Domain &),
    Range (*Binary)(const Domain &, const Range &),
    typename C,
    typename A
>
Range set_accumulate(
    const std::set<Domain,C,A> &domain,
    const Range &zero
) throw() {

    if(domain.empty()) {
        return zero;
    }

    typename std::set<Domain,C,A>::const_iterator it(domain.begin());
    typename std::set<Domain,C,A>::const_iterator end(domain.end());

    Range output(Unary(*it));
    for(; ++it != end; ) {
        output = Binary(*it, output);
    }

    return output;
}

#endif /* asn1_SET_H_ */
