 // This provide a generic all-or-nothing transaction system.  While a
 // Transaction is alive, you can register Committers, and when all Transaction
 // objects are destroyed, all Committers either commit() or rollback(),
 // depending on whether there is an active exception.
#pragma once

#include <exception>
#include <memory>
#include <cstdio>
#include "arrays.h"
#include "common.h"

namespace uni {

struct Committer {
    virtual void commit () noexcept { }
    virtual void rollback () noexcept { }
    virtual ~Committer () { }
};

 // Templated so you can have multiple domains of transactions.
template <class = void>
struct Transaction {
    static usize depth;

    Transaction () { depth++; }
    ~Transaction () { if (!--depth) finish(); }

    static auto& committers () {
        static UniqueArray<std::unique_ptr<Committer>> r;
        return r;
    }

    static void add_committer (Committer*&& co) {
        committers().emplace_back(move(co));
    }

     // Manually commit.  All current committers will be cleared, but the
     // current transaction will still be active until the last Transaction
     // object is destroyed.  You probably don't want to call this ever.
    static void commit () {
        committers().consume([](auto&& co) { co->commit(); });
    }
     // Manually rollback.  You probably don't want to call this unless you're
     // about to end the Transaction and know it should fail, but don't want to
     // incur the overhead of throwing an exception.
    static void rollback () {
        committers().consume_reverse([](auto&& co) { co->rollback(); });
    }

     // Manually either commit or rollback, depending on whether there's an
     // exception.  Called by ~Transaction.  Don't call this.
    NOINLINE static void finish () {
        if (std::uncaught_exceptions()) rollback();
        else commit();
    }

};
template <class T>
usize Transaction<T>::depth = 0;

} // uni
