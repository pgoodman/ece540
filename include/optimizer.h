/*
 * optimizer.h
 *
 *  Created on: Mar 23, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef project_OPTIMIZER_H_
#define project_OPTIMIZER_H_

#include <vector>
#include <map>
#include <set>

#include "include/cfg.h"
#include "include/data_flow/dom.h"
#include "include/data_flow/ae.h"
#include "include/data_flow/var_def.h"
#include "include/data_flow/var_use.h"
#include "include/loop.h"
#include "include/unsafe_cast.h"


/// manages data dependencies and forward dependencies (e.g. one pass injects
/// another)
///
/// the purpose of the below complexity is so that the only definition/declaration
/// of dependencies by optimization passes is through their argument lists.
class optimizer {
public:

    typedef unsigned pass;

private:

    struct dirty_state {
        bool cfg;
        bool doms;
        bool ae;
        bool var_use;
        bool var_def;
        bool loops;
    } dirty, prev_dirty;

    simple_instr                *instructions;

    cfg                         flow_graph;
    dominator_map               dominators;
    available_expression_map    available_expressions;
    var_def_map                 var_defs;
    var_use_map                 var_uses;
    loop_map                    loops;

    /// type tag; used only to distinguish among overloaded functions below
    template <typename T>
    class tag { };

    /// internal getters based on type tags; these handle getting references to
    /// internal data structures and making sure that they are up to date.

    static cfg &get(optimizer &self, tag<cfg>) throw();
    static dominator_map &get(optimizer &self, tag<dominator_map>) throw();
    static available_expression_map &get(optimizer &self, tag<available_expression_map>) throw();
    static var_def_map &get(optimizer &self, tag<var_def_map>) throw();
    static var_use_map &get(optimizer &self, tag<var_use_map>) throw();
    static loop_map &get(optimizer &self, tag<loop_map>) throw();

    /// optimization pass unwrappers, allow for easily storing optimization
    /// pass functions using the same type, but then manually figuring out and
    /// patching their dependencies.

    static void inject0(void (*callback)(optimizer &), optimizer &self) throw();

    template <typename T0>
    static void inject1(void (*callback)(optimizer &), optimizer &self) throw() {
        typedef void (opt_func)(optimizer &, T0 &);
        opt_func *typed_callback(unsafe_cast<opt_func *>(callback));
        T0 &a0(get(self, tag<T0>()));
        memcpy(&prev_dirty, &dirty, sizeof dirty);
        typed_callback(self, a0);
    }

    template <typename T0, typename T1>
    static void inject2(void (*callback)(optimizer &), optimizer &self) throw() {
        typedef void (opt_func)(optimizer &, T0 &, T1 &);
        opt_func *typed_callback(unsafe_cast<opt_func *>(callback));
        T0 &a0(get(self, tag<T0>()));
        T1 &a1(get(self, tag<T1>()));
        memcpy(&prev_dirty, &dirty, sizeof dirty);
        typed_callback(self, a0, a1);
    }

    template <typename T0, typename T1, typename T2>
    static void inject3(void (*callback)(optimizer &), optimizer &self) throw() {
        typedef void (opt_func)(optimizer &, T0 &, T1 &, T2 &);
        opt_func *typed_callback(unsafe_cast<opt_func *>(callback));
        T0 &a0(get(self, tag<T0>()));
        T1 &a1(get(self, tag<T1>()));
        T2 &a2(get(self, tag<T2>()));
        memcpy(&prev_dirty, &dirty, sizeof dirty);
        typed_callback(self, a0, a1, a2);
    }

    /// represents all information needed to "recover" a type-erased optimization
    /// pass into the fully typed one with dependency injection automatically
    /// handled.
    typedef void (untyped_optimization_func)(optimizer &);
    typedef void (optimization_unwrapper)(untyped_optimization_func *, optimizer &);
    struct internal_pass {
    public:
        untyped_optimization_func *untyped_func;
        optimization_unwrapper *unwrapper;
    };

    std::vector<internal_pass> passes;
    std::map<unsigned, std::set<pass> > cascades[2];

public:

    optimizer(simple_instr *) throw();

    bool added_def(void) throw();
    bool removed_use(void) throw();
    bool added_block(void) throw();
    bool removed_block(void) throw();
    bool changed_control_flow(void) throw();

    /// functions to add optimizations passes to the optimizer

    pass add_pass(void (*func)(optimizer &)) throw();

    template <typename T0>
    pass add_pass(void (*func)(optimizer &, T0 &)) throw() {
        internal_pass p;
        p.untyped_func = unsafe_cast<untyped_optimization_func *>(func);
        p.unwrapper = &inject1<T0>;
        pass pass_id(static_cast<unsigned>(passes.size()));
        passes.push_back(p);
        return pass_id;
    }

    template <typename T0, typename T1>
    pass add_pass(void (*func)(optimizer &, T0 &, T1 &)) throw() {
        internal_pass p;
        p.untyped_func = unsafe_cast<untyped_optimization_func *>(func);
        p.unwrapper = &inject2<T0,T1>;
        pass pass_id(static_cast<unsigned>(passes.size()));
        passes.push_back(p);
        return pass_id;
    }

    template <typename T0, typename T1, typename T2>
    pass add_pass(void (*func)(optimizer &, T0 &, T1 &, T2 &)) throw() {
        internal_pass p;
        p.untyped_func = unsafe_cast<untyped_optimization_func *>(func);
        p.unwrapper = &inject3<T0,T1,T2>;
        pass pass_id(static_cast<unsigned>(passes.size()));
        passes.push_back(p);
        return pass_id;
    }

    /// add a conditional cascading relation between two optimization passes.
    /// true: if the first changes the cfg, run the second
    /// false: if the first doesn't change the cfg, run the second
    void cascade_cond(pass &, pass &, bool) throw();

    /// run an optimization pass, and recursive cascade; returns true iff
    /// anything was done
    bool run(pass &) throw();
};


#endif /* project_OPTIMIZER_H_ */
