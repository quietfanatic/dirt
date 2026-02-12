#include "resource.h"
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
#include "resource-scheme.h"
#include "universe.private.h"

///// INTERNALS

namespace ayu {
namespace in {

 // ResourceData is in universe.private.h for reasons

[[noreturn, gnu::cold]]
static void raise_ResourceStateInvalid () {
    raise(e_ResourceStateInvalid, "Resource state is not valid for this operation");
}

[[noreturn, gnu::cold]]
static void raise_ResourceValueEmpty () {
    raise(e_ResourceValueInvalid, "Resource value is empty");
}

[[noreturn, gnu::cold]]
static void wrap_error_for_resource_unconstructed (
    AnyString name, ResourceState state, StaticString operation
) {
    try { throw; }
    catch (Error& e) {
        e.add_tag("ayu::ResourceName", name);
        e.add_tag("ayu::ResourceOperation", operation);
        e.add_tag("ayu::ResourceState", show(&state));
        throw e;
    }
    catch (std::exception& ex) {
        Error e;
        e.code = e_External;
        e.details = ex.what();
        e.external = std::current_exception();
        try { throw e; }
        catch (Error& e) {
            wrap_error_for_resource_unconstructed(name, state, operation);
        }
        catch (...) { never(); }
    }
}

[[noreturn, gnu::cold]]
static void wrap_error_for_resource (ResourceRef res, StaticString operation) {
    wrap_error_for_resource_unconstructed(res->name().spec(), res->state(), operation);
}

[[noreturn, gnu::cold]]
static void wrap_error_for_resources (
    Slice<ResourceRef> reses, StaticString operation
) {
    try { throw; }
    catch (Error& e) {
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
        throw;
    }
    catch (std::exception& ex) {
        Error e;
        e.code = e_External;
        e.details = ex.what();
        e.external = std::current_exception();
        try { throw e; }
        catch (Error& e) {
            wrap_error_for_resources(reses, operation);
        }
        catch (...) { never(); }
    }
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

static TypeAndTree verify_tree_for_scheme (
    ResourceScheme* scheme,
    const Tree& tree
) {
    if (tree.form == Form::Null) {
        raise_ResourceValueEmpty();
    }
    auto a = Slice<Tree>(tree);
    if (a.size() == 2) {
        Type type = Type(Str(a[0]));
        scheme->validate_type(type);
        return {type, a[1]};
    }
    else raise_LengthRejected(Type::For<AnyVal>(), 2, 2, a.size());
}

struct ROV {
     // Since this keeps a ref count on the ResourceData, if unload is called
     // with a ResourceData that has a ref count of 0 (but wasn't deleted
     // because it was loaded), then when this object is destroyed the ref count
     // will go back to 0 and the ResourceData will be actually deleted (unless
     // it was rolled back).
    SharedResource res;
    AnyVal old_value;
    void rollback () {
        auto data = static_cast<ResourceData*>(res.data.p);
        data->value = move(old_value);
        data->state = RS::Loaded;
    }
};

} using namespace in;

///// ACCESSORS

const IRI& Resource::name () const noexcept {
    return static_cast<const ResourceData*>(this)->name;
}
ResourceState Resource::state () const noexcept {
    return static_cast<const ResourceData*>(this)->state;
}

AnyVal& Resource::value () {
    auto data = static_cast<ResourceData*>(this);
    if (data->state == RS::Unloaded) {
        load(ResourceRef(this));
    }
    return data->value;
}
AnyVal& Resource::get_value () noexcept {
    return static_cast<ResourceData*>(this)->value;
}

void Resource::set_value (AnyVal&& value) {
    auto data = static_cast<ResourceData*>(this);
    AnyVal v = move(value);
    try {
        if (data->state == RS::Loading) {
            raise_ResourceStateInvalid();
        }
        if (!v) {
            raise_ResourceValueEmpty();
        }
        if (data->name) {
            auto scheme = g_universe->require_scheme(data->name);
            scheme->validate_type(v.type);
        }
    }
    catch (...) {
        wrap_error_for_resource(this, "load");
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
                auto data = static_cast<ResourceData*>(res.data.p);
                data->value = move(old_value);
                data->state = data->value ? RS::Loaded : RS::Unloaded;
            }
        };
        ResourceTransaction::add_committer(
            new SetValueCommitter(this, move(data->value))
        );
    }
    data->value = move(v);
    data->state = RS::Loaded;
}

Link Resource::operator[] (const AnyString& key) { return link()[key]; }
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
        auto scheme = g_universe->require_scheme(name);
        scheme->validate_name(name);
    }
    catch (...) {
        wrap_error_for_resource_unconstructed(
            name.possibly_invalid_spec(), RS::Unloaded, "construct"
        );
    }
    new (this) SharedResource(g_universe->get_resource(name));
}

SharedResource::SharedResource (const IRI& name, AnyVal&& value) :
    SharedResource(name)
{
    AnyVal v = move(value);
    try {
        if (data->state() != RS::Unloaded) raise_ResourceStateInvalid();
    }
    catch (...) {
        wrap_error_for_resource(*this, "construct with value");
    }
    data->set_value(move(v));
}

void in::delete_Resource_if_unloaded (Resource* res) noexcept {
    auto data = static_cast<ResourceData*>(res);
    if (data->state == RS::Unloaded) {
        g_universe->delete_resource(res);
    }
}

///// RESOURCE OPERATIONS

static void load_cancel (ResourceRef res) {
    auto data = static_cast<ResourceData*>(res.data);
    data->value = {};
    data->state = RS::Unloaded;
}

void load (ResourceRef res) try {
    auto data = static_cast<ResourceData*>(res.data);
    if (data->state != RS::Unloaded) return;
    data->state = RS::Loading;

    auto scheme = g_universe->require_scheme(data->name);
    auto path = scheme->require_filepath(data->name);
    Tree tree = tree_from_file(move(path));
    auto tnt = verify_tree_for_scheme(scheme, tree);
     // Run item_from_tree on the AnyVal's value, not on the AnyVal
     // itself.  Otherwise, the associated locations will have an extra +1
     // in the fragment.
    expect(!data->value);
    data->value = AnyVal(tnt.type);
    item_from_tree(
        data->value.ptr(), tnt.tree, SharedRoute(res),
        FromTreeOptions::DelaySwizzle
    );

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
    data->state = RS::Loaded;

} catch (...) {
    load_cancel(res);
    wrap_error_for_resource(res, "load");
}

void save (ResourceRef res, PrintOptions opts) try {
    auto data = static_cast<ResourceData*>(res.data);
    if (data->state != RS::Loaded) raise_ResourceStateInvalid();
    if (!data->value) raise_ResourceValueEmpty();
    auto scheme = g_universe->require_scheme(data->name);
    scheme->validate_type(data->value.type);
    auto path = scheme->require_filepath(data->name);
     // Do type and value separately, because the Route refers to the value,
     // not the whole AnyVal.
    KeepRouteCache klc;
    auto type = data->value.type.name();
    auto value_tree = item_to_tree(data->value.ptr(), SharedRoute(res));
    auto contents = tree_to_string_for_file(
        Tree::array(Tree(type), move(value_tree)), opts
    );

    auto outfile = File(path, "wb");
    if (ResourceTransaction::depth) {
        struct SaveCommitter : Committer {
            AnyString contents;
            AnyString path;
            File outfile;
            SaveCommitter (AnyString&& c, AnyString&& p, File&& f) :
                contents(move(c)), path(move(p)), outfile(move(f))
            { }
            void commit () noexcept override {
                outfile.write(contents, path);
            }
        };
        ResourceTransaction::add_committer(
            new SaveCommitter(move(contents), move(path), move(outfile))
        );
    }
    else {
        outfile.write(contents, path);
    }
} catch (...) {
    wrap_error_for_resource(res, "save");
}

static void really_unload (ResourceData* data) {
    if (ResourceTransaction::depth) {
        struct ForceUnloadCommitter : Committer {
            ROV rov;
            ForceUnloadCommitter (ROV&& r) : rov(move(r)) { }
            void rollback () noexcept override {
                rov.rollback();
            }
        };
        ResourceTransaction::add_committer(
            new ForceUnloadCommitter({data, move(data->value)})
        );
        data->state = RS::Unloaded;
    }
    else {
        data->value = {};
        if (!data->ref_count) {
            g_universe->delete_resource(data);
        }
        else data->state = RS::Unloaded;
    }
}

struct ResourceScanInfo {
    ResourceData* data;
    UniqueArray<Link> outgoing_links;
};
 // TODO: replace with binary search
using LinksToReses = std::unordered_map<Link, ResourceData*>;

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
    auto to_data = it->second;
    if (to_data->reachable) return;
    to_data->reachable = true;
    for (auto& link : scan_info[to_data->node_id].outgoing_links) {
        reach_link(scan_info, links_to_reses, link);
    }
}

void unload (Slice<ResourceRef> to_unload) try {
    auto& resources = g_universe->resources;
     // TODO: Track how many loaded resources there are to preallocate this.
    auto scan_info = UniqueArray<ResourceScanInfo>(Capacity(resources.size()));
     // Start out by getting a bit of info about all loaded resources.
    bool none_root = true;
    bool all_root = true;
    for (auto& [name, res] : resources) {
        auto data = static_cast<ResourceData*>(res.data);
         // Only scan loaded resources
        if (data->state != RS::Loaded) continue;
         // Assign integer ID for indexing
        data->node_id = scan_info.size();
        scan_info.emplace_back_expect_capacity(data, UniqueArray<Link>());
         // Our root set for the reachability traversal is all resources that
         // have a reference count but were not explicitly requested to be
         // unloaded.
        if (data->ref_count) {
            data->root = true;
            for (auto& tu : to_unload) {
                if (tu == res) {
                    data->root = false;
                    break;
                }
            }
        }
        else data->root = false;
        if (data->root) none_root = false;
        else all_root = false;
        data->reachable = false;
    }
    if (all_root) {
         // All resources are still in use and no resources were requested to be
         // unloaded.  Everyone can go home.
        return;
    }
    if (none_root && !g_universe->tracked) {
         // Root set is empty!  We get to skip reachability scanning and just
         // unload everything.
        scan_info.consume([](auto&& info){ really_unload(info.data); });
        return;
    }
     // Collect as much info as we can from one scan.  Unfortunately we can't
     // traverse the data graph directly, because finding out what Resource a
     // link points to requires a full scan itself.  We don't have to cache as
     // much data as link_to_route though; we only need to keep track of the
     // Route's root, not the whole Route itself.
    auto links_to_reses = std::unordered_map<Link, ResourceData*>();
    for (auto& info : scan_info) {
         // TODO: Don't generate routes if we're throwing them away
        scan_resource_links(info.data,
            [&links_to_reses, &info](const Link& item, RouteRef)
        {
             // Don't need to enumerate links for resources in the root set,
             // because they start out reachable.
            if (!info.data->root) {
                links_to_reses.emplace(item, info.data);
            }
            item.read([&info](Type t, Mu* v){
                if (t == Type::For<Link>()) {
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
            if (item.type() == Type::For<Link>()) {
                reach_link(scan_info, links_to_reses, item);
            }
            return false;
        });
    }
    for (auto& info : scan_info) {
        if (info.data->root) {
            info.data->reachable = true;
            for (auto& link : scan_info[info.data->node_id].outgoing_links) {
                reach_link(scan_info, links_to_reses, link);
            }
        }
    }
     // At this point, all resources should be marked whether they're reachable.
     // First throw an error if any resources we were explicitly told to unload
     // are still reachable.
    for (auto res : to_unload) {
        auto data = static_cast<ResourceData*>(res.data);
        if (data->reachable) {
            raise(e_ResourceUnloadWouldBreak, cat(
                "Can't unload ", data->name.spec(),
                " because it is still reachable.  Further info NYI."
            ));
        }
    }
     // Now finally unload all unreachable resources.
    for (auto& info : scan_info) {
        if (!info.data->reachable) really_unload(info.data);
    }
} catch (...) {
    wrap_error_for_resources(to_unload, "unload");
}

void force_unload (ResourceRef res) noexcept {
    auto data = static_cast<ResourceData*>(res.data);
    if (data->state == RS::Unloaded) return;
    require(data->state != RS::Loading);
    really_unload(data);
}

struct Update {
    Link link2link;
    Link new_link;
};

NOINLINE static void reload_commit (UniqueArray<Update>&& updates) {
    updates.consume([](Update&& update){
        update.link2link.write(
            AccessCB(move(update), [](Update&& update, Type t, Mu* v){
                expect(t == Type::For<Link>());
                reinterpret_cast<Link&>(*v) = move(update.new_link);
            })
        );
    });
}

NOINLINE static void reload_rollback (UniqueArray<ROV>&& rovs) {
    rovs.consume([](auto&& rov){
        auto data = static_cast<ResourceData*>(rov.res.data.p);
        data->value = move(rov.old_value);
    });
}

void reload (Slice<ResourceRef> reses) try {
    UniqueArray<ROV> rovs;
    for (auto res : reses) {
        if (res->state() == RS::Loaded) {
            rovs.push_back({res, {}});
        }
        else raise_ResourceStateInvalid();
    }
     // Preserve step
    for (auto& rov : rovs) {
        auto data = static_cast<ResourceData*>(rov.res.data.p);
        rov.old_value = move(data->value);
    }

    UniqueArray<Update> updates;
    try {
         // Construct step
         // TODO: Start ResourceTransaction for dependently-loaded resources.
        for (auto res : reses) {
            auto data = static_cast<ResourceData*>(res.data);
            data->state = RS::Loading;
            auto scheme = g_universe->require_scheme(data->name);
            auto path = scheme->require_filepath(data->name);
            Tree tree = tree_from_file(move(path));
            auto tnt = verify_tree_for_scheme(scheme, tree);
            expect(!data->value);
            data->value = AnyVal(tnt.type);
             // Do not DelaySwizzle for reload.  TODO: Forbid reload while a
             // serialization operation is ongoing.
            item_from_tree(data->value.ptr(), tnt.tree, SharedRoute(res));
            data->state = RS::Loaded;
        }
         // Verify step
        UniqueArray<ResourceRef> others;
        for (auto& [name, other] : g_universe->resources) {
            switch (other->state()) {
                case RS::Unloaded: continue;
                case RS::Loaded: others.emplace_back(&*other); break;
                default: raise(e_General, "Another resource is currently loading");
            }
            for (auto res : reses) {
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
                if (link2link.type() != Type::For<Link>()) return false;
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
    wrap_error_for_resources(reses, "reload");
}

void rename (ResourceRef old_res, ResourceRef new_res) try {
    auto old_data = static_cast<ResourceData*>(old_res.data);
    auto new_data = static_cast<ResourceData*>(new_res.data);
    if (old_data->state != RS::Loaded) {
        raise_ResourceStateInvalid();
    }
    if (new_data->state != RS::Unloaded) {
        raise_ResourceStateInvalid();
    }
    expect(!new_data->value);
    new_data->value = move(old_data->value);
    new_data->state = RS::Loaded;
    old_data->state = RS::Unloaded;
} catch (...) {
    wrap_error_for_resources({old_res, new_res}, "rename");
}

AnyString resource_filepath (const IRI& name) try {
    auto scheme = g_universe->require_scheme(name);
    return scheme->get_filepath(name);
} catch (Error& e) {
    e.add_tag("ayu::ResourceName", name.possibly_invalid_spec());
    throw e;
}

void delete_source (const IRI& name) try {
    auto scheme = g_universe->require_scheme(name);
    auto path = scheme->require_filepath(name);
    remove_utf8(path.c_str());
} catch (Error& e) {
    e.add_tag("ayu::ResourceName", name.possibly_invalid_spec());
    throw e;
}

bool source_exists (const IRI& name) try {
    auto scheme = g_universe->require_scheme(name);
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
    for (auto& [name, rd] : g_universe->resources)
    if (rd->state() != RS::Unloaded) {
        r.push_back(rd);
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
#include "../test/test-environment.private.h"

AYU_DESCRIBE_INSTANTIATE(std::vector<i32*>)

static tap::TestSet tests ("dirt/ayu/resources/resource", []{
    using namespace tap;
    using namespace iri::literals;

    test::TestEnvironment env;

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

    ayu::Document* doc = null;
    doesnt_throw([&]{
        doc = &input->value().as<ayu::Document>();
    }, "Getting typed value from a resource");
    is(input->state(), RS::Loaded, "Resource::value() automatically loads resource");
    is(input["foo"][1].get_as<i32>(), 4, "Value was generated properly (0)");
    is(input["bar"][1].get_as<std::string>(), "qux", "Value was generated properly (1)");

    throws_code<e_ResourceStateInvalid>([&]{ save(output); }, "save throws on unloaded resource");

    doc->delete_with_name("foo");
    doc->new_with_name<i32>("asdf", 51);

    doesnt_throw([&]{ rename(input, output); }, "rename");
    is(input->state(), RS::Unloaded, "Old resource is RS::Unloaded after renaming");
    is(output->state(), RS::Loaded, "New resource is RS::Loaded after renaming");
    is(&output->value().as<ayu::Document>(), doc, "Rename moves value without reconstructing it");

    doesnt_throw([&]{ save(output); }, "save");
    is(tree_from_file(resource_filepath(output->name())), tree_from_string(
        "[ayu::Document {bar:[std::string qux] asdf:[i32 51] _next_id:0}]"
    ), "Resource was saved with correct contents");
    ok(source_exists(output->name()), "source_exists returns true before deletion");
    doesnt_throw([&]{ delete_source(output->name()); }, "delete_source");
    ok(!source_exists(output->name()), "source_exists returns false after deletion");
    throws_code<e_OpenFailed>([&]{
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
    doc = &output->value().as<ayu::Document>();
    link = output["asdf"][1].address_as<i32>();
    doesnt_throw([&]{
        rt = link_to_route(link);
    });
    is(item_to_tree(&rt), tree_from_string("\"ayu-test:/test-output.ayu#/asdf+1\""), "link_to_route works");
    doc->new_<Link>(output["bar"][1]);
    doesnt_throw([&]{ save(output); }, "save with link");
    doc->new_<i32*>(output["asdf"][1]);
    doesnt_throw([&]{ save(output); }, "save with pointer");
    is(tree_from_file(resource_filepath(output->name())), tree_from_string(
        "[ayu::Document {bar:[std::string qux] asdf:[i32 51] _0:[ayu::Link #/bar+1] _1:[i32* #/asdf+1] _next_id:2}]"
    ), "File was saved with correct link as route");
    throws_code<e_OpenFailed>([&]{
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

    throws_code<e_ResourceTypeRejected>([&]{
        load(SharedResource("ayu-test:/wrongtype.ayu"_iri));
    }, "ResourceScheme::accepts_type rejects wrong type");

    AnyString ordinary_path;
    throws_code<e_ResourceSchemeNotFound>([&]{
        ordinary_path = resource_filepath("file:/foo/bar"_iri);
    }, "Can't use file:/ resource when there's a scheme registered.");
     // Copy because deactivating modifies this array.
    auto schemes = g_universe->schemes;
    for (auto& scheme : schemes) {
        scheme.value->deactivate();
    }
    doesnt_throw([&]{
        ordinary_path = resource_filepath("file:/foo/bar"_iri);
    }, "Can use file:/ resource when there are no schemes registered.");
    is(ordinary_path, "/foo/bar", "file:/ IRI gives correct filepath");

    done_testing();
});
#endif
