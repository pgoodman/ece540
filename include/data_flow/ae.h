/*
 * ae.h
 *
 *  Created on: Mar 5, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn4_AE_H_
#define asn4_AE_H_

#include <vector>
#include <map>

extern "C" {
#   include <simple.h>
}

// forward declarations
class available_expression_map;
class basic_block;
class cfg;

namespace detail {

    /// a representation of an available expression as an orderable "string".
    /// this can be computer from an instruction, and potentially stores the
    /// instruction's registers in a different order.
    ///
    /// unary operators are given null op_rights.
    struct available_expression_impl {
    public:
        simple_op op_code;
        const simple_reg *op_left;
        const simple_reg *op_right;

        available_expression_impl(const simple_instr *) throw();

        bool operator<(const available_expression_impl &) const throw();
        bool operator==(const available_expression_impl &) const throw();
    };
}

/// the first "version" of some expression that is available. this knows about
/// the instruction that generated this expression (in program order), and about
/// its own position in the available expression map's sequence of expressions.
struct available_expression {
public:
    unsigned id;
    simple_instr *in;

    available_expression(unsigned, simple_instr *) throw();

    bool operator<(const available_expression &) const throw();
    bool operator==(const available_expression &) const throw();
};

/// represents a set of available expressions
class available_expression_set : public std::set<available_expression> {
public:

    /// erase any expression that uses a particular value
    void erase(const simple_reg *) throw();
};

void find_available_expressions(cfg &, available_expression_map &) throw();

/// maps
class available_expression_map {
private:

    friend void find_available_expressions(cfg &, available_expression_map &) throw();

    std::map<detail::available_expression_impl, unsigned> expression_ids;
    std::vector<available_expression> expressions;
    std::vector<available_expression_set> expression_sets;

    static bool find_expression(basic_block *, available_expression_map &self) throw();
    static bool find_expression(simple_instr *, available_expression_map &self) throw();

    unsigned next_expression_id;

    // TODO: this could be sketchy later if cfg changes size
    unsigned num_basic_blocks;

public:

    available_expression_map(void) throw();

    /// apply a function for each expression
    bool for_each_expression(bool (*callback)(available_expression)) throw();

    template <typename T0>
    bool for_each_expression(bool (*callback)(available_expression, T0 &), T0 &a0) throw() {
        for(unsigned i(0); i < next_expression_id; ++i) {
            if(!callback(expressions[i], a0)) {
                return false;
            }
        }
        return true;
    }

    /// get an expression by an instruction
    available_expression operator()(const simple_instr *in) throw();

    /// get a set of available expressions by the basic block
    available_expression_set &operator()(const basic_block *) throw();

    /// clear out all sets of available expressions for each basic block
    void clear(void) throw();
};


#endif /* asn4_AE_H_ */
