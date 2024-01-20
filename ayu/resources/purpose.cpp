#include "purpose.h"

#include "resource.private.h"
#include "universe.private.h"

namespace ayu {

struct PushCurrentPurpose {
    Purpose* old;
    PushCurrentPurpose (Purpose* p) : old(current_purpose) {
        current_purpose = p;
    }
    ~PushCurrentPurpose () {
        current_purpose = old;
    }
};

void Purpose::acquire (Slice<Resource> rs) {
    if (!rs) return;
    PushCurrentPurpose _(this);
    load_purposeless(rs);
    for (auto& r : rs) {
        for (auto& res : resources) {
            if (res == r) goto next_r;
        }
        resources.push_back(r);
        r.data->purpose_count++;
        next_r:;
    }
}

static void release_and_unload (Purpose& self, Slice<Resource> rs) {
    PushCurrentPurpose _(&self);
    auto to_unload = UniqueArray<Resource>(Capacity(rs.size()));
    for (auto& r : rs) {
        if (!--r.data->purpose_count) {
            to_unload.push_back_expect_capacity(r);
        }
    }
    unload(to_unload);
}

void Purpose::release (Slice<Resource> rs) {
    if (!rs) return;
    for (auto& r : rs) {
        for (auto& res : resources) {
            if (res == r) goto next_r;
        }
        raise(e_ResourceNotInPurpose,
            "Cannot release Resource from Purpose that doesn't have it."
        );
        next_r:;
    }
    release_and_unload(*this, rs);
    auto new_resources = UniqueArray<Resource>(Capacity(resources.size()));
    for (auto& res : resources) {
        for (auto& r : rs) {
            if (r == res) goto next_res;
        }
        new_resources.push_back(res);
        next_res:;
    }
    resources = new_resources;
}

void Purpose::release_all () {
    release_and_unload(*this, resources);
    resources = {};
}

Purpose general_purpose;
Purpose* current_purpose = &general_purpose;

} // ayu
