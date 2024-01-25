#include "purpose.h"

#include "resource.private.h"

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

static void add_to_purpose (Purpose& self, ResourceRef res) {
    if (self.find(res)) return;
    self.resources.push_back(res);
    auto data = static_cast<ResourceData*>(res.data);
    data->purpose_count++;
}

static void remove_from_purpose (Purpose& self, ResourceRef res) {
    if (auto p = self.find(res)) {
        self.resources.erase(p);
        auto data = static_cast<ResourceData*>(res.data);
        --data->purpose_count;
    }
}

} using namespace in;

void Purpose::acquire (ResourceRef res) {
    add_to_purpose(*this, res);
    PushCurrentPurpose _(this);
    try {
        load_under_purpose(res);
    }
    catch (...) { remove_from_purpose(*this, res); throw; }

    if (ResourceTransaction::depth) {
        struct AcquireCommitter : Committer {
            Purpose* self;
             // Keep a reference count for this resource.  We considered keeping
             // a ResourceRef instead because we're never going to dereference
             // it, but there's a possibility of another ResourceData being
             // allocated that just happens to have the same address, which
             // could cause problems.
            SharedResource res;
            AcquireCommitter (Purpose* s, ResourceRef r) : self(s), res(r) { }
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

void Purpose::release (ResourceRef res) {
    if (!find(res)) {
        raise(e_ResourceNotInPurpose,
            "Cannot release Resource from Purpose that doesn't have it."
        );
    }
    auto data = static_cast<ResourceData*>(res.data);
    if (!--data->purpose_count) {
        unload(res);
    }
     // Don't reuse iterator from find_in_purpose, it might have been
     // invalidated during unload().  Unlikely but possible.
    remove_from_purpose(*this, res);
    if (ResourceTransaction::depth) {
        struct ReleaseCommitter : Committer {
            Purpose* self;
            SharedResource res;
            ReleaseCommitter (Purpose* s, ResourceRef r) : self(s), res(r) { }
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
        auto data = static_cast<ResourceData*>(res.data.p);
        if (!--data->purpose_count) {
            unload(res);
        }
    }
    if (ResourceTransaction::depth) {
        struct ReleaseAllCommitter : Committer {
            Purpose* self;
            UniqueArray<SharedResource> reses;
            ReleaseAllCommitter (Purpose* s, UniqueArray<SharedResource>&& r) :
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

SharedResource* Purpose::find (ResourceRef res) {
    for (auto& r : resources) if (r == res) return &r;
    return null;
}

Purpose general_purpose;
Purpose* current_purpose = &general_purpose;

} // ayu
