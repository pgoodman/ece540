/*
 * diag.hpp
 *
 *  Created on: Jan 21, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_DIAG_HPP_
#define asn1_DIAG_HPP_

#if defined(__GNUC__) || defined(__clang__)
#   define DIAG_ATTR __attribute__((format(printf, 3, 4)))
#else
#   define DIAG_ATTR
#endif

namespace diag {

#   define error(...) error_(__FILE__, __LINE__, __VA_ARGS__)
#   define warning(...) warning_(__FILE__, __LINE__, __VA_ARGS__)

    /// generalizations of Simple SUIF's _simple_error and _simple_warning
    /// that do generate the same compiler warnings for conversions from const
    /// char * to char *, and that also allow printf formatting.
    void error_(const char *, int, const char *message, ...) throw() DIAG_ATTR;
    void warning_(const char *, int, const char *message, ...) throw() DIAG_ATTR;

}


#endif /* asn1_DIAG_HPP_ */
