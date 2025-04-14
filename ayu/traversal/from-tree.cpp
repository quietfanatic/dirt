#include "from-tree.h"
#include <memory>
#include "../reflection/description.private.h"
#include "../resources/resource.h"
#include "compound.private.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

struct SwizzleOp {
    SwizzleFunc<Mu>* f;
    AnyRef item;
    Tree tree;
    SharedRoute rt;

    ALWAYS_INLINE
    SwizzleOp (
        SwizzleFunc<Mu>* f, AnyRef&& i, const Tree& t, SharedRoute&& l
    ) noexcept :
        f(f), item(move(i)), tree(t), rt(move(l))
    { }
     // Allow optimized reallocation
    ALWAYS_INLINE
    SwizzleOp (SwizzleOp&& o) {
        std::memcpy((void*)this, &o, sizeof(SwizzleOp));
        std::memset((void*)&o, 0, sizeof(SwizzleOp));
    }
};
struct InitOp {
    InitFunc<Mu>* f;
    double priority;
    AnyRef item;
    SharedRoute rt;

     // This being noexcept allows UniqueArray::emplace to be smaller
    ALWAYS_INLINE
    InitOp (
        InitFunc<Mu>* f, double p, AnyRef&& i, SharedRoute&& l
    ) noexcept :
        f(f), priority(p), item(move(i)), rt(move(l))
    { }
     // Allow optimized reallocation
    ALWAYS_INLINE
    InitOp (InitOp&& o) {
        std::memcpy((void*)this, &o, sizeof(InitOp));
        std::memset((void*)&o, 0, sizeof(InitOp));
    }
};
struct IFTContext {
    static IFTContext* current;
    IFTContext* previous;
    IFTContext () : previous(current) {
        current = this;
    }
    ~IFTContext () {
        expect(current == this);
        current = previous;
    }

    UniqueArray<SwizzleOp> swizzle_ops;
    UniqueArray<InitOp> init_ops;
};

IFTContext* IFTContext::current = null;

struct FromTreeTraversalHead {
    const Tree* tree;
};
struct ClaimAttrsTraversalHead {
    u32* next_list;
};
template <class T = Traversal>
struct FromTreeTraversal : FromTreeTraversalHead, T { };
template <class T = Traversal>
struct ClaimAttrsTraversal :
    ClaimAttrsTraversalHead, FromTreeTraversal<T>
{ };

struct TraverseFromTree {

///// START, DOING SWIZZLE AND INIT

    static
    void start (
        const AnyRef& item, const Tree& tree, RouteRef rt,
        FromTreeOptions opts
    ) {
        plog("from_tree start");
        if (tree.form == Form::Undefined) {
            raise(e_FromTreeFormRejected,
                "Undefined tree given to item_from_tree"
            );
        }
        if (!!(opts & FromTreeOptions::DelaySwizzle) && IFTContext::current) {
             // Delay swizzle and inits to the outer item_from_tree call.  Basically
             // this just means keep the current context instead of making a new one.
            start_without_context(item, tree, rt);
        }
        else start_with_context(item, tree, rt);
        plog("from_tree end");
    }

    NOINLINE static
    void start_with_context (
        const AnyRef& item, const Tree& tree, RouteRef rt
    ) {
         // Start a resource transaction so that dependency loads are all or
         // nothing.
        ResourceTransaction tr;
        IFTContext ctx;
        start_without_context(item, tree, rt);
        do_swizzle_init(ctx);
        expect(!ctx.swizzle_ops.owned());
        expect(!ctx.init_ops.owned());
    }

    NOINLINE static
    void start_without_context (
        const AnyRef& item, const Tree& tree, RouteRef rt
    ) {
        PushBaseRoute pbl(rt ? rt : RouteRef(SharedRoute(item)));
        FromTreeTraversal<StartTraversal> child;
        child.tree = &tree;
        trav_start<visit>(child, item, rt, AccessMode::Write);
    }

    NOINLINE static
    void do_swizzle_init (IFTContext& ctx) {
        if (ctx.swizzle_ops) do_swizzle(ctx);
        else if (ctx.init_ops) do_init(ctx);
    }

    NOINLINE static
    void do_swizzle (IFTContext& ctx) {
        expect(ctx.swizzle_ops);
        ctx.swizzle_ops.consume([](SwizzleOp&& op){
            PushBaseRoute pbl (op.rt);
            try {
                op.item.modify(AccessCB(op, [](auto& op, AnyPtr v, bool){
                    op.f(*v.address, op.tree);
                }));
            }
            catch (...) {
                rethrow_with_route(op.rt);
            }
        });
         // Swizzling might add more swizzle ops; this will happen if we're
         // swizzling a pointer which points to a separate resource; that
         // resource will be load()ed in op.f().
        do_swizzle_init(ctx);
    }

    NOINLINE static
    void do_init (IFTContext& ctx) {
        expect(ctx.init_ops);
        ctx.init_ops.consume([](InitOp&& op){
            PushBaseRoute pbl (op.rt);
            try {
                op.item.modify(AccessCB(op, [](auto& op, AnyPtr v, bool){
                    op.f(*v.address);
                }));
            }
            catch (...) {
                rethrow_with_route(op.rt);
            }
        });
         // Initting might add more swizzle or init ops.  It'd be weird, but
         // it's allowed for an init() to load another resource.
        do_swizzle_init(ctx);
    }

///// PICK STRATEGY

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const FromTreeTraversal<>&>(tr);
        if (trav.readonly) {
            raise(e_General, "Tried to do from_tree operation on a readonly reference?");
        }
        if (auto before = trav.desc->before_from_tree()) {
            use_before(trav, before->f);
        }
        else after_before(trav);
    }

    NOINLINE static
    void use_before (const FromTreeTraversal<>& trav, FromTreeFunc<Mu>* f) {
        f(*trav.address, *trav.tree);
        after_before(trav);
    }

    NOINLINE static
    void after_before (const FromTreeTraversal<>& trav) {
         // If description has a from_tree, just use that.
        if (auto from_tree = trav.desc->from_tree()) [[likely]] {
            use_from_tree(trav, from_tree->f);
        }
         // Now check for values.  Values can be of any tree form now, not just
         // atomic forms.
        else if (auto values = trav.desc->values()) {
            if (!!(trav.desc->flags & DescFlags::ValuesAllStrings)) {
                if (trav.tree->form == Form::String) {
                    use_values_all_strings(trav, values);
                }
                else no_match(trav);
            }
            else use_values(trav, values);
        }
        else no_match(trav);
    }

    NOINLINE static
    void no_match (const FromTreeTraversal<>& trav) {
         // Now the behavior depends on what form of tree we got
        if (trav.tree->form == Form::Object) {
            if (auto keys = trav.desc->keys_acr()) {
                return use_computed_attrs(trav, keys);
            }
            else if (auto attrs = trav.desc->attrs()) {
                return use_attrs(trav, attrs);
            }
             // fallthrough
        }
        else if (trav.tree->form == Form::Array) {
            if (auto length = trav.desc->length_acr()) {
                if (!!(trav.desc->flags & DescFlags::ElemsContiguous)) {
                    return use_contiguous_elems(trav, length);
                }
                else {
                    return use_computed_elems(trav, length);
                }
            }
            else if (auto elems = trav.desc->elems()) {
                return use_elems(trav, elems);
            }
             // fallthrough
        }
         // Nothing matched, so try delegate
        if (auto acr = trav.desc->delegate_acr()) {
            use_delegate(trav, acr);
        }
         // Still nothing?  Allow swizzle with no from_tree.
        else if (trav.desc->swizzle_offset) {
            register_swizzle_init(trav);
        }
        else fail(trav);
    }

///// FROM TREE STRATEGY

    NOINLINE static
    void use_from_tree (
        const FromTreeTraversal<>& trav, FromTreeFunc<Mu>* f
    ) {
        f(*trav.address, *trav.tree);
        finish_item(trav);
    }

///// OBJECT STRATEGIES


    static
    void use_attrs (
        const FromTreeTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
         // We need to allocate an array of integers to keep track of claimed
         // keys on objects.  For small (that is, not enormous) objects, we want
         // to allocate on the stack.  Normally you'd use a variable-length
         // array for this, but it seems that at least on my compiler, VLAs have
         // a granularity of 4096 bytes and are not very well optimized, so
         // we're just going to pick a few fixed values for our array size.  The
         // function stub for allocating stack is very small, so having multiple
         // of these is cheap.
         //
         // TODO: This is probably more complicated than necessary.
         //
         // There isn't a lot of meaning to these numbers, but they should at
         // least be multiples of 16 (the granularity for stack allocations).
        constexpr usize stack_capacity_0 = 64; // 15 keys
        constexpr usize stack_capacity_1 = 256; // 63 keys
        constexpr usize stack_capacity_2 = 1024; // 255 keys
         // Linux has a larger default stack limit than other OSes so it's safe
         // to use more stack space.  Linux: 8M, Windows: 1M, MacOS: 512K
#ifdef __linux__
         // 4096 triggers some extra code on GCC
        constexpr usize stack_capacity_3 = 4080; // 1019 keys
#endif
        auto next_list_len = (trav.tree->meta >> 1) + 1;
        if (next_list_len <= stack_capacity_0 / 4) {
            use_attrs_stack<stack_capacity_0>(trav, attrs);
        }
        else if (next_list_len <= stack_capacity_1 / 4) {
            use_attrs_stack<stack_capacity_1>(trav, attrs);
        }
        else if (next_list_len <= stack_capacity_2 / 4) {
            use_attrs_stack<stack_capacity_2>(trav, attrs);
        }
#ifdef __linux__
        else if (next_list_len <= stack_capacity_3 / 4) {
            use_attrs_stack<stack_capacity_3>(trav, attrs);
        }
#endif
        else {
            use_attrs_heap(trav, attrs, next_list_len);
        }
    }

    template <usize capacity> NOINLINE static
    void use_attrs_stack (
        const FromTreeTraversal<>& trav, const AttrsDcrPrivate* attrs
    ) {
        u32 next_list_buf [capacity / 4];
        use_attrs_buf(trav, attrs, next_list_buf);
    }

    NOINLINE static
    void use_attrs_heap (
        const FromTreeTraversal<>& trav, const AttrsDcrPrivate* attrs,
        u32 next_list_len
    ) {
        auto next_list_buf = std::unique_ptr<u32[]>(new u32[next_list_len]);
        use_attrs_buf(trav, attrs, &next_list_buf[0]);
    }

    NOINLINE static
    void use_attrs_buf (
        const FromTreeTraversal<>& trav, const AttrsDcrPrivate* attrs,
        u32* next_list_buf
    ) {
         // Build a linked list of indexes so that we can claim attrs in
         // constant time.  next_list = next_list_buffer + 1, so that:
         //   - next_list[-1] is the index of the first non-claimed attr
         //     (its initial value is 0 for the first attr).
         //   - next_list[i] is the index of whatever non-claimed attr is
         //     next after the ith attr.
         //   - If next_list[i] is -1, that means i is the index of the last
         //     non-claimed attr.
         // When an attr is claimed, its link will be deleted from the
         // linked list by setting next_list[previous index] to
         // next_list[i].
         //
         // This makes the attr-claiming algorithm O(n^2) in the worst case
         // instead of O(n^3).  It also makes the best case O(n) (when all attrs
         // of the item are provided and in the same order they're declared in
         // the AYU_DESCRIBE block).
         //
         // In theory, we could make the worst-case O(n) as well by stuffing the
         // keys in an unordered_map or something, but the extra overhead is
         // unlikely to be worth it.
         //
         // (Note that using -1 as a sentinel does not reduce the usable array
         // size by 1.  The maximum array tree size is u32(-1), for which
         // u32(-1) is not a valid index.)
        auto len = trav.tree->meta >> 1;
        for (u32 i = 0; i < len; i++) next_list_buf[i] = i;
        next_list_buf[len] = u32(-1);

        claim_attrs_use_attrs(trav, &next_list_buf[0] + 1, attrs);
        if (next_list_buf[0] != u32(-1)) {
            expect(trav.tree->form == Form::Object);
            raise_AttrRejected(
                trav.desc, trav.tree->data.as_object_ptr[next_list_buf[0]].first
            );
        }
    }

    NOINLINE static
    void use_computed_attrs (
        const FromTreeTraversal<>& trav, const Accessor* keys_acr
    ) {
         // Computed attrs always take the entire object, so we don't need to
         // allocate a next_list.
        expect(trav.tree->form == Form::Object);
        set_keys(trav, Slice<TreePair>(*trav.tree), keys_acr);
        expect(trav.desc->computed_attrs_offset);
        auto f = trav.desc->computed_attrs()->f;
        expect(trav.tree->form == Form::Object);
        for (auto& pair : Slice<TreePair>(*trav.tree)) {
            write_computed_attr(trav, pair, f);
        }
        finish_item(trav);
    }

    NOINLINE static
    void claim_attrs (const Traversal& tr) {
        auto& trav = static_cast<const ClaimAttrsTraversal<>&>(tr);
        if (auto keys = trav.desc->keys_acr()) {
            claim_attrs_use_computed_attrs(trav, trav.next_list, keys);
        }
        else if (auto attrs = trav.desc->attrs()) {
            claim_attrs_use_attrs(trav, trav.next_list, attrs);
        }
        else if (auto acr = trav.desc->delegate_acr()) {
            claim_attrs_use_delegate(trav, trav.next_list, acr);
        }
        else raise_AttrsNotSupported(trav.desc);
    }

    NOINLINE static
    void claim_attrs_use_attrs (
        const FromTreeTraversal<>& trav, u32* next_list,
        const AttrsDcrPrivate* attrs
    ) {
        expect(trav.tree->form == Form::Object);
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto flags = attr->acr()->attr_flags;
             // First try matching attr directly even if it's included
            u32* prev_next; u32 j;
            for (
                prev_next = &next_list[-1], j = *prev_next;
                j != u32(-1);
                prev_next = &next_list[j], j = *prev_next
            ) {
                auto& [key, value] = trav.tree->data.as_object_ptr[j];
                if (key == attr->key) {
                    if (!(flags & AttrFlags::Ignored)) {
                        Tree singleton;
                        FromTreeTraversal<AttrTraversal> child;
                        if (!!(flags & AttrFlags::CollapseOptional)) {
                            child.tree = &(singleton = Tree::array(value));
                        }
                        else child.tree = &value;
                        trav_attr<visit>(
                            child, trav, attr->acr(), attr->key, AccessMode::Write
                        );
                    }
                     // Claim attr by deleting link
                    *prev_next = next_list[j];
                    goto next_attr;
                }
            }
             // No match, try including, optional, collapsing
            if (!!(flags & AttrFlags::Include)) {
                 // Included.  Recurse with the same tree.
                ClaimAttrsTraversal<AttrTraversal> child;
                child.next_list = next_list;
                child.tree = trav.tree;
                trav_attr<claim_attrs>(
                    child, trav, attr->acr(), attr->key, AccessMode::Write
                );
            }
            else if (!!(flags & (AttrFlags::Optional|AttrFlags::Ignored))) {
                 // Leave the attribute in its default-constructed state.
            }
            else {
                FromTreeTraversal<AttrTraversal> child;
                if (!!(flags & AttrFlags::CollapseOptional)) {
                     // If the attribute was not provided and has
                     // collapse_optional set, deserialize the item with an
                     // empty array.
                    static constexpr auto empty = Tree::array();
                    child.tree = &empty;
                }
                else if (const Tree* def = attr->default_value()) {
                    child.tree = def;
                }
                else raise_AttrMissing(trav.desc, attr->key);
                trav_attr<visit>(
                    child, trav, attr->acr(), attr->key, AccessMode::Write
                );
            }
            next_attr:;
        }
         // The claim_* stack doesn't call finish_item so call it here.
        finish_item(trav);
    }

    NOINLINE static
    void claim_attrs_use_computed_attrs (
        const FromTreeTraversal<>& trav, u32* next_list,
        const Accessor* keys_acr
    ) {
         // We should only get here if a parent item included a child item that
         // has computed attrs.
        expect(trav.tree->form == Form::Object);
        set_keys(trav, Slice<TreePair>(*trav.tree), keys_acr);
        expect(trav.desc->computed_attrs_offset);
        auto f = trav.desc->computed_attrs()->f;
        u32* prev_next; u32 i;
        for (
            prev_next = &next_list[-1], i = *prev_next;
            i != u32(-1);
            prev_next = &next_list[i], i = *prev_next
        ) {
            expect(trav.tree->form == Form::Object);
            write_computed_attr(trav, trav.tree->data.as_object_ptr[i], f);
        }
         // Consume entire list
        next_list[-1] = u32(-1);
        finish_item(trav);
    }

    NOINLINE static
    void claim_attrs_use_delegate (
        const FromTreeTraversal<>& trav, u32* next_list,
        const Accessor* acr
    ) {
        ClaimAttrsTraversal<DelegateTraversal> child;
        child.next_list = next_list;
        child.tree = trav.tree;
        trav_delegate<claim_attrs>(child, trav, acr, AccessMode::Write);
    }

    static
    void set_keys (
        const FromTreeTraversal<>& trav, Slice<TreePair> object,
        const Accessor* keys_acr
    ) {
        if (!(keys_acr->flags & AcrFlags::Readonly)) {
            set_keys_write(trav, object, keys_acr);
        }
        else {
            set_keys_readonly(trav, object, keys_acr);
        }
    }

    NOINLINE static
    void set_keys_write (
        const FromTreeTraversal<>& trav, Slice<TreePair> object,
        const Accessor* keys_acr
    ) {
         // Writable keys, so write them.
        auto keys = UniqueArray<AnyString>(
            object.size(), [&object](u32 i){ return object[i].first; }
        );
        keys_acr->write(*trav.address,
            AccessCB(move(keys), [](auto&& keys, AnyPtr v, bool)
        {
            require_writeable_keys(v.type);
            reinterpret_cast<AnyArray<AnyString>&>(*v.address) = move(keys);
        }));
        expect(!keys.owned());
    }

    NOINLINE static
    void set_keys_readonly (
        const FromTreeTraversal<>& trav, Slice<TreePair> object,
        const Accessor* keys_acr
    ) {
         // Readonly keys?  Read them and check that they match.
        AnyArray<AnyString> keys;
        keys_acr->read(*trav.address,
            AccessCB(keys, [](auto& keys, AnyPtr v, bool)
        {
            require_readable_keys(v.type);
            new (&keys) AnyArray<AnyString>(
                reinterpret_cast<AnyArray<AnyString>&>(*v.address)
            );
        }));
#ifndef NDEBUG
         // Check returned keys for duplicates
        for (u32 i = 0; i < keys.size(); i++)
        for (u32 j = 0; j < i; j++) {
            expect(keys[i] != keys[j]);
        }
#endif
        if (keys.size() >= object.size()) {
            for (auto& required : keys) {
                for (auto& given : object) {
                    if (given.first == required) goto next_required;
                }
                raise_AttrMissing(trav.desc, required);
                next_required:;
            }
        }
        else [[unlikely]] {
             // Too many keys given
            for (auto& given : object) {
                for (auto& required : keys) {
                    if (required == given.first) goto next_given;
                }
                raise_AttrRejected(trav.desc, given.first);
                next_given:;
            }
            never();
        }
    }

    static
    void write_computed_attr (
        const FromTreeTraversal<>& trav, const TreePair& pair, AttrFunc<Mu>* f
    ) {
        auto& [key, value] = pair;
        AnyRef ref = f(*trav.address, key);
        if (!ref) raise_AttrNotFound(trav.desc, key);
        FromTreeTraversal<ComputedAttrTraversal> child;
        child.tree = &value;
        trav_computed_attr<visit>(child, trav, ref, f, key, AccessMode::Write);
    }

///// ARRAY STRATEGIES

    NOINLINE static
    void use_elems (
        const FromTreeTraversal<>& trav, const ElemsDcrPrivate* elems
    ) {
         // Check whether length is acceptable
        u32 min = elems->chop_flag(AttrFlags::Optional);
        expect(trav.tree->form == Form::Array);
        auto array = Slice<Tree>(*trav.tree);
        if (array.size() < min || array.size() > elems->n_elems) {
            raise_LengthRejected(trav.desc, min, elems->n_elems, array.size());
        }
        for (u32 i = 0; i < array.size(); i++) {
            auto acr = elems->elem(i)->acr();
            if (!!(acr->attr_flags & AttrFlags::Ignored)) continue;
            FromTreeTraversal<ElemTraversal> child;
            child.tree = &array[i];
            trav_elem<visit>(child, trav, acr, i, AccessMode::Write);
        }
        finish_item(trav);
    }

    NOINLINE static
    void use_computed_elems (
        const FromTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        expect(trav.tree->form == Form::Array);
        auto array = Slice<Tree>(*trav.tree);
        u32 len = array.size();
        write_length_acr(len, AnyPtr(trav.desc, trav.address), length_acr);
        expect(trav.desc->computed_elems_offset);
        auto f = trav.desc->computed_elems()->f;
        for (u32 i = 0; i < array.size(); i++) {
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.desc, i);
            FromTreeTraversal<ComputedElemTraversal> child;
            child.tree = &array[i];
            trav_computed_elem<visit>(
                child, trav, ref, f, i, AccessMode::Write
            );
        }
        finish_item(trav);
    }

    NOINLINE static
    void use_contiguous_elems (
        const FromTreeTraversal<>& trav, const Accessor* length_acr
    ) {
        expect(trav.tree->form == Form::Array);
        auto array = Slice<Tree>(*trav.tree);
        u32 len = array.size();
        write_length_acr(len, AnyPtr(trav.desc, trav.address), length_acr);
        if (array) {
            expect(trav.desc->contiguous_elems_offset);
            auto f = trav.desc->contiguous_elems()->f;
            auto ptr = f(*trav.address);
            for (u32 i = 0; i < array.size(); i++) {
                FromTreeTraversal<ContiguousElemTraversal> child;
                child.tree = &array[i];
                trav_contiguous_elem<visit>(
                    child, trav, ptr, f, i, AccessMode::Write
                );
                ptr.address = (Mu*)((char*)ptr.address + ptr.type.cpp_size());
            }
        }
        finish_item(trav);
    }

///// OTHER STRATEGIES

    NOINLINE static
    void use_values_all_strings (
        const FromTreeTraversal<>& trav, const ValuesDcrPrivate* values
    ) {
        for (u32 i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
             // These are for optimization, not safety
            expect(trav.tree->form == Form::String);
            expect(value->name.form == Form::String);
            if (Str(*trav.tree) == Str(value->name)) {
                values->assign.generic(*trav.address, *value->get_value());
                return finish_item(trav);
            }
        }
        no_match(trav);
    }

    NOINLINE static
    void use_values (
        const FromTreeTraversal<>& trav, const ValuesDcrPrivate* values
    ) {
        for (u32 i = 0; i < values->n_values; i++) {
            auto value = values->value(i);
            if (*trav.tree == value->name) {
                values->assign.generic(*trav.address, *value->get_value());
                return finish_item(trav);
            }
        }
        no_match(trav);
    }

    NOINLINE static
    void use_delegate (
        const FromTreeTraversal<>& trav, const Accessor* acr
    ) {
        FromTreeTraversal<DelegateTraversal> child;
        child.tree = trav.tree;
        trav_delegate<visit>(child, trav, acr, AccessMode::Write);
        finish_item(trav);
    }

///// REGISTERING SWIZZLE AND INIT

    NOINLINE static
    void finish_item (const FromTreeTraversal<>& trav) {
         // Now register swizzle and init ops.  We're doing it now instead of at the
         // beginning to make sure that children get swizzled and initted before
         // their parent.
        if (!!trav.desc->swizzle_offset | !!trav.desc->init_offset) {
            register_swizzle_init(trav);
        }
         // Done
    }

    NOINLINE static
    void register_swizzle_init (const FromTreeTraversal<>& trav) {
         // We're duplicating the work to get the ref and rt if there's both a
         // swizzle and an init, but almost no types are going to have both.
        if (auto swizzle = trav.desc->swizzle()) {
            AnyRef ref;
            trav.to_reference(&ref);
            SharedRoute rt;
            trav.to_route(&rt);
            IFTContext::current->swizzle_ops.emplace_back(
                swizzle->f, move(ref), *trav.tree, move(rt)
            );
            expect(!ref.acr);
            expect(!rt);
        }
        if (auto init = trav.desc->init()) {
            auto& init_ops = IFTContext::current->init_ops;
            u32 i;
            for (i = init_ops.size(); i > 0; --i) {
                if (init->priority <= init_ops[i-1].priority) break;
            }
            AnyRef ref;
            trav.to_reference(&ref);
            SharedRoute rt;
            trav.to_route(&rt);
            if (i == init_ops.size()) init_ops.emplace_back(
                init->f, init->priority, move(ref), move(rt)
            );
            else init_ops.emplace(i,
                init->f, init->priority, move(ref), move(rt)
            );
            expect(!ref.acr);
            expect(!rt);
        }
    }

///// ERRORS

    [[noreturn, gnu::cold]] NOINLINE static
    void fail (const FromTreeTraversal<>& trav) {
         // If we got here, we failed to find any method to from_tree this item.
         // Go through maybe a little too much effort to figure out what went
         // wrong.
        if (trav.tree->form == Form::Error) {
             // Dunno how a lazy error managed to smuggle itself this far.  Give
             // it the attention it deserves.
            std::rethrow_exception(std::exception_ptr(*trav.tree));
        }
        bool object_rejected = trav.tree->form == Form::Object &&
            (trav.desc->values() || trav.desc->accepts_array());
        bool array_rejected = trav.tree->form == Form::Array &&
            (trav.desc->values() || trav.desc->accepts_object());
        bool other_rejected =
            trav.desc->accepts_array() || trav.desc->accepts_object();
        if (object_rejected || array_rejected || other_rejected) {
            raise_FromTreeFormRejected(trav.desc, trav.tree->form);
        }
        else if (trav.desc->values()) {
            raise(e_FromTreeValueNotFound, cat(
                "No value for type ", Type(trav.desc).name(),
                " matches the provided tree ", tree_to_string(*trav.tree)
            ));
        }
        else raise(e_FromTreeNotSupported, cat(
            "Item of type ", Type(trav.desc).name(), " does not support from_tree."
        ));
    }
};

} using namespace in;

void item_from_tree (
    const AnyRef& item, const Tree& tree, RouteRef rt,
    FromTreeOptions opts
) {
    TraverseFromTree::start(item, tree, rt, opts);
}

void raise_FromTreeFormRejected (Type t, Form f) {
    raise(e_FromTreeFormRejected, cat(
        "Item of type ", t.name(),
        " does not support from_tree with a tree of form ", item_to_string(&f)
    ));
}

} // ayu
