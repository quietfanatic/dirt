#include "purpose.h"

#include "resource.private.h"
#include "universe.private.h"

namespace ayu {
namespace in {

struct PushCurrentPurpose {
    Purpose* old;
    PushCurrentPurpose (Purpose* p) : old(current_purpose) {
        current_purpose = p;
    }
    ~PushCurrentPurpose () {
        current_purpose = old;
    }
};

static Resource* find_in_purpose (Purpose& self, Resource res) {
    for (auto& r : self.resources) {
        if (r == res) return &r;
    }
    return null;
}

static void add_to_purpose (Purpose& self, Resource res) {
    if (find_in_purpose(self, res)) return;
    self.resources.push_back(res);
    res.data->purpose_count++;
}

static void remove_from_purpose (Purpose& self, Resource res) {
    if (auto p = find_in_purpose(self, res)) {
        self.resources.erase(p);
        --res.data->purpose_count;
    }
}

} using namespace in;

void Purpose::acquire (Resource res) {
    add_to_purpose(*this, res);
    PushCurrentPurpose _(this);
    try {
        load_under_purpose(res);
    }
    catch (...) { remove_from_purpose(*this, res); throw; }

    if (ResourceTransaction::depth) {
        struct AcquireCommitter : Committer {
            Purpose* self;
            Resource res;
            AcquireCommitter (Purpose* s, Resource r) : self(s), res(r) { }
            void rollback () noexcept override {
                remove_from_purpose(*self, res);
                 // If remove_from_purpose returns false, it means we didn't
                 // find the resource in out list, maybe because someone called
                 // release() on this resource while the transaction was still
                 // active.  I guess we'll let it slide.
            }
        };
        ResourceTransaction::add_committer(
            new AcquireCommitter{this, res}
        );
    }
}

void Purpose::release (Resource res) {
    if (!find_in_purpose(*this, res)) {
        raise(e_ResourceNotInPurpose,
            "Cannot release Resource from Purpose that doesn't have it."
        );
    }
    if (!--res.data->purpose_count) {
        unload(res);
    }
     // Don't reuse iterator from find_in_purpose, it might have been
     // invalidated during unload().  Unlikely but possible.
    remove_from_purpose(*this, res);
    if (ResourceTransaction::depth) {
        struct ReleaseCommitter : Committer {
            Purpose* self;
            Resource res;
            ReleaseCommitter (Purpose* s, Resource r) : self(s), res(r) { }
            void rollback () noexcept override {
                add_to_purpose(*self, res);
            }
        };
        ResourceTransaction::add_committer(
            new ReleaseCommitter(this, res)
        );
    }
}

void Purpose::release_all () {
    auto reses = move(resources);
    for (auto& res : reses) {
        if (!--res.data->purpose_count) {
            unload(res);
        }
    }
    if (ResourceTransaction::depth) {
        struct ReleaseAllCommitter : Committer {
            Purpose* self;
            UniqueArray<Resource> reses;
            ReleaseAllCommitter (Purpose* s, UniqueArray<Resource>&& r) :
                self(s), reses(move(r))
            { }
            void rollback () noexcept override {
                reses.consume([&](auto&& res) { add_to_purpose(*self, res); });
            }
        };
        ResourceTransaction::add_committer(
            new ReleaseAllCommitter(this, move(reses))
        );
    }
}

Purpose general_purpose;
Purpose* current_purpose = &general_purpose;

} // ayu
