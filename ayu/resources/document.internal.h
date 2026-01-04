#pragma once

namespace ayu::in {

struct DocumentLinks {
    DocumentLinks* prev;
    DocumentLinks* next;
    DocumentLinks () : prev(this), next(this) { }
    DocumentLinks (DocumentLinks* o) :
        prev(o->prev),
        next(o)
    { // Insert this before the provided link
        o->prev->next = this;
        o->prev = this;
    }
    DocumentLinks (const DocumentLinks&) = delete;
    DocumentLinks (DocumentLinks&& o) {
         // Moving a cyclically linked list is not trivial, and there are many
         // ways it can go wrong.
        auto op = o.prev; auto on = o.next;
        op->next = this; on->prev = this;
        prev = o.prev; next = o.next;
        o.prev = &o; o.next = &o;
    }
    DocumentLinks& operator= (const DocumentLinks&) = delete;
    DocumentLinks& operator= (DocumentLinks&& o) {
        this->~DocumentLinks();
        new (this) DocumentLinks(move(o));
        return *this;
    }
    ~DocumentLinks () {
        prev->next = next;
        next->prev = prev;
    }
};

struct DocumentData {
    DocumentLinks items;
     // Lookups are likely to be in order, so start searching where the last
     // search ended.  This is nonsemantic and reset when moving.
    mutable DocumentLinks* last_lookup;
    u64 next_id;
    DocumentData () : last_lookup(&items), next_id(0) { }
    DocumentData (DocumentData&& o) :
        items(move(o.items)), last_lookup(&items), next_id(o.next_id)
    { o.last_lookup = &o.items; o.next_id = 0; }
    DocumentData& operator= (DocumentData&& o) {
        this->~DocumentData();
        new (this) DocumentData(move(o));
        return *this;
    }
    ~DocumentData ();
};

} // ayu::in
