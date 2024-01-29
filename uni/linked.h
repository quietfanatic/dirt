#pragma once
#include "common.h"

namespace uni {

template <class T>
struct Links {
    Links<T>* next;
    Links<T>* prev;
};

template <class T>
struct Linked : Links<T> {
    Linked () : Links<T>(this, this) { }
    Linked (Links<T>* p) : Links<T>(p, p->prev) {
        p->prev = this;
        this->prev->next = this;
    }
    ~Linked () {
        this->prev->next = this->next;
        this->next->prev = this->prev;
    }
    void link (Links<T>* p) {
        this->prev->next = this->next;
        this->next->prev = this->prev;
        this->next = p;
        this->prev = p->prev;
        p->prev = this;
        this->prev->next = this;
    }
    void unlink () {
        this->prev->next = this->next;
        this->next->prev = this->prev;
    }
};

template <class T>
struct LinkedList : Links<T> {
    LinkedList () : Links<T>(this, this) { }

    struct iterator {
        Links<T>* p;
        T& operator* () const { return *static_cast<T*>(p); }
        T* operator-> () const { return static_cast<T*>(p); }
        iterator& operator++ () { p = p->next; return *this; }
        friend bool operator == (iterator, iterator) = default;
    };
    iterator begin () const { return iterator{this->next}; }
    iterator end () const { return iterator{const_cast<LinkedList<T>*>(this)}; }
    explicit operator bool () const { return this->next != this->prev; }
    bool empty () const { return !*this; }
    usize size () const {
        usize r = 0;
        for (auto& l : *this) r += 1;
        return r;
    }
};

} // uni
