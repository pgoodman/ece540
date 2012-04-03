/*
 * operator.cc
 *
 *  Created on: Apr 1, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#include "include/diag.h"

namespace op {

    /// computes a modulo in terms of how SUIF sees it, i.e. always returning
    /// a non-negative integer
    int mod(const int &ll, const int &rr) throw() {
        int mod(ll % rr);
        if(0 > mod) {
            mod += rr;
        }
        return mod;
    }

    /// compute a logical right shift (fill in zeroes for high order bits)
    int lsr(const int &ll, const unsigned &rr) throw() {
        if(rr > (sizeof ll * 8U)) {
            diag::warning("Right shift of size %d is too big.", rr);
            return ll >= 0 ? 0 : ~0;

        }

        // TODO: issue on x < rr < num bits, where x is max size of shift right?
        return static_cast<int>(
            static_cast<unsigned>(ll) >> static_cast<unsigned>(rr)
        );
    }

    /// perform a logical left shift.
    /// TODO: worry about rr being too big?
    int lsl(const int &ll, const unsigned &rr) throw() {
        return ll << rr;
    }

    enum {
        INT_NUM_BITS = sizeof(int) * 8U
    };

    /// compute an arithmetic right shift (use high order bit as fill)
    int asr(const int &ll, const unsigned &rr) throw() {
        int lsr_(lsr(ll, rr));
        if(ll >= 0) {
            return lsr_;
        } else if(rr < INT_NUM_BITS) {
            return lsr_ | lsl(~0, INT_NUM_BITS - rr);
        } else {
            return ~0;
        }
    }

    /// compute a rotation of the bits
    int rot(const int &ll, const int &rr) throw() {
        unsigned ul(static_cast<unsigned>(ll));
        int sr(rr);

        if(rr > 0) { // left
            return static_cast<int>((ul << sr) | (ul >> (INT_NUM_BITS - sr)));

        } else if(0 == rr) { // nowhere
            return ll;

        } else { // right
            sr = -sr;
            return static_cast<int>((ul >> sr) | (ul << (INT_NUM_BITS - sr)));
        }
    }
}


