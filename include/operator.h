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


namespace op {

    template <typename L, typename O>
    struct bitwise_not {
        O operator()(const L &ll) throw() {
            return ~ll;
        }
    };

    template <typename L, typename O>
    struct negate {
        O operator()(const L &ll) throw() {
            return -ll;
        }
    };

    template <typename L, typename R, typename O>
    struct add {
        O operator()(const L &ll, const R &rr) throw() {
            return ll + rr;
        }
    };

    template <typename L, typename R, typename O>
    struct subtract {
        O operator()(const L &ll, const R &rr) throw() {
            return ll - rr;
        }
    };

    template <typename L, typename R, typename O>
    struct multiply {
        O operator()(const L &ll, const R &rr) throw() {
            return ll * rr;
        }
    };

    template <typename L, typename R, typename O>
    struct divide {
        O operator()(const L &ll, const R &rr) throw() {
            return ll / rr;
        }
    };

    template <typename L, typename R, typename O>
    struct modulo {
        O operator()(const L &ll, const R &rr) throw() {
            return ll % rr;
        }
    };

    template <typename L, typename R, typename O>
    struct logical_and {
        O operator()(const L &ll, const R &rr) throw() {
            return ll && rr;
        }
    };

    template <typename L, typename R, typename O>
    struct logical_or {
        O operator()(const L &ll, const R &rr) throw() {
            return ll || rr;
        }
    };

    template <typename L, typename R, typename O>
    struct equal {
        O operator()(const L &ll, const R &rr) throw() {
            return ll == rr;
        }
    };

    template <typename L, typename R, typename O>
    struct not_equal {
        O operator()(const L &ll, const R &rr) throw() {
            return ll != rr;
        }
    };

    template <typename L, typename R, typename O>
    struct less_than {
        O operator()(const L &ll, const R &rr) throw() {
            return ll < rr;
        }
    };

    template <typename L, typename R, typename O>
    struct less_than_equal {
        O operator()(const L &ll, const R &rr) throw() {
            return ll <= rr;
        }
    };

    template <typename L, typename R, typename O>
    struct bitwise_and {
        O operator()(const L &ll, const R &rr) throw() {
            return ll & rr;
        }
    };

    template <typename L, typename R, typename O>
    struct bitwise_or {
        O operator()(const L &ll, const R &rr) throw() {
            return ll | rr;
        }
    };

    template <typename L, typename R, typename O>
    struct bitwise_xor {
        O operator()(const L &ll, const R &rr) throw() {
            return ll ^ rr;
        }
    };

    /// computes a modulo in terms of how SUIF sees it, i.e. always returning
    /// a non-negative integer
    int mod(const int &ll, const int &rr) throw();

    /// compute a logical right shift (fill in zeroes for high order bits)
    int lsr(const int &ll, const unsigned &rr) throw();

    /// perform a logical left shift.
    /// TODO: worry about rr being too big?
    int lsl(const int &ll, const unsigned &rr) throw();

    /// compute an arithmetic right shift (use high order bit as fill)
    int asr(const int &ll, const unsigned &rr) throw();

    /// compute a rotation of the bits
    int rot(const int &ll, const int &rr) throw();
}


#endif /* asn1_OPERATOR_H_ */
