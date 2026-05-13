#include "resource.private.h"
#include "../../iri/iri.h"
#include "../../uni/io.h"
#include "../data/parse.h"
#include "../data/print.h"
#include "../reflection/link.h"
#include "../reflection/anyval.h"
#include "../reflection/describe-standard.h"
#include "../reflection/describe.h"
#include "../traversal/compound.h"
#include "../traversal/from-tree.h"
#include "../traversal/scan.h"
#include "../traversal/to-tree.h"
#include "scheme.h"

///// INTERNALS

namespace ayu {
namespace in {

struct ResourcePrivate : Resource {
    AnyVal value {};
    IRI name;
    ResourceState state = RS::Unloaded;
     // These are only used during reachability scanning, but we have extra room
     // for them here.
    bool root;
    bool reachable;
     // This is also only used during reachability scanning, but storing it
     // externally would require using an unordered_map (to use a UniqueArray,
     // we need an integer index, but that's what this itself is).
    u32 node_id;
    ResourcePrivate (const IRI& n) : name(n) { }
};

[[noreturn, gnu::cold]]
static void raise_ResourceStateInvalid () {
    raise(e_ResourceStateInvalid, "Resource state is not valid for this operation");
}

[[noreturn, gnu::cold]]
static void raise_ResourceValueEmpty () {
    raise(e_ResourceValueInvalid, "Resource value is empty");
}

[[noreturn, gnu::cold]]
static void tag_error_with_resource_data (
    const SharedString& name, ResourceState state, StaticString operation
) {
    Error& e = current_error();
    e.add_tag("ayu::ResourceName", name);
    e.add_tag("ayu::ResourceOperation", operation);
    e.add_tag("ayu::ResourceState", show(&state));
    throw e;
}

[[noreturn, gnu::cold]]
static void tag_error_with_resource (ResourceRef res, StaticString operation) {
    tag_error_with_resource_data(res->name().spec(), res->state(), operation);
}

[[noreturn, gnu::cold]]
static void tag_error_with_resources (
    Slice<ResourceRef> reses, StaticString operation
) {
    Error& e = current_error();
     // Hopefully this isn't too long of an error message.
    e.add_tag("ayu::ResourceOperation", operation);
    for (u32 i = 0; i < reses.size(); i++) {
        e.add_tag(
            cat("ayu::ResourceName[", i, ']'),
            reses[i]->name().possibly_invalid_spec()
        );
        auto state = reses[i]->state();
        e.add_tag(
            cat("ayu::ResourceState[", i, ']'),
            show(&state)
        );
    }
    throw e;
}

struct Break {
    SharedRoute from;
    SharedRoute to;
};

[[noreturn, gnu::cold]]
static void raise_would_break (
    ErrorCode code, UniqueArray<Break> breaks
) {
    UniqueString mess = cat(
        (code == e_ResourceReloadWouldBreak ? "Re" : "Un"),
        "loading resources would break ", breaks.size(), " link(s): \n"
    );
    for (usize i = 0; i < breaks.size(); ++i) {
        if (i > 5) break;
        encat(mess,
            "    ", route_to_iri(breaks[i].from).spec(),
            " -> ", route_to_iri(breaks[i].to).spec(), '\n'
        );
    }
    if (breaks.size() > 5) {
        encat(mess, "    ...and ", breaks.size() - 5, " others.\n");
    }
    breaks = {};
    raise(code, move(mess));
}

struct TypeAndTree {
    Type type;
    Tree tree;
};

struct ROV {
     // Since this keeps a ref count on the resource, if unload is called with a
     // resource that has a ref count of 0 (but wasn't deleted because it was
     // loaded), then when this object is destroyed the ref count will go back
     // to 0 and the resource will be actually deleted (unless it was rolled
     // back).
    SharedResource res;
    AnyVal old_value;
    void rollback () {
        auto self = static_cast<ResourcePrivate*>(res.p.p);
        self->value = move(old_value);
        self->state = RS::Loaded;
    }
};

NOINLINE static
void delete_Resource (Resource* res) noexcept {
    auto& g_resources = g_universe->resources;
    for (auto& p : g_resources) {
        if (p.value == res) {
            g_resources.erase(&p);
            delete res;
            return;
        }
    }
    require(false);
}

 // Has to be pointer for in::RCP<>
void delete_Resource_if_unloaded (Resource* res) noexcept {
    if (currently_scanning) {
         // Last SharedResource got dropped in the middle of a scan!  We're not
         // allowed to manipulate the resource list and we can't throw an
         // exception in a destructor so uh...leak the resource data I guess.
         // TODO: find a nonintrusive way to clean these up during unload.
#ifndef NDEBUG
        warn_utf8("Warning: A resource's refcount hit 0 during a scan.  Some resource data might be leaked.\n");
#endif
        return;
    }
    if (res->state() != RS::Unloaded) return;
    delete_Resource(res);
}

} using namespace in;

///// ACCESSORS

const IRI& Resource::name () const noexcept {
    return static_cast<const ResourcePrivate*>(this)->name;
}
ResourceState Resource::state () const noexcept {
    return static_cast<const ResourcePrivate*>(this)->state;
}

AnyVal& Resource::value () {
    auto self = static_cast<ResourcePrivate*>(this);
    if (self->state == RS::Unloaded) {
        load(this);
    }
    return self->value;
}
AnyVal& Resource::get_value () noexcept {
    return static_cast<ResourcePrivate*>(this)->value;
}

void Resource::set_value (AnyVal&& value) try {
    auto self = static_cast<ResourcePrivate*>(this);
    AnyVal v = move(value);

    if (currently_scanning) raise(e_ForbiddenWhileScanning, "Cannot set_value while a scan is ongoing");

    if (self->state == RS::Loading) {
        raise_ResourceStateInvalid();
    }
    if (!v) {
        raise_ResourceValueEmpty();
    }
    if (self->name) {
        auto scheme = require_scheme(self->name);
        scheme->validate_type(v.type);
    }
    if (ResourceTransaction::depth) {
         // Don't use ROV here so we don't force a vtable onto ROV
        struct SetValueCommitter : Committer {
            SharedResource res;
            AnyVal old_value;
            SetValueCommitter (SharedResource&& r, AnyVal&& v) :
                res(move(r)), old_value(move(v))
            { }
            void rollback () noexcept override {
                auto self = static_cast<ResourcePrivate*>(res.p.p);
                self->value = move(old_value);
                self->state = self->value ? RS::Loaded : RS::Unloaded;
            }
        };
        ResourceTransaction::add_committer(
            new SetValueCommitter(this, move(self->value))
        );
    }
    self->value = move(v);
    self->state = RS::Loaded;
} catch (...) {
    tag_error_with_resource(this, "set_value");
}

Link Resource::operator[] (const SharedString& key) { return link()[key]; }
Link Resource::operator[] (u32 index) { return link()[index]; }

///// CONSTRUCTION, DESTRUCTION

SharedResource::SharedResource (const IRI& name) {
    try {
        if (!name) {
            raise(e_ResourceNameInvalid, "Invalid IRI provided as resource name");
        }
        else if (name.has_fragment()) {
            raise(e_ResourceNameInvalid, "Resource name cannot have a #fragment");
        }
        auto scheme = require_scheme(name);
        scheme->validate_name(name);
    }
    catch (...) {
        tag_error_with_resource_data(
            name.possibly_invalid_spec(), RS::Unloaded, "construct"
        );
    }
    Str spec = expect(name.spec());
    expect(spec.begin() < spec.end());
    usize hash = uni::hash(spec);

     // See if this resource already exists
    for (auto [h, r] : g_universe->resources) {
        if (h == hash && r->name().spec_ == spec) {
            new (this) SharedResource(r);
            return;
        }
    }
     // Create new resource data
    if (currently_scanning) raise(e_ForbiddenWhileScanning, "Cannot construct new Resource while a scan is ongoing");
    new (this) SharedResource(new ResourcePrivate(name));
    g_universe->resources.emplace_back(hash, *this);
}

SharedResource::SharedResource (const IRI& name, AnyVal&& value) :
    SharedResource(name)
{
    auto self = static_cast<ResourcePrivate*>(p.p);
    AnyVal v = move(value);
    try {
        if (self->state != RS::Unloaded) raise_ResourceStateInvalid();
    }
    catch (...) {
        tag_error_with_resource(*this, "construct with value");
    }
    self->set_value(move(v));
}

///// RESOURCE OPERATIONS

static void load_inner (ResourcePrivate* self) {
    auto scheme = require_scheme(self->name);
    auto ext = require_extension(self->name);
    auto path = scheme->require_filepath(self->name);
    UniqueArray<u8> blob = blob_from_file(path);
    expect(!self->value);
    ext->from_blob(self->value, blob, self, scheme);
#ifndef NDEBUG
    expect(scheme->accepts_type(self->value.type));
    expect(ext->accepts_type(self->value.type));
#endif
}

static void load_cancel (ResourceRef res) {
    auto self = static_cast<ResourcePrivate*>(res.p);
    self->value = {};
    self->state = RS::Unloaded;
}

void load (ResourceRef res) try {
    if (currently_scanning) raise(e_ForbiddenWhileScanning, "Cannot load Resource while a scan is ongoing");

    auto self = static_cast<ResourcePrivate*>(res.p);
    if (self->state != RS::Unloaded) return;
    self->state = RS::Loading;

    load_inner(self);

    if (ResourceTransaction::depth) {
        struct LoadCommitter : Committer {
            SharedResource res;
            LoadCommitter (SharedResource&& r) : res(move(r)) { }
            void rollback () noexcept override {
                load_cancel(res);
            }
        };
        ResourceTransaction::add_committer(
            new LoadCommitter(res)
        );
    }
    self->state = RS::Loaded;
} catch (...) {
    load_cancel(res);
    tag_error_with_resource(res, "load");
}

void save (ResourceRef res, PrintOptions opts) try {
    if (currently_scanning) raise(e_ForbiddenWhileScanning, "Cannot save Resource while a scan is ongoing");

    auto self = static_cast<ResourcePrivate*>(res.p);
    if (self->state != RS::Loaded) raise_ResourceStateInvalid();
    if (!self->value) raise_ResourceValueEmpty();

    KeepRouteCache klc;

    auto scheme = require_scheme(self->name);
    auto ext = require_extension(self->name);
    scheme->validate_type(self->value.type);
    ext->validate_type(self->value.type);
    auto path = scheme->require_filepath(self->name);
    auto blob = ext->to_blob(self->value, res, opts);

    auto outfile = File(path, "wb");
    if (ResourceTransaction::depth) {
        struct SaveCommitter : Committer {
            UniqueArray<u8> blob;
            SharedString path;
            File outfile;
            SaveCommitter (UniqueArray<u8>&& b, SharedString&& p, File&& f) :
                blob(move(b)), path(move(p)), outfile(move(f))
            { }
            void commit () noexcept override try {
                 // TODO: propagate this error somehow?  Retry?
                outfile.write(Str(blob));
            } catch (...) { unrecoverable_exception("when committing a write to a file"); }
        };
        ResourceTransaction::add_committer(
            new SaveCommitter(move(blob), move(path), move(outfile))
        );
    }
    else try {
        outfile.write(Str(blob));
        outfile.close();
    } catch (Error& e) { e.set_tag("uni::FilePath", path); throw; }
} catch (...) {
    tag_error_with_resource(res, "save");
}

static void really_unload (ResourcePrivate* self) {
    if (ResourceTransaction::depth) {
        struct ForceUnloadCommitter : Committer {
            ROV rov;
            ForceUnloadCommitter (ROV&& r) : rov(move(r)) { }
            void rollback () noexcept override {
                rov.rollback();
            }
        };
        ResourceTransaction::add_committer(
            new ForceUnloadCommitter({self, move(self->value)})
        );
        self->state = RS::Unloaded;
    }
    else {
        self->value = {};
        self->state = RS::Unloaded;
        if (!self->ref_count) {
            delete_Resource_if_unloaded(self);
        }
    }
}

struct ResourceScanInfo {
    ResourcePrivate* res;
    UniqueArray<Link> outgoing_links;
};
 // TODO: replace with binary search
using LinksToReses = std::unordered_map<Link, ResourcePrivate*>;

static void reach_link (
    const UniqueArray<ResourceScanInfo>& scan_info,
    const LinksToReses& links_to_reses,
    const Link& item
) {
    auto it = links_to_reses.find(item);
    if (it == links_to_reses.end()) {
         // Link is already invalid?  Either that or it points to the root set,
         // which we didn't bother studying because we already know it's
         // reachable.
        return;
    }
    auto to = it->second;
    if (to->reachable) return;
    to->reachable = true;
    for (auto& link : scan_info[to->node_id].outgoing_links) {
        reach_link(scan_info, links_to_reses, link);
    }
}

void unload (Slice<ResourceRef> to_unload) try {
    if (currently_scanning) raise(e_ForbiddenWhileScanning, "Cannot unload Resource while a scan is ongoing");

    auto& resources = g_universe->resources;
     // TODO: Track how many loaded resources there are to preallocate this.
    auto scan_info = UniqueArray<ResourceScanInfo>(Capacity(resources.size()));
     // Start out by getting a bit of info about all loaded resources.
    bool none_root = true;
    bool all_root = true;
    for (auto [h, res] : resources) {
        auto self = static_cast<ResourcePrivate*>(res.p);
         // Only scan loaded resources
        if (self->state != RS::Loaded) continue;
         // Assign integer ID for indexing
        self->node_id = scan_info.size();
        scan_info.emplace_back_expect_capacity(self, UniqueArray<Link>());
         // Our root set for the reachability traversal is all resources that
         // have a reference count but were not explicitly requested to be
         // unloaded.
        if (self->ref_count) {
            self->root = true;
            for (auto& tu : to_unload) {
                if (tu == self) {
                    self->root = false;
                    break;
                }
            }
        }
        else self->root = false;
        if (self->root) none_root = false;
        else all_root = false;
        self->reachable = false;
    }
    if (all_root) {
         // All resources are still in use and no resources were requested to be
         // unloaded.  Everyone can go home.
        return;
    }
    if (none_root && !g_universe->tracked) {
         // Root set is empty!  We get to skip reachability scanning and just
         // unload everything.
        scan_info.consume([](auto&& info){ really_unload(info.res); });
        return;
    }
     // Collect as much info as we can from one scan.  Unfortunately we can't
     // traverse the program data graph directly, because finding out what
     // Resource a link points to requires a full scan itself.  We don't have to
     // cache as much as link_to_route though; we only need to keep track of the
     // Route's root, not the whole Route itself.
    auto links_to_reses = std::unordered_map<Link, ResourcePrivate*>();
    for (auto& info : scan_info) {
         // TODO: Don't generate routes if we're throwing them away?
        scan_resource_links(info.res,
            [&links_to_reses, &info](const Link& item, RouteRef)
        {
             // Don't need to enumerate links for resources in the root set,
             // because they start out reachable.
            if (!info.res->root) {
                links_to_reses.emplace(item, info.res);
            }
            item.read([&info](Type t, Mu* v){
                if (t == Type::of<Link>()) {
                    info.outgoing_links.emplace_back(
                        *reinterpret_cast<Link*>(v)
                    );
                }
            });
            return false;
        });
    }
     // Now traverse the graph starting with the tracked items and roots.
    for (auto& g : g_universe->tracked) {
        scan_links(
            g, {},
            [&scan_info, links_to_reses](const Link& item, RouteRef)
        {
            if (item.type() == Type::of<Link>()) {
                reach_link(scan_info, links_to_reses, item);
            }
            return false;
        });
    }
    for (auto& info : scan_info) {
        if (info.res->root) {
            info.res->reachable = true;
            for (auto& link : scan_info[info.res->node_id].outgoing_links) {
                reach_link(scan_info, links_to_reses, link);
            }
        }
    }
     // At this point, all resources should be marked whether they're reachable.
     // First throw an error if any resources we were explicitly told to unload
     // are still reachable.
    for (auto res : to_unload) {
        auto self = static_cast<ResourcePrivate*>(res.p);
        if (self->reachable) {
            raise(e_ResourceUnloadWouldBreak, cat(
                "Can't unload ", self->name.spec(),
                " because it is still reachable.  Further info NYI."
            ));
        }
    }
     // Now finally unload all unreachable resources.
    for (auto& info : scan_info) {
        if (!info.res->reachable) really_unload(info.res);
    }
} catch (...) {
    tag_error_with_resources(to_unload, "unload");
}

void force_unload (ResourceRef res) noexcept {
    require(!currently_scanning);
    auto self = static_cast<ResourcePrivate*>(res.p);
    if (self->state == RS::Unloaded) return;
    require(self->state != RS::Loading);
    really_unload(self);
}

struct Update {
    Link link2link;
    Link new_link;
};

NOINLINE static void reload_commit (UniqueArray<Update>&& updates) {
    updates.consume([](Update&& update){
        update.link2link.write(
            AccessCB(move(update), [](Update&& update, Type t, Mu* v){
                expect(t == Type::of<Link>());
                reinterpret_cast<Link&>(*v) = move(update.new_link);
            })
        );
    });
}

NOINLINE static void reload_rollback (UniqueArray<ROV>&& rovs) {
    rovs.consume([](auto&& rov){
        auto self = static_cast<ResourcePrivate*>(rov.res.p.p);
        self->value = move(rov.old_value);
    });
}

void reload (Slice<ResourceRef> to_reload) try {
    if (currently_scanning) raise(e_ForbiddenWhileScanning, "Cannot reload Resource while a scan is ongoing");
     // Some obscure bugs can occur if you do this.  Don't do it.
    if (currently_running_from_tree()) raise(e_General, "Cannot reload during a from_tree operation");

    UniqueArray<ROV> rovs;
    for (auto res : to_reload) {
        if (res->state() == RS::Loaded) {
            rovs.push_back({res, {}});
        }
        else raise_ResourceStateInvalid();
    }
     // Preserve step
    for (auto& rov : rovs) {
        auto self = static_cast<ResourcePrivate*>(rov.res.p.p);
        rov.old_value = move(self->value);
    }

    UniqueArray<Update> updates;
    try {
         // Construct step
         // TODO: Start ResourceTransaction for dependently-loaded resources.
        for (auto res : to_reload) {
            auto self = static_cast<ResourcePrivate*>(res.p);
            self->state = RS::Loading;
            load_inner(self);
            self->state = RS::Loaded;
        }
         // Verify step
        UniqueArray<ResourceRef> others;
        for (auto [h, other] : g_universe->resources) {
            switch (other->state()) {
                case RS::Unloaded: continue;
                case RS::Loaded: others.emplace_back(other); break;
                default: raise(e_General, "Another resource is currently loading");
            }
            for (auto res : to_reload) {
                if (res == other) goto next_other;
            }
            next_other:;
        }
        if (others || g_universe->tracked) {
             // First build mapping of old links to locations
            std::unordered_map<Link, SharedRoute> old_links;
            for (auto& rov : rovs) {
                scan_links(
                    rov.old_value.ptr(), SharedRoute(rov.res),
                    [&old_links](const Link& link, RouteRef rt) {
                        old_links.emplace(link, rt);
                        return false;
                    }
                );
            }
             // Then build set of link-links to update.
            UniqueArray<Break> breaks;
            auto check_link =
                [&updates, &old_links, &breaks](Link link2link, RouteRef rt)
            {
                 // TODO: check for AnyPtr as well for a shortcut?
                if (link2link.type() != Type::of<Link>()) return false;
                Link link = link2link.get_as<Link>();
                auto iter = old_links.find(link);
                if (iter == old_links.end()) return false;
                try {
                    Link new_link = link_from_route(iter->second);
                    updates.emplace_back(move(link2link), move(new_link));
                }
                catch (std::exception&) {
                    breaks.emplace_back(rt, iter->second);
                }
                return false;
            };
            for (auto tracked : g_universe->tracked) {
                scan_links(tracked, {}, check_link);
            }
            for (auto other : others) {
                scan_resource_links(other, check_link);
            }
            if (breaks) {
                raise_would_break(e_ResourceReloadWouldBreak, move(breaks));
            }
        }
    }
    catch (...) {
        reload_rollback(move(rovs));
        expect(!rovs);
        throw;
    }
     // Commit step.  TODO: Update links now and roll them back if necessary
    if (ResourceTransaction::depth) {
        struct ReloadCommitter : Committer {
            UniqueArray<ROV> rovs;
            UniqueArray<Update> updates;
            ReloadCommitter (UniqueArray<ROV>&& r, UniqueArray<Update>&& u) :
                rovs(move(r)), updates(move(u))
            { }
            void commit () noexcept override {
                reload_commit(move(updates));
                expect(!updates);
            }
            void rollback () noexcept override {
                reload_rollback(move(rovs));
                expect(!rovs);
            }
        };
        ResourceTransaction::add_committer(
            new ReloadCommitter(move(rovs), move(updates))
        );
    }
    else {
        reload_commit(move(updates));
        expect(!updates);
    }
}
catch (...) {
    tag_error_with_resources(to_reload, "reload");
}

void rename (ResourceRef old_res, ResourceRef new_res) try {
    auto old_self = static_cast<ResourcePrivate*>(old_res.p);
    auto new_self = static_cast<ResourcePrivate*>(new_res.p);
    if (old_self->state != RS::Loaded) {
        raise_ResourceStateInvalid();
    }
    if (new_self->state != RS::Unloaded) {
        raise_ResourceStateInvalid();
    }
    expect(!new_self->value);
    new_self->value = move(old_self->value);
    new_self->state = RS::Loaded;
    old_self->state = RS::Unloaded;
} catch (...) {
    tag_error_with_resources({old_res, new_res}, "rename");
}

SharedString resource_filepath (const IRI& name) try {
    auto scheme = require_scheme(name);
    return scheme->get_filepath(name);
} catch (Error& e) {
    e.add_tag("ayu::ResourceName", name.possibly_invalid_spec());
    throw e;
}

void delete_source (const IRI& name) try {
    auto scheme = require_scheme(name);
    auto path = scheme->require_filepath(name);
    remove_utf8(path.c_str());
} catch (Error& e) {
    e.add_tag("ayu::ResourceName", name.possibly_invalid_spec());
    throw e;
}

bool source_exists (const IRI& name) try {
    auto scheme = require_scheme(name);
    auto path = scheme->require_filepath(name);
    if (std::FILE* f = fopen_utf8(path.c_str())) {
        fclose(f);
        return true;
    }
    else return false;
} catch (Error& e) {
    e.add_tag("ayu::ResourceName", name.possibly_invalid_spec());
    throw e;
}

UniqueArray<SharedResource> loaded_resources () noexcept {
    UniqueArray<SharedResource> r;
    for (auto [hash, res] : g_universe->resources)
    if (res->state() != RS::Unloaded) {
        r.push_back(res);
    }
    return r;
}

///// TRACKED ITEMS

namespace in {

void track_ptr (AnyPtr item) noexcept {
    expect(item);
#ifndef NDEBUG
    for (auto& g : g_universe->tracked) {
        expect(g != item);
    }
#endif
    g_universe->tracked.push_back(item);
}

void untrack_ptr (AnyPtr item) noexcept {
    auto& gs = g_universe->tracked;
    for (auto& g : gs) {
        if (g == item) {
            gs.erase(&g);
            return;
        }
    }
#ifndef NDEBUG
    never();
#endif
}

Link track_ptr (AnyPtr item, const IRI& loc) {
    Link r = link_from_iri(loc);
    track_ptr(item);
    return r;
}

} // in

} using namespace ayu;

///// DESCRIPTIONS

AYU_DESCRIBE(ayu::ResourceState,
    values(
        value("unloaded", RS::Unloaded),
        value("loading", RS::Loading),
        value("loaded", RS::Loaded)
    )
)
AYU_DESCRIBE(ayu::SharedResource,
    delegate(funcs(
        [](const SharedResource& v) -> const IRI& {
             // TODO: Make relative to current resource?
            return v->name();
        },
        [](SharedResource& v, const IRI& m){
            v = SharedResource(m);
        }
    ))
)
AYU_DESCRIBE(ayu::ResourceRef,
    delegate(const_ref_func<IRI>(
        [](const ResourceRef& v) -> const IRI& {
            return v->name();
        }
    ))
)

///// TESTS

#ifndef TAP_DISABLE_TESTS
#include "../test/test-environment.h"
#include "collection.h"

AYU_DESCRIBE_INSTANTIATE(std::vector<i32*>)

namespace ayu::in {
struct TestResourceExtension : ResourceExtension {
    bool accepts_type (Type type) override {
        return type == Type::of<Collection>();
    }
    using ResourceExtension::ResourceExtension;
};
} // ayu::in

static tap::TestSet tests ("dirt/ayu/resources/resource", []{
    using namespace tap;
    using namespace iri::literals;

    test::TestEnvironment env;

     // Someone else may have registered an extension
    if (auto ext = get_extension("ayu-test:/foo.ayu"_iri)) {
        ext->deactivate();
    }
    if (auto ext = get_extension("ayu-test:/foo.ayutest"_iri)) {
        ext->deactivate();
    }
    auto ayu_ext = ResourceExtension("ayu");
    auto ayutest_ext = TestResourceExtension("ayutest");

    SharedResource input ("ayu-test:/testfile.ayu"_iri);
    SharedResource input2 ("ayu-test:/othertest.ayu"_iri);
    SharedResource rec1 ("ayu-test:/rec1.ayu"_iri);
    SharedResource rec2 ("ayu-test:/rec2.ayu"_iri);
    SharedResource badinput ("ayu-test:/badref.ayu"_iri);
    SharedResource output ("ayu-test:/test-output.ayu"_iri);
    SharedResource unicode ("ayu-test:/ユニコード.ayu"_iri);
    SharedResource unicode2 ("ayu-test:/ユニコード2.ayu"_iri);

    is(input->state(), RS::Unloaded, "Resources start out unloaded");
    doesnt_throw([&]{ load(input); }, "load");
    is(input->state(), RS::Loaded, "Resource state is RS::Loaded after loading");
    ok(!!input->value(), "Resource has value after loading");

    throws_code<e_ResourceStateInvalid>([&]{
        SharedResource(input->name(), AnyVal::make<int>(3));
    }, "Creating resource throws on duplicate");

    doesnt_throw([&]{ unload(input); }, "unload");
    is(input->state(), RS::Unloaded, "Resource state is RS::Unloaded after unloading");
    ok(!input->get_value(), "Resource has no value after unloading");

    ayu::Collection* coll = null;
    doesnt_throw([&]{
        coll = &input->value().as<ayu::Collection>();
    }, "Getting typed value from a resource");
    is(input->state(), RS::Loaded, "Resource::value() automatically loads resource");
    is(input["foo"][1].get_as<i32>(), 4, "Value was generated properly (0)");
    is(input["bar"][1].get_as<std::string>(), "qux", "Value was generated properly (1)");

    throws_code<e_ResourceStateInvalid>([&]{ save(output); }, "save throws on unloaded resource");

    coll->delete_(coll->find_with_name("foo")->ptr());
    coll->new_with_name<i32>("asdf", 51);

    doesnt_throw([&]{ rename(input, output); }, "rename");
    is(input->state(), RS::Unloaded, "Old resource is RS::Unloaded after renaming");
    is(output->state(), RS::Loaded, "New resource is RS::Loaded after renaming");
    is(&output->value().as<ayu::Collection>(), coll, "Rename moves value without reconstructing it");

    doesnt_throw([&]{ save(output); }, "save");
    is(tree_from_file(resource_filepath(output->name())), tree_from_string(
        "[ayu::Collection {bar:[std::string qux] asdf:[i32 51]}]"
    ), "Resource was saved with correct contents");
    ok(source_exists(output->name()), "source_exists returns true before deletion");
    doesnt_throw([&]{ delete_source(output->name()); }, "delete_source");
    ok(!source_exists(output->name()), "source_exists returns false after deletion");
    throws_code<e_IOError>([&]{
        tree_from_file(resource_filepath(output->name()));
    }, "Can't open file after calling delete_source");
    doesnt_throw([&]{ delete_source(output->name()); }, "Can call delete_source twice");
    SharedRoute rt;
    doesnt_throw([&]{
        item_from_string(&rt, cat('"', input->name().spec(), "#/bar+1\""));
    }, "Can read route from tree");
    Link link;
    doesnt_throw([&]{
        link = link_from_route(rt);
    }, "link_from_route");
    doesnt_throw([&]{
        is(link.get_as<std::string>(), "qux", "link_from_route got correct item");
    });
    coll = &output->value().as<ayu::Collection>();
    link = output["asdf"][1].address_as<i32>();
    doesnt_throw([&]{
        rt = link_to_route(link);
    });
    is(item_to_tree(&rt), tree_from_string("\"ayu-test:/test-output.ayu#/asdf+1\""), "link_to_route works");
    coll->new_<Link>(output["bar"][1]);
    doesnt_throw([&]{ save(output); }, "save with link");
    coll->new_<i32*>(output["asdf"][1]);
    doesnt_throw([&]{ save(output); }, "save with pointer");
    is(tree_from_file(resource_filepath(output->name())), tree_from_string(
        "[ayu::Collection {bar:[std::string qux] asdf:[i32 51] _0:[ayu::Link #/bar+1] _1:[i32* #/asdf+1] _next_id:2}]"
    ), "File was saved with correct link as route");
    throws_code<e_IOError>([&]{
        load(badinput);
    }, "Can't load file with incorrect link in it");

    doesnt_throw([&]{
        unload(input);
        load(input2);
    }, "Can load second file linked to first");
    is(input->state(), RS::Loaded, "Loading second file linked to first file loads first file");
    std::string* bar;
    doesnt_throw([&]{
        bar = input["bar"][1];
    }, "can use [] syntax on resources and links");
    is(
        input2["ext_pointer"][1].get_as<std::string*>(),
        bar,
        "Loading a pointer worked!"
    );

    int asdf = 0;
    doesnt_throw([&]{
        asdf = *unicode["ptr"][1].get_as<int*>();
    }, "Can load and reference files with unicode in their name");
    is(asdf, 4444);

    is(
        unicode2["self_pointer"][1].get_as<std::string*>(),
        unicode2["val"][1].address_as<std::string>(),
        "Loading pointer with \"#\" for own file worked."
    );
    throws_code<e_ResourceUnloadWouldBreak>([&]{
        unload(input);
    }, "Can't unload resource when there are link to it");
    doesnt_throw([&]{
        unload(input2);
        unload(input);
    }, "Can unload if we unload the linking resource first");
    doesnt_throw([&]{
        load(rec1);
    }, "Can load resources with link cycle");
    throws_code<e_ResourceUnloadWouldBreak>([&]{
        unload(rec1);
    }, "Can't unload part of a link cycle 1");
    throws_code<e_ResourceUnloadWouldBreak>([&]{
        unload(rec2);
    }, "Can't unload part of a link cycle 2");
    doesnt_throw([&]{
        unload({rec1, rec2});
    }, "Can unload link cycle by unload both resources at once");
     // TODO: test that calling unload unloads dependent resources if there are
     // no SharedResource handles pointing to them.

    load(rec1);
    int* old_p = rec1["link"][1].get_as<int*>();
    int* global_p = old_p;
    ayu::track(global_p);

    doesnt_throw([&]{
        reload(rec2);
    }, "Can reload file with links to it");
    int* new_p = rec1["link"][1].get_as<int*>();
    isnt(new_p, old_p, "Link to reloaded file was updated");
    is(global_p, new_p, "Global was updated.");

     ///// EXTENSIONS (TODO: more testing)

    SharedResource good_ayutest ("ayu-test:/good.ayutest"_iri);
    SharedResource bad_ayutest ("ayu-test:/bad.ayutest"_iri);

    doesnt_throw([&]{
        load(good_ayutest);
    }, "ayutest extension works");

    throws_code<e_ResourceTypeRejected>([&]{
        load(bad_ayutest);
    }, "ResourceExtension can reject type");

    ayutest_ext.deactivate();

    doesnt_throw([&]{
        unload(good_ayutest);
    }, "Can unload resource without registered extension");

    throws_code<e_ResourceExtensionNotFound>([&]{
        load(good_ayutest);
    }, "Cannot load resource without registered extension");

     ///// SCHEMES (TODO: more testing)

    throws_code<e_ResourceTypeRejected>([&]{
        load(SharedResource("ayu-test:/wrongtype.ayu"_iri));
    }, "ResourceScheme::accepts_type rejects wrong type");

    SharedString ordinary_path;
    throws_code<e_ResourceSchemeNotFound>([&]{
        ordinary_path = resource_filepath("file:/foo/bar"_iri);
    }, "Can't use file:/ resource when there's a scheme registered.");
     // Test the default scheme behavior.  Someone else may have registered
     // schemes, so deregister them all.
    while (g_universe->schemes) {
        g_universe->schemes[0].scheme->deactivate();
    }
    doesnt_throw([&]{
        ordinary_path = resource_filepath("file:/foo/bar"_iri);
    }, "Can use file:/ resource when there are no schemes registered.");
    is(ordinary_path, "/foo/bar", "file:/ IRI gives correct filepath");

    done_testing();
});
#endif
