/*
 * unsafe_cast.hpp
 *
 *  Created on: Mar 7, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn4_UNSAFE_CAST_HPP_
#define asn4_UNSAFE_CAST_HPP_

#include <stdint.h>
#include <cstring>

template <typename ToT, typename FromT>
inline ToT unsafe_cast(const FromT &v) throw()  {
    ToT dest;
    memcpy(&dest, &v, sizeof(ToT));
    return dest;
}

template <typename ToT, typename FromT>
inline ToT unsafe_cast(FromT *v) throw() {
    return unsafe_cast<ToT>(
        reinterpret_cast<uintptr_t>(v)
    );
}


#endif /* asn4_UNSAFE_CAST_HPP_ */
