/*
 * partial_function.h
 *
 *  Created on: Jan 24, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_PARTIAL_FUNCTION_H_
#define asn1_PARTIAL_FUNCTION_H_

#include <map>

/// simple wrapper around map so that later we can treat everything uniformly
/// as a functor, and later on can optimize this data structure.
template <typename Domain, typename Range>
class partial_function : public std::map<Domain, Range> {
public:

    Range &operator()(const Domain &in) throw() {
        return this->operator[](in);
    }
};


#endif /* asn1_PARTIAL_FUNCTION_H_ */
