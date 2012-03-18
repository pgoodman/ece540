/*
 * linked_list_iterator.h
 *
 *  Created on: Jan 15, 2012
 *      Author: petergoodman
 *     Version: $Id$
 */

#ifndef asn1_LINKED_LIST_ITERATOR_H_
#define asn1_LINKED_LIST_ITERATOR_H_

#include <iterator>
#include <cstddef>

template <typename T>
class linked_list_iterator : public std::iterator<
    std::output_iterator_tag,
    ptrdiff_t,
    T,
    T *,
    T &
> {
private:

    typedef T *pointer;
    typedef T &reference;

    pointer pos;

public:

    linked_list_iterator(pointer pos_) throw()
        : pos(pos_)
    { }

    linked_list_iterator(const linked_list_iterator &that) throw()
        : pos(that.pos)
    { }

    linked_list_iterator &operator=(const linked_list_iterator &that) throw() {
        pos = that.pos;
        return *this;
    }

    pointer operator*(void) throw() {
        return pos;
    }

    pointer operator->(void) throw() {
        return pos;
    }

    linked_list_iterator &operator++(void) throw() {
        pos = pos->next;
        return *this;
    }

    linked_list_iterator operator++(int) throw() {
        return linked_list_iterator(pos->next);
    }

    bool operator==(const linked_list_iterator &that) const throw() {
        return pos == that.pos;
    }

    bool operator!=(const linked_list_iterator &that) const throw() {
        return pos != that.pos;
    }
};


#endif /* asn1_LINKED_LIST_ITERATOR_H_ */
