// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Code for handling extensions implemented as WebAssembly modules.
 *
 * Authors:
 *   Dan Eicher <dan.eicher@gmail.com>
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "wasm-backend.h"

#include <cstring>
#include <fstream>
#include <limits>
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>
#include <wasm.h>
#include <2geom/convex-hull.h>
#include <2geom/path-sink.h>

#include "actions/actions-helper-gui.h"
#include "colors/color.h"
#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "extension/effect.h"
#include "extension/execution-env.h"
#include "extension/extension.h"
#include "extension/input.h"
#include "gc-anchored.h"
#include "helper/geom-pathstroke.h"
#include "inkscape-application.h"
#include "layer-manager.h"
#include "libnrtype/Layout-TNG.h"
#include "livarot/LivarotDefs.h"
#include "live_effects/effect.h"
#include "message-stack.h"
#include "message.h"
#include "object/sp-clippath.h"
#include "object/sp-defs.h"
#include "object/sp-flowtext.h"
#include "object/sp-item-group.h"
#include "object/sp-lpe-item.h"
#include "object/sp-marker.h"
#include "object/sp-mask.h"
#include "object/sp-object.h"
#include "object/sp-page.h"
#include "object/sp-paint-server.h"
#include "object/sp-root.h"
#include "object/sp-shape.h"
#include "object/sp-text.h"
#include "page-manager.h"
#include "path/path-boolop.h"
#include "path/path-outline.h"
#include "path/path-simplify.h"
#include "path/path-util.h"
#include "preferences.h"
#include "selection.h"
#include "style.h"
#include "svg/svg-length.h"
#include "svg/svg.h"
#include "text-editing.h"
#include "ui/util.h"
#include "util/cast.h"
#include "util/units.h"
#include "wasm-abi.h"
#include "xml/attribute-record.h"
#include "xml/document.h"
#include "xml/node.h"
#include "xml/repr.h"

namespace Inkscape::Extension::Implementation {
namespace {

/**
 * One host function, named the way a wasm import names it: by module AND field.
 *
 * Both halves are part of the key. Matching on the field alone would let any module claim
 * any host function -- "Whatever.appendChild" reaching the real one -- which is a mistake
 * with a long history in embedders.
 */
struct HostFunction
{
    char const *module;
    char const *name;
    wasm_func_callback_with_env_t callback;
    size_t param_count;
    size_t result_count;
};

/** Shorthand for the argument unpacking every callback starts with. */
inline WasmInvocation *invocationOf(void *env)
{
    return static_cast<WasmInvocation *>(env);
}

/**
 * Detach @a node from its parent, if it has one, so that it can be inserted somewhere.
 *
 * DOM says inserting a node that is already in the tree MOVES it. Inkscape's addChild()
 * instead asserts that the node is unparented -- `g_assert(!child->_parent)` -- so handing it
 * a node still attached elsewhere aborts the process. Reparenting is about the most ordinary
 * thing a plugin does, so the removal happens here rather than being left to the guest.
 *
 * The anchor is handed to the invocation rather than released once the node is reinserted,
 * and the reason is not the few instructions in between. The collector scans the C stack
 * conservatively, so while a node is sitting in a local variable it is already reachable --
 * detaching and reattaching within one host call is safe on its own.
 *
 * What is not safe is afterwards. THE HANDLE TABLE IS NOT A GC ROOT: it is a std::vector
 * from the ordinary allocator, and the collector does not scan that memory. So once a host
 * call returns, a node that is unparented and reachable only through its handle is reachable
 * by nothing the collector can see -- and the guest may keep that handle for the rest of the
 * invocation, doing work that allocates. The exposure is not a window of instructions, it is
 * every moment between one host call and the next.
 *
 * Hence the rule the whole file depends on: every node a handle can reach is either parented
 * or owned by the invocation. Detaching a node removes the first, so this supplies the second.
 */
inline void detachForInsert(WasmInvocation *invocation, Inkscape::XML::Node *node)
{
    if (auto *parent = node->parent()) {
        node->anchor();
        invocation->own(node);
        parent->removeChild(node);
    }
}

/**
 * Unpack the receiving node of a Node operation.
 *
 * Every Node method starts the same way, and the failure is always the same trap, so the
 * repetition is worth removing: what is left in each function is what it actually does.
 */
#define INK_NODE_ARG(name, operation)                             \
    auto *invocation = invocationOf(env);                         \
    auto *name = invocation->getNode(args->data[0].of.i32);       \
    if (!name) {                                                  \
        return invocation->trap(operation ": not a Node handle"); \
    }

#define INK_DOCUMENT_ARG(name, operation)                             \
    auto *invocation = invocationOf(env);                             \
    auto *name = invocation->getDocument(args->data[0].of.i32);       \
    if (!name) {                                                      \
        return invocation->trap(operation ": not a Document handle"); \
    }

/** Read a (offset, length) string argument pair, or trap. */
#define INK_STRING_ARG(name, first, operation)                                                     \
    std::string name;                                                                              \
    if (!invocation->readString(args->data[first].of.i32, args->data[(first) + 1].of.i32, name)) { \
        return invocation->trap(operation ": string is outside the module's memory");              \
    }

/**
 * Answer a string through the (out_offset, out_capacity) pair at @a first, or trap.
 *
 * The trap is the bad-span case only. A value that does not fit the buffer offered is not an
 * error at all -- the guest is told the length it needs and asks again -- so the two travel
 * separately: one as a trap, the other as the result. @see WasmInvocation::writeString().
 */
#define INK_STRING_RESULT(value, first, operation)                                                              \
    int32_t written = 0;                                                                                        \
    if (!invocation->writeString(args->data[first].of.i32, args->data[(first) + 1].of.i32, (value), written)) { \
        return invocation->trap(operation ": result is outside the module's memory");                           \
    }                                                                                                           \
    results->data[0] = WASM_I32_VAL(written);

/**
 * Unpack the item a query operates on.
 *
 * Two different nothings, kept apart: a handle that is not a Node is a bug in the plugin and
 * traps here, while a Node that draws nothing -- a comment, a <defs> child -- yields a null
 * item and each operation answers "absent". A plugin can therefore tell its own mistake from
 * a fact about the drawing, which it could not if both produced the same result.
 */
#define INK_ITEM_ARG(name, operation)                              \
    auto *invocation = invocationOf(env);                          \
    auto *name##_node = invocation->getNode(args->data[0].of.i32); \
    if (!name##_node) {                                            \
        return invocation->trap(operation ": not a Node handle");  \
    }                                                              \
    auto *name = invocation->itemFor(name##_node);

/**
 * Unpack the document an SVGSVGElement query is asked of.
 *
 * The handle is the root element, because that is where SVG puts width, height and viewBox --
 * the answers are the document's, which for the root element is the same thing.
 */
#define INK_ROOT_ARG(operation)                                      \
    auto *invocation = invocationOf(env);                            \
    if (!invocation->getNode(args->data[0].of.i32)) {                \
        return invocation->trap(operation ": not a Node handle");    \
    }                                                                \
    auto *document = invocation->document();                         \
    if (!document) {                                                 \
        return invocation->trap(operation ": there is no document"); \
    }

/**
 * DOM's node type numbering, which is not Inkscape's.
 *
 * Inkscape's enum class NodeType (xml/node.h) has five members counted from zero starting at
 * DOCUMENT_NODE; DOM fixes ELEMENT=1, TEXT=3, PI=7, COMMENT=8, DOCUMENT=9 and always has.
 * The published API uses DOM's numbers -- returning Inkscape's would be an invented interface
 * wearing a familiar name.
 */
int32_t domNodeType(Inkscape::XML::NodeType type)
{
    switch (type) {
        case Inkscape::XML::NodeType::ELEMENT_NODE:
            return 1;
        case Inkscape::XML::NodeType::TEXT_NODE:
            return 3;
        case Inkscape::XML::NodeType::PI_NODE:
            return 7;
        case Inkscape::XML::NodeType::COMMENT_NODE:
            return 8;
        case Inkscape::XML::NodeType::DOCUMENT_NODE:
            return 9;
    }
    return 0;
}

/** org.inkscape.Node.nodeType(node) -> i32 */
wasm_trap_t *nodeType(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "nodeType")
    results->data[0] = WASM_I32_VAL(domNodeType(node->type()));
    return nullptr;
}

/** org.inkscape.Node.nodeName(node, out_offset, out_capacity) -> i32 */
wasm_trap_t *nodeName(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "nodeName")
    INK_STRING_RESULT(node->name(), 1, "nodeName")
    return nullptr;
}

/**
 * org.inkscape.Node.localName(node, out_offset, out_capacity) -> i32
 *
 * Inkscape stores qualified names ("svg:rect"); DOM splits them. The part after the colon is
 * the local name, and a name without one is entirely local.
 */
wasm_trap_t *localName(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "localName")
    char const *name = node->name();
    char const *colon = name ? strchr(name, ':') : nullptr;
    INK_STRING_RESULT(colon ? colon + 1 : name, 1, "localName")
    return nullptr;
}

/**
 * org.inkscape.Node.namespaceURI(node, out_offset, out_capacity) -> i32
 *
 * Resolved through the same prefix table the rest of Inkscape uses, so a node in a foreign
 * namespace reports the URI that was actually declared rather than a guess.
 */
wasm_trap_t *namespaceURI(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "namespaceURI")
    char const *name = node->name();
    char const *colon = name ? strchr(name, ':') : nullptr;

    char const *uri = nullptr;
    if (colon) {
        std::string const prefix(name, colon - name);
        uri = sp_xml_ns_prefix_uri(prefix.c_str());
    }
    INK_STRING_RESULT(uri, 1, "namespaceURI")
    return nullptr;
}

/** org.inkscape.Node.ownerDocument(node) -> Document */
wasm_trap_t *ownerDocument(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "ownerDocument")
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Document, node->document()));
    return nullptr;
}

/** The five navigation accessors, which differ only in which neighbour they ask for. */
#define INK_NAVIGATOR(function, operation, expression)                                             \
    wasm_trap_t *function(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)          \
    {                                                                                              \
        INK_NODE_ARG(node, operation)                                                              \
        results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, expression)); \
        return nullptr;                                                                            \
    }

INK_NAVIGATOR(parentNode, "parentNode", node->parent())
INK_NAVIGATOR(firstChild, "firstChild", node->firstChild())
INK_NAVIGATOR(lastChild, "lastChild", node->lastChild())
INK_NAVIGATOR(nextSibling, "nextSibling", node->next())
INK_NAVIGATOR(previousSibling, "previousSibling", node->prev())

/**
 * org.inkscape.Node.childNodes(node) -> NodeList
 *
 * The list is the parent itself: DOM requires a live collection, and re-reading the node on
 * each access gives that for free, with nothing to invalidate.
 */
wasm_trap_t *childNodes(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "childNodes")
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::NodeList, node));
    return nullptr;
}

// A NodeList reaches the guest two ways: live, standing for a parent whose children are
// re-read on each access, and as a snapshot of a query's results. The interface declares one
// NodeList, so both operations accept either -- a plugin should not have to know which kind
// of list it was handed, and could not find out if it wanted to.

/** org.inkscape.NodeList.length(list) -> i32 */
wasm_trap_t *nodeListLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    int32_t const handle = args->data[0].of.i32;

    if (auto *node = invocation->getNodeList(handle)) {
        results->data[0] = WASM_I32_VAL(static_cast<int32_t>(node->childCount()));
        return nullptr;
    }
    if (auto const *snapshot = invocation->getNodeSnapshot(handle)) {
        results->data[0] = WASM_I32_VAL(static_cast<int32_t>(snapshot->size()));
        return nullptr;
    }
    return invocation->trap("NodeList.length: not a NodeList handle");
}

/** org.inkscape.NodeList.item(list, index) -> Node */
wasm_trap_t *nodeListItem(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    int32_t const handle = args->data[0].of.i32;
    int32_t const index = args->data[1].of.i32;

    // Out of range is null, not a trap: DOM says item() returns null, and a plugin walking a
    // list until it gets null is using the interface correctly.
    Inkscape::XML::Node *child = nullptr;
    if (auto *node = invocation->getNodeList(handle)) {
        child = index < 0 ? nullptr : node->nthChild(static_cast<unsigned>(index));
    } else if (auto const *snapshot = invocation->getNodeSnapshot(handle)) {
        child = (index >= 0 && static_cast<size_t>(index) < snapshot->size()) ? (*snapshot)[index] : nullptr;
    } else {
        return invocation->trap("NodeList.item: not a NodeList handle");
    }

    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, child));
    return nullptr;
}

/** org.inkscape.DOMStringList.length(list) -> i32 */
wasm_trap_t *stringListLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto const *strings = invocation->getStringList(args->data[0].of.i32);
    if (!strings) {
        return invocation->trap("DOMStringList.length: not a DOMStringList handle");
    }
    results->data[0] = WASM_I32_VAL(static_cast<int32_t>(strings->size()));
    return nullptr;
}

/** org.inkscape.DOMStringList.item(list, index, out_offset, out_capacity) -> i32 */
wasm_trap_t *stringListItem(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto const *strings = invocation->getStringList(args->data[0].of.i32);
    if (!strings) {
        return invocation->trap("DOMStringList.item: not a DOMStringList handle");
    }
    int32_t const index = args->data[1].of.i32;
    char const *value =
        (index >= 0 && static_cast<size_t>(index) < strings->size()) ? (*strings)[index].c_str() : nullptr;
    INK_STRING_RESULT(value, 2, "DOMStringList.item")
    return nullptr;
}

/** org.inkscape.Node.textContent(node, out_offset, out_capacity) -> i32 */
wasm_trap_t *getTextContent(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "textContent")
    INK_STRING_RESULT(node->content(), 1, "textContent")
    return nullptr;
}

/** org.inkscape.Node.setTextContent(node, offset, length) */
wasm_trap_t *setTextContent(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_NODE_ARG(node, "setTextContent")
    INK_STRING_ARG(text, 1, "setTextContent")
    node->setContent(text.c_str());
    return nullptr;
}

/** org.inkscape.Document.documentElement(document) -> Node */
wasm_trap_t *documentElement(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_DOCUMENT_ARG(document, "documentElement")
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, document->root()));
    return nullptr;
}

/** org.inkscape.Document.createElement(document, name_offset, name_length) -> Node */
wasm_trap_t *createElement(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *document = invocation->getDocument(args->data[0].of.i32);
    if (!document) {
        return invocation->trap("createElement: not a Document handle");
    }

    std::string name;
    if (!invocation->readString(args->data[1].of.i32, args->data[2].of.i32, name)) {
        return invocation->trap("createElement: name is outside the module's memory");
    }

    auto *node = document->createElement(name.c_str());
    invocation->own(node);
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, node));
    return nullptr;
}

/** org.inkscape.Element.setAttribute(node, key_offset, key_length, value_offset, value_length) */
wasm_trap_t *setAttribute(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    auto *invocation = invocationOf(env);
    auto *node = invocation->getNode(args->data[0].of.i32);
    if (!node) {
        return invocation->trap("setAttribute: not a Node handle");
    }

    std::string key;
    std::string value;
    if (!invocation->readString(args->data[1].of.i32, args->data[2].of.i32, key) ||
        !invocation->readString(args->data[3].of.i32, args->data[4].of.i32, value)) {
        return invocation->trap("setAttribute: string is outside the module's memory");
    }

    node->setAttribute(key, value);
    return nullptr;
}

/** org.inkscape.Document.createElementNS(document, ns_off, ns_len, name_off, name_len) -> Node */
wasm_trap_t *createElementNS(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_DOCUMENT_ARG(document, "createElementNS")
    INK_STRING_ARG(uri, 1, "createElementNS")
    INK_STRING_ARG(qualified, 3, "createElementNS")

    // Inkscape names nodes by prefix, so a URI has to be turned into one. Asking for the
    // prefix with the caller's own as the suggestion means a document that already declares
    // the namespace keeps using its spelling, and an undeclared one gets a usable prefix
    // rather than being rejected -- which is what makes foreign namespaces authorable.
    std::string name = qualified;
    if (!uri.empty()) {
        auto const colon = qualified.find(':');
        std::string const suggested = colon == std::string::npos ? std::string() : qualified.substr(0, colon);
        std::string const local = colon == std::string::npos ? qualified : qualified.substr(colon + 1);
        if (char const *prefix = sp_xml_ns_uri_prefix(uri.c_str(), suggested.empty() ? nullptr : suggested.c_str())) {
            name = std::string(prefix) + ":" + local;
        }
    }

    auto *node = document->createElement(name.c_str());
    invocation->own(node);
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, node));
    return nullptr;
}

/** org.inkscape.Document.createTextNode(document, offset, length) -> Node */
wasm_trap_t *createTextNode(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_DOCUMENT_ARG(document, "createTextNode")
    INK_STRING_ARG(text, 1, "createTextNode")
    auto *node = document->createTextNode(text.c_str());
    invocation->own(node);
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, node));
    return nullptr;
}

/** org.inkscape.Document.createComment(document, offset, length) -> Node */
wasm_trap_t *createComment(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_DOCUMENT_ARG(document, "createComment")
    INK_STRING_ARG(text, 1, "createComment")
    auto *node = document->createComment(text.c_str());
    invocation->own(node);
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, node));
    return nullptr;
}

/** org.inkscape.Document.getElementById(document, offset, length) -> Node */
wasm_trap_t *getElementById(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_DOCUMENT_ARG(document, "getElementById")
    INK_STRING_ARG(id, 1, "getElementById")
    // Recursive: getElementById searches the whole document, not the root's own children.
    auto *found = sp_repr_lookup_descendant(document->root(), "id", id.c_str());
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, found));
    return nullptr;
}

/**
 * org.inkscape.Node.insertBefore(parent, node, reference) -> Node
 *
 * DOM names the node to insert BEFORE; Inkscape's addChild names the one to insert AFTER.
 * The inversion is done here, once, because getting it backwards silently reorders a
 * drawing rather than failing.
 *
 * A null reference means append, as DOM specifies.
 */
wasm_trap_t *insertBefore(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(parent, "insertBefore")
    auto *node = invocation->getNode(args->data[1].of.i32);
    if (!node) {
        return invocation->trap("insertBefore: not a Node handle");
    }

    int32_t const reference_handle = args->data[2].of.i32;
    Inkscape::XML::Node *reference = nullptr;
    if (reference_handle != 0) {
        reference = invocation->getNode(reference_handle);
        if (!reference) {
            return invocation->trap("insertBefore: reference is not a Node handle");
        }
        if (reference->parent() != parent) {
            return invocation->trap("insertBefore: reference is not a child of the parent");
        }
    }

    if (node == parent) {
        return invocation->trap("insertBefore: a node cannot contain itself");
    }

    // Read the insertion point before detaching: if the node being moved is the reference's
    // previous sibling, removing it changes where "before the reference" is.
    auto *after = reference ? reference->prev() : parent->lastChild();
    if (after == node) {
        after = node->prev();
    }
    detachForInsert(invocation, node);
    parent->addChild(node, after);

    results->data[0] = WASM_I32_VAL(args->data[1].of.i32);
    return nullptr;
}

/** org.inkscape.Node.removeChild(parent, child) -> Node */
wasm_trap_t *removeChild(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(parent, "removeChild")
    auto *child = invocation->getNode(args->data[1].of.i32);
    if (!child) {
        return invocation->trap("removeChild: not a Node handle");
    }
    if (child->parent() != parent) {
        return invocation->trap("removeChild: not a child of the parent");
    }

    // Anchored before removal, because a node in the document is held alive by its parent
    // and nothing else. DOM returns the removed node and the guest still has a handle to it,
    // so it has to survive being detached; the invocation releases it at the end.
    child->anchor();
    invocation->own(child);
    parent->removeChild(child);

    results->data[0] = WASM_I32_VAL(args->data[1].of.i32);
    return nullptr;
}

/** org.inkscape.Node.replaceChild(parent, node, old) -> Node */
wasm_trap_t *replaceChild(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(parent, "replaceChild")
    auto *node = invocation->getNode(args->data[1].of.i32);
    auto *old = invocation->getNode(args->data[2].of.i32);
    if (!node || !old) {
        return invocation->trap("replaceChild: not a Node handle");
    }
    if (old->parent() != parent) {
        return invocation->trap("replaceChild: the node replaced is not a child of the parent");
    }

    if (node == parent || old == node) {
        return invocation->trap("replaceChild: a node cannot replace itself or contain itself");
    }

    // Anchored BEFORE it leaves the tree, not after. DOM returns the replaced node and the
    // guest still holds a handle to it, so it has to survive removal -- and once removed its
    // refcount is zero with nothing pointing at it, so anchoring afterwards is already too
    // late if anything allocated in between.
    old->anchor();
    invocation->own(old);

    detachForInsert(invocation, node);
    parent->addChild(node, old);
    parent->removeChild(old);

    results->data[0] = WASM_I32_VAL(args->data[2].of.i32);
    return nullptr;
}

/**
 * org.inkscape.Node.cloneNode(node, deep) -> Node
 *
 * XML::Node::duplicate() is always deep, so a shallow clone is made by duplicating and then
 * emptying it. The parameter is kept because DOM has it and a plugin author will expect it;
 * quietly ignoring it would be worse than the small cost of honouring it.
 */
wasm_trap_t *cloneNode(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "cloneNode")
    auto *copy = node->duplicate(node->document());
    if (copy && args->data[1].of.i32 == 0) {
        while (auto *child = copy->firstChild()) {
            copy->removeChild(child);
        }
    }
    invocation->own(copy);
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, copy));
    return nullptr;
}

/** org.inkscape.Element.removeAttribute(node, offset, length) */
wasm_trap_t *removeAttribute(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_NODE_ARG(node, "removeAttribute")
    INK_STRING_ARG(key, 1, "removeAttribute")
    node->removeAttribute(key);
    return nullptr;
}

/** org.inkscape.Element.hasAttribute(node, offset, length) -> i32 */
wasm_trap_t *hasAttribute(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "hasAttribute")
    INK_STRING_ARG(key, 1, "hasAttribute")
    results->data[0] = WASM_I32_VAL(node->attribute(key.c_str()) != nullptr ? 1 : 0);
    return nullptr;
}

/** org.inkscape.Element.getAttributeNames(node) -> DOMStringList */
wasm_trap_t *getAttributeNames(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "getAttributeNames")
    std::vector<std::string> names;
    for (auto const &attribute : node->attributeList()) {
        names.emplace_back(g_quark_to_string(attribute.key));
    }
    results->data[0] = WASM_I32_VAL(invocation->makeStringList(std::move(names)));
    return nullptr;
}

/**
 * org.inkscape.Element.getAttribute(node, key_offset, key_length, out_offset, out_capacity) -> i32
 *
 * An absent attribute answers -1; anything else is the value's own length, written when the
 * buffer offered was big enough for it. @see WasmInvocation::writeString().
 */
wasm_trap_t *getAttribute(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *node = invocation->getNode(args->data[0].of.i32);
    if (!node) {
        return invocation->trap("getAttribute: not a Node handle");
    }

    std::string key;
    if (!invocation->readString(args->data[1].of.i32, args->data[2].of.i32, key)) {
        return invocation->trap("getAttribute: key is outside the module's memory");
    }

    INK_STRING_RESULT(node->attribute(key.c_str()), 3, "getAttribute")
    return nullptr;
}

/** org.inkscape.Node.appendChild(parent, child) -> Node */
wasm_trap_t *appendChild(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *parent = invocation->getNode(args->data[0].of.i32);
    auto *child = invocation->getNode(args->data[1].of.i32);
    if (!parent || !child) {
        return invocation->trap("appendChild: not a Node handle");
    }
    if (child == parent) {
        return invocation->trap("appendChild: a node cannot contain itself");
    }

    detachForInsert(invocation, child);
    parent->appendChild(child);

    results->data[0] = WASM_I32_VAL(args->data[1].of.i32);
    return nullptr;
}

// ── Lookup and identity ─────────────────────────────────────────────────────────────────
//
// Finding things, and saying what they are. A quarter of the shipped Python extensions walk
// the tree by hand looking for elements of a type, because a subprocess holding serialised SVG
// has nothing better; Inkscape itself has had indexed lookup and real CSS selector matching
// the whole time.
//
// The naming follows DOM where DOM has a name for the operation, even though the backing is
// Inkscape's own: getObjectsByElement is getElementsByTagName, and pretending otherwise would
// invent vocabulary for something already named.

/** Unpack the SPDocument behind a Document handle. */
#define INK_SPDOC_ARG(name, operation)                                \
    auto *invocation = invocationOf(env);                             \
    if (!invocation->getDocument(args->data[0].of.i32)) {             \
        return invocation->trap(operation ": not a Document handle"); \
    }                                                                 \
    auto *name = invocation->document();                              \
    if (!name) {                                                      \
        return invocation->trap(operation ": there is no document");  \
    }

/** Turn a list of objects into a NodeList handle over their reprs. */
int32_t nodesOfObjects(WasmInvocation *invocation, std::vector<SPObject *> const &objects)
{
    std::vector<Inkscape::XML::Node *> nodes;
    nodes.reserve(objects.size());
    for (auto *object : objects) {
        if (object) {
            if (auto *repr = object->getRepr()) {
                nodes.push_back(repr);
            }
        }
    }
    return invocation->makeNodeSnapshot(std::move(nodes));
}

/**
 * Drop the shadow copies a <use> makes.
 *
 * Inkscape's lookups walk the object tree, where a <use> holds a copy of its target as a child
 * object. Those copies have no elements in the document and cannot be addressed by id, and DOM
 * defines these searches over the document tree, so they are not results.
 */
std::vector<SPObject *> withoutCloneShadows(std::vector<SPObject *> objects)
{
    std::erase_if(objects, [](SPObject *object) { return !object || object->cloned; });
    return objects;
}

/** org.inkscape.Document.getElementsByTagName(document, name_off, name_len) -> NodeList */
wasm_trap_t *getElementsByTagName(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_SPDOC_ARG(document, "getElementsByTagName")
    INK_STRING_ARG(name, 1, "getElementsByTagName")
    document->ensureUpToDate();
    results->data[0] =
        WASM_I32_VAL(nodesOfObjects(invocation, withoutCloneShadows(document->getObjectsByElement(name))));
    return nullptr;
}

/** org.inkscape.Document.getElementsByClassName(document, name_off, name_len) -> NodeList */
wasm_trap_t *getElementsByClassName(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_SPDOC_ARG(document, "getElementsByClassName")
    INK_STRING_ARG(name, 1, "getElementsByClassName")
    document->ensureUpToDate();
    results->data[0] = WASM_I32_VAL(nodesOfObjects(invocation, withoutCloneShadows(document->getObjectsByClass(name))));
    return nullptr;
}

/**
 * org.inkscape.Document.querySelectorAll(document, selector_off, selector_len) -> NodeList
 *
 * Real selector matching, not a tag-name shortcut: Inkscape resolves author stylesheets and
 * carries the machinery to do it. A plugin approximating `g > rect.marker` by walking the tree
 * is reimplementing a matcher that is already here and already correct.
 */
wasm_trap_t *querySelectorAll(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_SPDOC_ARG(document, "querySelectorAll")
    INK_STRING_ARG(selector, 1, "querySelectorAll")
    document->ensureUpToDate();
    results->data[0] =
        WASM_I32_VAL(nodesOfObjects(invocation, withoutCloneShadows(document->getObjectsBySelector(selector))));
    return nullptr;
}

/** org.inkscape.Document.querySelector(document, selector_off, selector_len) -> Element? */
wasm_trap_t *querySelector(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_SPDOC_ARG(document, "querySelector")
    INK_STRING_ARG(selector, 1, "querySelector")
    document->ensureUpToDate();
    auto const matches = withoutCloneShadows(document->getObjectsBySelector(selector));
    // DOM's querySelector is the first match in document order, and 0 when there is none --
    // not an empty list, because the caller asked for one element.
    Inkscape::XML::Node *first = nullptr;
    for (auto *object : matches) {
        if (object && object->getRepr()) {
            first = object->getRepr();
            break;
        }
    }
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, first));
    return nullptr;
}

/**
 * org.inkscape.Document.generateId(document, prefix_off, prefix_len, out, cap) -> i32
 *
 * A document-wide unique id. Without it every plugin creating elements has to invent its own
 * scheme and hope: ids collide silently, and the loser is whichever element gets referenced by
 * something else -- a gradient, a clip path, a use.
 */
wasm_trap_t *generateId(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_SPDOC_ARG(document, "generateId")
    INK_STRING_ARG(prefix, 1, "generateId")
    std::string const generated = document->generate_unique_id(prefix.c_str());
    INK_STRING_RESULT(generated.c_str(), 3, "generateId")
    return nullptr;
}

/**
 * org.inkscape.Document.resourceList(document, key_off, key_len) -> NodeList
 *
 * The document's registry of a kind of resource. Inkscape maintains these as documents are
 * edited; the alternative is walking <defs> and classifying by tag, which finds the ones in
 * <defs> and misses every other one.
 *
 * The keys are Inkscape's, and the set is exactly what registers itself -- "gradient",
 * "pattern", "filter", "font", "image", "symbol", "mask", "grid", "guide", "layer", "page",
 * "hatch", "script", "iccprofile". Notably NOT "marker": markers are not registered, so asking
 * for them answers empty rather than answering wrongly.
 */
wasm_trap_t *resourceList(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_SPDOC_ARG(document, "resourceList")
    INK_STRING_ARG(key, 1, "resourceList")
    results->data[0] = WASM_I32_VAL(nodesOfObjects(invocation, document->getResourceList(key.c_str())));
    return nullptr;
}

/** org.inkscape.Document.resolveHref(document, href_off, href_len) -> Element? */
wasm_trap_t *resolveHref(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_SPDOC_ARG(document, "resolveHref")
    INK_STRING_ARG(href, 1, "resolveHref")
    auto *object = document->getObjectByHref(href);
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, object ? object->getRepr() : nullptr));
    return nullptr;
}

/** org.inkscape.SVGSVGElement.defs(root) -> Element? */
wasm_trap_t *defs(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ROOT_ARG("defs")
    auto *defs = document->getDefs();
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, defs ? defs->getRepr() : nullptr));
    return nullptr;
}

/** org.inkscape.SVGSVGElement.namedView(root) -> Element? */
wasm_trap_t *namedView(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ROOT_ARG("namedView")
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, document->getReprNamedView()));
    return nullptr;
}

/**
 * Identity: what a thing is called, in the places SVG and Inkscape put a name.
 *
 * <title> and <desc> are CHILD ELEMENTS, so neither is reachable as an attribute at all, and
 * writing one means creating an element in the right place -- the fiddly work a host absorbs.
 *
 * Label is two facts, not one, and they are kept apart for the same reason specified and
 * computed style are. label() is the `inkscape:label` attribute and is ABSENT when unset;
 * defaultLabel() is the name the UI displays, which falls back to "#id" or "<tag>" and is
 * therefore never absent and held by no attribute. Collapsing them would mean either a
 * setter whose getter disagrees with it, or a plugin unable to ask whether the author named
 * this thing at all.
 */
#define INK_OBJECT_ARG(name, operation)                            \
    auto *invocation = invocationOf(env);                          \
    auto *name##_node = invocation->getNode(args->data[0].of.i32); \
    if (!name##_node) {                                            \
        return invocation->trap(operation ": not a Node handle");  \
    }                                                              \
    auto *name = invocation->objectFor(name##_node);

/**
 * org.inkscape.SVGElement.label(element, out, cap) -> i32
 *
 * The `inkscape:label` attribute, absent when the author never set one. Exactly what
 * setLabel writes, so the pair round-trips.
 */
wasm_trap_t *label(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_OBJECT_ARG(object, "label")
    INK_STRING_RESULT(object ? object->label() : nullptr, 1, "label")
    return nullptr;
}

/**
 * org.inkscape.SVGElement.defaultLabel(element, out, cap) -> i32
 *
 * The name the UI shows: the label if there is one, otherwise "#id", otherwise "<tag>". Never
 * absent, and held by no attribute -- a plugin listing objects for a user cannot reconstruct
 * it, because the fallback is Inkscape's presentation choice rather than document content.
 */
wasm_trap_t *defaultLabel(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_OBJECT_ARG(object, "defaultLabel")
    INK_STRING_RESULT(object ? object->defaultLabel() : nullptr, 1, "defaultLabel")
    return nullptr;
}

/** org.inkscape.SVGElement.setLabel(element, offset, length) */
wasm_trap_t *setLabel(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_OBJECT_ARG(object, "setLabel")
    INK_STRING_ARG(label, 1, "setLabel")
    if (object) {
        object->setLabel(label.c_str());
    }
    return nullptr;
}

/**
 * org.inkscape.SVGElement.title(element, out, cap) -> i32
 * org.inkscape.SVGElement.desc(element, out, cap) -> i32
 *
 * Both return a freshly allocated string, so both free it. Absent when the element has no such
 * child, which is a different answer from an empty one.
 */
wasm_trap_t *titleOrDesc(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results, bool wants_title,
                         char const *operation)
{
    auto *invocation = invocationOf(env);
    auto *node = invocation->getNode(args->data[0].of.i32);
    if (!node) {
        return invocation->trap((std::string(operation) + ": not a Node handle").c_str());
    }
    auto *object = invocation->objectFor(node);

    char *owned = object ? (wants_title ? object->title() : object->desc()) : nullptr;
    int32_t written = 0;
    bool const fits = invocation->writeString(args->data[1].of.i32, args->data[2].of.i32, owned, written);
    g_free(owned);
    if (!fits) {
        return invocation->trap((std::string(operation) + ": result is outside the module's memory").c_str());
    }
    results->data[0] = WASM_I32_VAL(written);
    return nullptr;
}

wasm_trap_t *title(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    return titleOrDesc(env, args, results, true, "title");
}

wasm_trap_t *desc(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    return titleOrDesc(env, args, results, false, "desc");
}

/** org.inkscape.SVGElement.setTitle(element, offset, length) */
wasm_trap_t *setTitle(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_OBJECT_ARG(object, "setTitle")
    INK_STRING_ARG(title, 1, "setTitle")
    if (object) {
        // verbatim, so a plugin that means to write "   " gets it. The default treats
        // whitespace as a request to remove the element, which is right for a user typing in a
        // dialog and wrong for a caller passing a computed string.
        object->setTitle(title.c_str(), true);
    }
    return nullptr;
}

/** org.inkscape.SVGElement.setDesc(element, offset, length) */
wasm_trap_t *setDesc(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_OBJECT_ARG(object, "setDesc")
    INK_STRING_ARG(desc, 1, "setDesc")
    if (object) {
        object->setDesc(desc.c_str(), true);
    }
    return nullptr;
}

/**
 * org.inkscape.SVGElement.linkedObjects(element, direction) -> NodeList
 *
 * Who references this. Asking a gradient which elements paint with it, or a clip path which
 * elements it clips, means resolving href strings against the document AND knowing every
 * attribute and style property that can carry one -- Inkscape already maintains the back
 * link, because it has to update these when the target changes.
 *
 * `direction` is Inkscape's own LinkedObjectNature and takes its values: -1 dependent
 * (things referencing this), 0 any, 1 dependency (things this references).
 *
 * The forward direction is thin, and that is the host's honest state rather than a promise:
 * SPObject::getLinked answers only from hrefList, which is a back-link list, and only SPUse,
 * SPAnchor and SPText override it to report what they point AT. So direction 1 answers for a
 * clone, a hyperlink and text-on-a-path, and answers nothing for an element that merely fills
 * with a gradient. Reversing the question -- ask the gradient who uses it -- works for
 * everything.
 */
wasm_trap_t *linkedObjects(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_OBJECT_ARG(object, "linkedObjects")
    if (!object) {
        results->data[0] = WASM_I32_VAL(invocation->makeNodeSnapshot({}));
        return nullptr;
    }

    auto direction = SPObject::LinkedObjectNature::ANY;
    switch (args->data[1].of.i32) {
        case -1:
            direction = SPObject::LinkedObjectNature::DEPENDENT;
            break;
        case 1:
            direction = SPObject::LinkedObjectNature::DEPENDENCY;
            break;
        default:
            break;
    }
    results->data[0] = WASM_I32_VAL(nodesOfObjects(invocation, object->getLinked(direction)));
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.isHidden(element) -> i32
 * org.inkscape.SVGGraphicsElement.isLocked(element) -> i32
 *
 * Both are resolved state rather than an attribute reading: hidden follows from the style
 * cascade, locked from sodipodi:insensitive on this element or any ancestor. A plugin that
 * edits a locked or hidden object has done the thing the user asked the editor to prevent, so
 * "skip what is not editable" needs an answer that accounts for ancestors.
 */
wasm_trap_t *isHidden(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "isHidden")
    results->data[0] = WASM_I32_VAL(item && item->isHidden() ? 1 : 0);
    return nullptr;
}

wasm_trap_t *isLocked(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "isLocked")
    results->data[0] = WASM_I32_VAL(item && item->isLocked() ? 1 : 0);
    return nullptr;
}

// ── Document metrics ────────────────────────────────────────────────────────────────────
//
// The size and scale of the drawing itself. `Grid::effect()` opens with preferredBounds() and
// getDocumentScale(), so an extension that cannot ask for them cannot be a peer of the
// builtins -- which is the whole claim.
//
// These hang off SVGSVGElement rather than Document because that is where SVG puts them: the
// width, height and viewBox are attributes of <svg>. The element handle is the root, reached
// through documentElement(); the answers are the document's, which for the root element is
// the same thing.

/** Write a Geom::Affine as the six numbers of a DOMMatrix. */
bool writeAffine(WasmInvocation *invocation, int32_t offset, Geom::Affine const &affine)
{
    double const values[6] = {affine[0], affine[1], affine[2], affine[3], affine[4], affine[5]};
    return invocation->writeDoubles(offset, values, 6);
}

/** Answer a Geom::OptRect as a DOMRect out-parameter plus a present flag. */
wasm_trap_t *answerRect(WasmInvocation *invocation, wasm_val_vec_t *results, int32_t offset, Geom::OptRect const &box,
                        char const *operation)
{
    if (!box) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    double const rect[4] = {box->left(), box->top(), box->width(), box->height()};
    if (!invocation->writeDoubles(offset, rect, 4)) {
        return invocation->trap((std::string(operation) + ": result is outside the module's memory").c_str());
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGSVGElement.preferredBounds(root, out_offset) -> i32 present
 *
 * The page if the drawing has one, the content otherwise. What `Grid` measures to decide how
 * far its grid should run.
 */
wasm_trap_t *preferredBounds(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ROOT_ARG("preferredBounds")
    return answerRect(invocation, results, args->data[1].of.i32, document->preferredBounds(), "preferredBounds");
}

/** org.inkscape.SVGSVGElement.pageBounds(root, out_offset) -> i32 present */
wasm_trap_t *pageBounds(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ROOT_ARG("pageBounds")
    return answerRect(invocation, results, args->data[1].of.i32, document->pageBounds(), "pageBounds");
}

/**
 * org.inkscape.SVGSVGElement.documentScale(root, out_offset) -> i32 present
 *
 * The two components of the document scale, as a DOMPoint. `Grid` inverts this to turn the
 * millimetres its parameters are given in into the user units it writes.
 */
wasm_trap_t *documentScale(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ROOT_ARG("documentScale")
    Geom::Scale const scale = document->getDocumentScale();
    double const values[2] = {scale[Geom::X], scale[Geom::Y]};
    if (!invocation->writeDoubles(args->data[1].of.i32, values, 2)) {
        return invocation->trap("documentScale: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGSVGElement.documentSize(root, out_offset) -> i32 present
 *
 * Width and height as a DOMPoint, in user units. The document stores them as unit-carrying
 * quantities; the unit is reported separately by displayUnit, because a plugin doing
 * arithmetic wants one consistent unit and a plugin writing a label wants the author's.
 */
wasm_trap_t *documentSize(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ROOT_ARG("documentSize")
    double const values[2] = {document->getWidth().value("px"), document->getHeight().value("px")};
    if (!invocation->writeDoubles(args->data[1].of.i32, values, 2)) {
        return invocation->trap("documentSize: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/** org.inkscape.SVGSVGElement.viewBox(root, out_offset) -> i32 present */
wasm_trap_t *viewBox(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ROOT_ARG("viewBox")
    return answerRect(invocation, results, args->data[1].of.i32, Geom::OptRect(document->getViewBox()), "viewBox");
}

/**
 * org.inkscape.SVGSVGElement.displayUnit(root, out_offset, out_capacity) -> i32
 *
 * The unit the author is working in, as its abbreviation ("mm", "px"). Corpus evidence for
 * this one is blunt: a third of the shipped Python extensions convert units by hand.
 */
wasm_trap_t *displayUnit(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ROOT_ARG("displayUnit")
    auto const *unit = document->getDisplayUnit();
    INK_STRING_RESULT(unit ? unit->abbr.c_str() : nullptr, 1, "displayUnit")
    return nullptr;
}

// ── Typed attributes ────────────────────────────────────────────────────────────────────
//
// SVG 2 wraps attributes in SVGAnimatedLength and friends, whose `animVal` half needs an
// animation timeline Inkscape does not have. What a plugin actually wants from that layer is
// the parsed value, and Inkscape has already written the parsers -- so these expose the
// parsing, not the object model: a plugin handed an SVGAnimatedLength would get one live half
// and one that could never answer.

/** Unpack the (element, attribute name) pair every typed accessor starts with. */
#define INK_ATTR_ARG(node, attribute, operation)                  \
    auto *invocation = invocationOf(env);                         \
    auto *node = invocation->getNode(args->data[0].of.i32);       \
    if (!node) {                                                  \
        return invocation->trap(operation ": not a Node handle"); \
    }                                                             \
    INK_STRING_ARG(attribute, 1, operation)

/**
 * org.inkscape.SVGElement.getLength(element, name_off, name_len, out_offset) -> i32 unit
 *
 * Writes the author's number and its value in user units as a DOMPoint, and returns which
 * unit was written -- SVGLength::Unit, so 0 is a bare number and PERCENT is its own answer.
 * -1 means the attribute is absent or is not a length.
 *
 * Both numbers are reported because both are wanted and neither derives from the other
 * without the parser: "10mm" is 10 to a plugin echoing the author's value back, and 37.795
 * to one doing geometry.
 */
wasm_trap_t *getLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ATTR_ARG(node, attribute, "getLength")

    char const *text = node->attribute(attribute.c_str());
    SVGLength::Unit unit = SVGLength::NONE;
    double value = 0.0;
    double computed = 0.0;
    if (!text || !parse_number_with_unit(text, unit, value, computed, false)) {
        results->data[0] = WASM_I32_VAL(-1);
        return nullptr;
    }

    double const values[2] = {value, computed};
    if (!invocation->writeDoubles(args->data[3].of.i32, values, 2)) {
        return invocation->trap("getLength: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(static_cast<int32_t>(unit));
    return nullptr;
}

/** org.inkscape.SVGElement.getNumber(element, name_off, name_len, out_offset) -> i32 present */
wasm_trap_t *getNumber(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ATTR_ARG(node, attribute, "getNumber")

    char const *text = node->attribute(attribute.c_str());
    double value = 0.0;
    if (!text || !sp_svg_number_read_d(text, &value)) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    if (!invocation->writeDoubles(args->data[3].of.i32, &value, 1)) {
        return invocation->trap("getNumber: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGElement.getTransform(element, name_off, name_len, out_offset) -> i32 present
 *
 * The attribute parsed to a DOMMatrix. Named rather than fixed to `transform` because
 * `gradientTransform` and `patternTransform` carry the same grammar.
 */
wasm_trap_t *getTransform(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ATTR_ARG(node, attribute, "getTransform")

    char const *text = node->attribute(attribute.c_str());
    Geom::Affine affine;
    if (!text || !sp_svg_transform_read(text, &affine)) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    if (!writeAffine(invocation, args->data[3].of.i32, affine)) {
        return invocation->trap("getTransform: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGElement.setLength(element, name_off, name_len, value, unit)
 *
 * The inverse of getLength, so a plugin can write "10mm" without owning a table of unit
 * spellings. An unknown unit code writes a bare number rather than inventing a suffix.
 */
wasm_trap_t *setLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_ATTR_ARG(node, attribute, "setLength")

    SVGLength length;
    length.set(static_cast<SVGLength::Unit>(args->data[4].of.i32), args->data[3].of.f64);
    node->setAttribute(attribute, sp_svg_length_write_with_units(length));
    return nullptr;
}

/** org.inkscape.SVGElement.setNumber(element, name_off, name_len, value) */
wasm_trap_t *setNumber(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_ATTR_ARG(node, attribute, "setNumber")
    node->setAttributeSvgDouble(attribute.c_str(), args->data[3].of.f64);
    return nullptr;
}

/**
 * org.inkscape.SVGElement.setTransform(element, name_off, name_len, matrix_offset)
 *
 * Serialised by Inkscape's own writer, which picks the shortest correct spelling -- a pure
 * translation comes out as translate(...) rather than a six-number matrix.
 */
wasm_trap_t *setTransform(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_ATTR_ARG(node, attribute, "setTransform")

    int32_t const offset = args->data[3].of.i32;
    if (!invocation->spanIsValid(offset, 6 * static_cast<int32_t>(sizeof(double)))) {
        return invocation->trap("setTransform: matrix is outside the module's memory");
    }
    double values[6];
    memcpy(values, invocation->memoryBytes() + offset, sizeof values);

    Geom::Affine const affine(values[0], values[1], values[2], values[3], values[4], values[5]);
    node->setAttribute(attribute, sp_svg_transform_write(affine));
    return nullptr;
}

// ── Derived queries ─────────────────────────────────────────────────────────────────────
//
// The half that cannot be answered from the markup. A bounding box that accounts for stroke,
// markers and filters, a resolved transform, the length of a run of shaped text: none of it
// is recoverable from attributes without reimplementing the renderer, which is exactly why
// extensions running in a subprocess have never been able to ask.
//
// Every one of these takes an Element handle rather than some separate item handle, because
// in SVG2 a graphics element IS an element. Absence is reported as a zero present-flag and
// never as a zero value: a plugin must be able to tell "this draws nothing" from "this is
// empty", which is the distinction Geom::OptRect makes internally.

/** Bits of the getBBox() options argument; the names are SVG2's SVGBoundingBoxOptions. */
enum BBoxOption
{
    BBOX_FILL = 1 << 0,
    BBOX_STROKE = 1 << 1,
    BBOX_MARKERS = 1 << 2,
    BBOX_CLIPPED = 1 << 3
};

/** org.inkscape.SVGGraphicsElement.getBBox(element, options, out_offset) -> i32 present */
wasm_trap_t *getBBox(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    if (!item) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    uint32_t const options = static_cast<uint32_t>(args->data[1].of.i32);

    // SVG2 presents fill, stroke and markers as independent switches. Inkscape has two
    // bounding boxes, not seven: the geometric one, which is the path alone, and the visual
    // one, which carries stroke, markers and filter expansion together. So asking for
    // markers without stroke returns a box that also includes the stroke.
    //
    // The alternative would be to compute a marker-only box here, which means reimplementing
    // part of the renderer to satisfy a combination nothing asks for. Widening is the safe
    // direction -- a caller laying out around the result reserves too much space rather than
    // too little -- and saying so is better than an approximation nobody documented.
    // In the element's OWN user space, which is what SVG means by getBBox: the element's
    // transform is not applied, and neither are its ancestors'. Inkscape's document* bounds
    // are in document coordinates -- the same numbers with every transform above and
    // including this element folded in -- so a <rect x="10"> inside a translated group would
    // report where it ends up rather than where it is declared. Passing the identity keeps
    // the answer in the coordinate system the attributes are written in.
    bool const visual = (options & (BBOX_STROKE | BBOX_MARKERS)) != 0;
    bool const clipped = (options & BBOX_CLIPPED) != 0;
    Geom::OptRect box =
        visual || clipped ? item->visualBounds(Geom::identity(), true, clipped, clipped) : item->geometricBounds();

    return answerRect(invocation, results, args->data[2].of.i32, box, "getBBox");
}

/**
 * org.inkscape.SVGGraphicsElement.documentBBox(element, options, out_offset) -> i32 present
 *
 * getBBox() in document coordinates: the same box with every transform from the root down to
 * and including this element folded in. getBBox() answers in the element's own user space,
 * which is what SVG defines, and is the wrong space for comparing two elements under
 * different groups -- the case every align, distribute or measure plugin starts from.
 *
 * Composing it guest-side means walking to the root multiplying getCTM() results, which is
 * both the arithmetic Inkscape already did and an invitation to get it wrong.
 */
wasm_trap_t *documentBBox(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "documentBBox")
    if (!item) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    uint32_t const options = static_cast<uint32_t>(args->data[1].of.i32);
    auto const type = (options & (BBOX_STROKE | BBOX_MARKERS)) != 0 ? SPItem::VISUAL_BBOX : SPItem::GEOMETRIC_BBOX;
    return answerRect(invocation, results, args->data[2].of.i32, item->documentBounds(type), "documentBBox");
}

/** org.inkscape.SVGGraphicsElement.getCTM(element, out_offset) -> i32 present */
wasm_trap_t *getCTM(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    if (!item) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    if (!writeAffine(invocation, args->data[1].of.i32, item->i2doc_affine())) {
        return invocation->trap("getCTM: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.getScreenCTM(element, out_offset) -> i32 present
 *
 * Desktop coordinates only exist when there is a desktop. Run from the command line there is
 * none, and the honest answer is "absent" -- returning the identity would be a wrong answer
 * dressed as a right one.
 */
wasm_trap_t *getScreenCTM(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    auto *desktop = invocation->desktop();
    if (!item || !desktop) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    // i2dt_affine() alone is item-to-desktop *document* coordinates, which exist with or
    // without a window; the screen transform is that composed with the view's zoom, rotation
    // and pan. Returning the former would answer a different question under the same name.
    if (!writeAffine(invocation, args->data[1].of.i32, item->i2dt_affine() * desktop->d2w())) {
        return invocation->trap("getScreenCTM: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/** The path of an item in its own coordinates, or an empty vector if it has none. */
Geom::PathVector pathOf(SPItem *item)
{
    if (auto *shape = cast<SPShape>(item)) {
        if (auto const *curve = shape->curve()) {
            return *curve;
        }
    }
    return {};
}

/** org.inkscape.SVGGeometryElement.getTotalLength(element) -> f64 */
wasm_trap_t *getTotalLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    double total = 0.0;
    if (item) {
        for (auto const &path : pathOf(item)) {
            total += path.length();
        }
    }
    results->data[0] = WASM_F64_VAL(total);
    return nullptr;
}

/** org.inkscape.SVGGeometryElement.getPointAtLength(element, distance, out_offset) -> i32 present */
wasm_trap_t *getPointAtLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    auto const paths = item ? pathOf(item) : Geom::PathVector();
    if (paths.empty()) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    // SVG measures along the whole geometry, so walk the subpaths until the distance falls
    // inside one of them, then re-parameterise that one by arc length -- path time is not
    // proportional to distance, so pointAt() on the raw time would land somewhere else. This
    // is the same helper the live path effects use for the same reason.
    double remaining = args->data[1].of.f64;
    Geom::Point point = paths.front().initialPoint();
    for (auto const &path : paths) {
        double const length = path.length();
        if (remaining <= length || &path == &paths.back()) {
            auto const uniform = Geom::arc_length_parametrization(path.toPwSb(), 2, 0.1);
            point = uniform.valueAt(std::max(0.0, std::min(remaining, uniform.domain().max())));
            break;
        }
        remaining -= length;
    }

    double const values[2] = {point.x(), point.y()};
    if (!invocation->writeDoubles(args->data[2].of.i32, values, 2)) {
        return invocation->trap("getPointAtLength: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.isPointInFill(element, x, y) -> i32
 * org.inkscape.SVGGeometryElement.isPointInStroke(element, x, y) -> i32
 *
 * isPointInStroke is the same argument as getBBox applied to hit testing: the answer depends
 * on stroke geometry, which a process holding serialised SVG does not have.
 */
wasm_trap_t *isPointInFill(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    bool inside = false;
    if (item) {
        // The point is in the element's own user space, as SVG specifies, so the path is
        // used as it stands rather than pushed into document coordinates.
        Geom::Point const point(args->data[1].of.f64, args->data[2].of.f64);
        auto const paths = pathOf(item);
        inside = !paths.empty() && paths.winding(point) != 0;
    }
    results->data[0] = WASM_I32_VAL(inside ? 1 : 0);
    return nullptr;
}

wasm_trap_t *isPointInStroke(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    bool inside = false;
    if (item && item->style) {
        // Element user space, as above -- so the stroke width is the computed one, with no
        // scaling by the element's place in the document.
        Geom::Point const point(args->data[1].of.f64, args->data[2].of.f64);
        auto const paths = pathOf(item);
        double const width = item->style->stroke_width.computed;
        if (!paths.empty() && width > 0.0) {
            // Within half a stroke width of the outline, which is what the stroke covers.
            Geom::Coord distance = 0.0;
            paths.nearestTime(point, &distance);
            inside = distance <= width / 2.0;
        }
    }
    results->data[0] = WASM_I32_VAL(inside ? 1 : 0);
    return nullptr;
}

// ── Path geometry ───────────────────────────────────────────────────────────────────────
//
// Corpus rank 1: 58 of the 129 shipped effect scripts parse path data. What they parse is the
// `d` attribute, which getAttribute has always handed over -- so the gap is not the string. It
// is that `d` is not the shape:
//
//   a <rect>, <circle>, <star> or <spiral> has no `d` at all
//   an LPE item's `d` is its INPUT, not what the canvas shows
//   a stroke has an outline the fill path does not describe
//
// Every one of those is a shape Inkscape computes and a plugin cannot. `pathData` answers
// with the resolved geometry, serialised as ordinary SVG path syntax, so a plugin that already
// has a path parser keeps using it.

/** Serialise a PathVector as an SVG `d` string result. */
#define INK_PATH_RESULT(pathv, first, operation)           \
    std::string const rendered = sp_svg_write_path(pathv); \
    INK_STRING_RESULT(rendered.c_str(), first, operation)

/**
 * The fill rule an element is actually drawn with.
 *
 * Not a detail that can be defaulted: every boolean and offset operation takes a fill rule, and
 * for a shape with a hole -- a donut, a letter "o", anything authored `fill-rule:evenodd` --
 * nonzero and even-odd disagree about which region is inside. Assuming nonzero produces a
 * plausible wrong answer rather than a failure, which is the worst kind, so the element's own
 * rule is read instead of chosen here.
 */
FillRule fillRuleOf(SPItem const *item)
{
    if (!item || !item->style) {
        return fill_nonZero;
    }
    switch (item->style->fill_rule.computed) {
        case SP_WIND_RULE_EVENODD:
            return fill_oddEven;
        case SP_WIND_RULE_POSITIVE:
            return fill_positive;
        default:
            return fill_nonZero;
    }
}

/** Read a DOMPoint argument (two doubles in the module's memory). */
bool readPoint(WasmInvocation *invocation, int32_t offset, Geom::Point &out)
{
    if (!invocation->spanIsValid(offset, 2 * static_cast<int32_t>(sizeof(double)))) {
        return false;
    }
    double values[2];
    memcpy(values, invocation->memoryBytes() + offset, sizeof values);
    out = Geom::Point(values[0], values[1]);
    return true;
}

/**
 * org.inkscape.SVGGeometryElement.tangentAtLength(element, length, out_offset) -> i32 present
 *
 * The direction of travel at a distance along the path, as a unit vector. getPointAtLength says
 * where; this says which way, and position without heading is half an answer for anything
 * placing an arrowhead, a tick or a label along a path.
 *
 * The same arc-length reparametrisation getPointAtLength uses, so the two agree about what
 * "distance along" means -- path time is not proportional to distance, and answering from the
 * raw time would put the heading somewhere other than the point.
 */
wasm_trap_t *tangentAtLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "tangentAtLength")
    auto const paths = item ? pathOf(item) : Geom::PathVector();
    if (paths.empty()) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    double remaining = args->data[1].of.f64;
    Geom::Point heading(1, 0);
    for (auto const &path : paths) {
        double const length = path.length();
        if (remaining <= length || &path == &paths.back()) {
            auto const uniform = Geom::arc_length_parametrization(path.toPwSb(), 2, 0.1);
            auto const at = std::max(0.0, std::min(remaining, uniform.domain().max()));
            auto const derivative = Geom::derivative(uniform).valueAt(at);
            if (derivative.length() > 0) {
                heading = Geom::unit_vector(derivative);
            }
            break;
        }
        remaining -= length;
    }

    double const values[2] = {heading.x(), heading.y()};
    if (!invocation->writeDoubles(args->data[2].of.i32, values, 2)) {
        return invocation->trap("tangentAtLength: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.selfIntersections(element) -> i32 count
 *
 * How many times the path crosses itself, which is a different question from where two paths
 * meet and the one worth asking before a boolean operation or an offset. 2geom answers it with
 * PathVector::intersectSelf and no Inkscape wrapper uses it.
 */
wasm_trap_t *selfIntersections(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "selfIntersections")
    auto const paths = item ? pathOf(item) : Geom::PathVector();
    results->data[0] = WASM_I32_VAL(paths.empty() ? 0 : static_cast<int32_t>(paths.intersectSelf().size()));
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.pathData(element, out, cap) -> i32
 *
 * The shape as drawn, for anything that has one. Absent when the element draws no path at all,
 * which a <g> or an <image> does not -- that is a fact about the element, not a failure.
 */
wasm_trap_t *pathData(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "pathData")
    auto const curve = item ? curve_for_item(item) : std::nullopt;
    if (!curve) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }
    INK_PATH_RESULT(*curve, 1, "pathData")
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.pathDataBeforeLPE(element, out, cap) -> i32
 *
 * The same shape with live path effects not applied. A plugin editing the path a user is still
 * shaping with an LPE has to write back to the input, and writing the output would discard the
 * effect -- so both are exposed rather than one being called "the" path.
 */
wasm_trap_t *pathDataBeforeLPE(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "pathDataBeforeLPE")
    auto const curve = item ? curve_for_item_before_LPE(item) : std::nullopt;
    if (!curve) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }
    INK_PATH_RESULT(*curve, 1, "pathDataBeforeLPE")
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.exactBounds(element, out, cap) -> i32
 *
 * The bounding SHAPE, not the axis-aligned box: a rotated rectangle's exact bounds are a
 * parallelogram, and its getBBox is the larger box containing it. A plugin packing or
 * collision-testing rotated objects gets a usable answer from one and a wasteful one from the
 * other.
 */
wasm_trap_t *exactBounds(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "exactBounds")
    auto const bounds = item ? item->documentExactBounds() : std::nullopt;
    if (!bounds) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }
    INK_PATH_RESULT(*bounds, 1, "exactBounds")
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.outline(element, exclude_markers, out, cap) -> i32
 *
 * The stroke as a filled shape -- what "stroke to path" produces, without performing it. A
 * plugin measuring or hit-testing against the drawn stroke needs this and cannot derive it:
 * the outline depends on width, join, cap, miter limit and dash pattern together.
 */
wasm_trap_t *outline(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "outline")
    if (!item) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }

    // Returns a fresh PathVector the caller owns, so it is adopted rather than leaked.
    std::unique_ptr<Geom::PathVector> const outlined(item_to_outline(item, args->data[1].of.i32 != 0));
    if (!outlined) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }
    INK_PATH_RESULT(*outlined, 2, "outline")
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.toPath(element, legacy) -> Element?
 *
 * Stroke to path, performed. Unlike the operations above this MUTATES: the element is replaced
 * by path objects, and the handle it answers is the replacement. The original handle is stale
 * afterwards and using it traps, which is the handle table doing its job rather than a hazard.
 *
 * `legacy` selects Inkscape's pre-1.0 conversion, which produces a single path where the
 * current one produces a group of fill, stroke and markers. A plugin reproducing an older
 * document's output needs the old shape, and the difference is not something it can undo
 * afterwards -- so it is asked for rather than assumed.
 */
wasm_trap_t *toPath(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "toPath")
    Inkscape::XML::Node *replacement = item ? item_to_paths(item, args->data[1].of.i32 != 0) : nullptr;
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, replacement));
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.simplify(element, threshold, justCoalesce) -> i32 changed
 *
 * Inkscape's own fitter, which is not a generic Ramer-Douglas-Peucker: it refits cubic segments
 * rather than dropping nodes, so the result keeps curvature a point-dropping simplifier loses.
 * The threshold is scaled by the item's own size, matching what the menu item does, so the same
 * number means the same thing on a large and a small object.
 *
 * `justCoalesce` merges nearly-coincident nodes without refitting the curve, which is the other
 * mode the same function offers and a different operation to want. It is a parameter rather
 * than a default because neither answer is the obvious one.
 */
wasm_trap_t *simplify(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "simplify")
    if (!item) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    // Scaled by the object's diagonal, so a threshold means the same on a large and a small
    // shape -- the same normalisation the menu item applies.
    auto const bounds = item->documentVisualBounds();
    double const size = bounds ? Geom::L2(bounds->dimensions()) : 1.0;
    results->data[0] = WASM_I32_VAL(path_simplify(item, args->data[1].of.f64, args->data[2].of.i32 != 0, size));
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.boolop(a, b, op, out, cap) -> i32
 *
 * Union, intersection, difference, symmetric difference, cut and slice over two elements'
 * resolved geometry, answering a `d` string and changing nothing. `sp_pathvector_boolop` is
 * callable without a selection or a desktop, so this works headless -- which the action-layer
 * equivalents do not.
 *
 * op is livarot's BooleanOp: 0 union, 1 intersection, 2 difference, 3 symmetric difference,
 * 4 cut, 5 slice. Inkscape's own numbering rather than a second one invented here.
 */
wasm_trap_t *boolop(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(first, "boolop")
    auto *second_node = invocation->getNode(args->data[1].of.i32);
    if (!second_node) {
        return invocation->trap("boolop: not a Node handle");
    }
    auto *second = invocation->itemFor(second_node);

    auto const left = first ? curve_for_item(first) : std::nullopt;
    auto const right = second ? curve_for_item(second) : std::nullopt;
    if (!left || !right) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }

    int32_t const op = args->data[2].of.i32;
    if (op < bool_op_union || op > bool_op_slice) {
        return invocation->trap("boolop: no such boolean operation");
    }

    // Both sides are taken in document space, because the two elements may sit under different
    // transforms and combining their own user-space coordinates would union shapes that are
    // nowhere near each other on the canvas.
    Geom::PathVector const a = *left * first->i2doc_affine();
    Geom::PathVector const b = *right * second->i2doc_affine();
    // Each operand's OWN fill rule, which is why sp_pathvector_boolop takes two of them.
    Geom::PathVector const combined =
        sp_pathvector_boolop(a, b, static_cast<BooleanOp>(op), fillRuleOf(first), fillRuleOf(second));
    INK_PATH_RESULT(combined, 3, "boolop")
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.offset(element, amount, join, miterLimit, out, cap) -> i32
 *
 * Grow or shrink a filled shape by a distance -- inset for a negative amount. Not the same as
 * scaling: an offset keeps every edge the same distance from the original, which is what a
 * margin or a cut line needs and what scaling gets wrong on anything that is not a circle.
 *
 * `join` is Inkscape::LineJoinType: 0 bevel, 1 round, 2 miter. It changes the result visibly at
 * every corner, so it is the caller's rather than a default chosen here; an out-of-range code
 * traps rather than quietly becoming miter. The fill rule is the element's own, for the reason
 * fillRuleOf() gives.
 */
wasm_trap_t *offset(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "offset")
    auto const curve = item ? curve_for_item(item) : std::nullopt;
    if (!curve) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }

    int32_t const join = args->data[2].of.i32;
    if (join < Inkscape::JOIN_BEVEL || join > Inkscape::JOIN_MITER) {
        return invocation->trap("offset: no such line join");
    }

    // The tolerance stays the host's: it is a numeric quality knob on the fitter, not a
    // property of the shape a plugin is describing, and 0.1 is what Inkscape's own offset
    // paths use.
    Geom::PathVector const offset = Inkscape::do_offset(*curve, args->data[1].of.f64, 0.1, args->data[3].of.f64,
                                                        fillRuleOf(item), static_cast<Inkscape::LineJoinType>(join));
    INK_PATH_RESULT(offset, 4, "offset")
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.nearestPoint(element, point_offset, out_offset) -> i32
 *
 * The closest point on the path to a given one, as a DOMPoint, plus its distance. What
 * "snap this to that edge" and "place a label along here" both start from, and what a plugin
 * approximates badly by sampling getPointAtLength in a loop.
 */
wasm_trap_t *nearestPoint(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "nearestPoint")
    auto const curve = item ? curve_for_item(item) : std::nullopt;
    Geom::Point query;
    if (!readPoint(invocation, args->data[1].of.i32, query)) {
        return invocation->trap("nearestPoint: point is outside the module's memory");
    }
    if (!curve || curve->empty()) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    double distance = 0.0;
    auto const time = curve->nearestTime(query, &distance);
    if (!time) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    Geom::Point const found = curve->pointAt(*time);
    double const answer[3] = {found[Geom::X], found[Geom::Y], distance};
    if (!invocation->writeDoubles(args->data[2].of.i32, answer, 3)) {
        return invocation->trap("nearestPoint: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.winding(element, point_offset) -> i32
 *
 * The winding number at a point. isPointInFill answers the fill-rule question; this answers the
 * one underneath it, which is what a plugin implementing its own rule -- or deciding whether a
 * subpath is a hole -- actually needs.
 */
wasm_trap_t *winding(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "winding")
    auto const curve = item ? curve_for_item(item) : std::nullopt;
    Geom::Point query;
    if (!readPoint(invocation, args->data[1].of.i32, query)) {
        return invocation->trap("winding: point is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(curve ? curve->winding(query) : 0);
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.intersect(a, b, out_offset, out_capacity) -> i32 count
 *
 * Where two paths cross, as a DOMPoint each. Returns how many crossings there ARE, and writes
 * them only when they all fit -- the same three-way rule the string results use, for the same
 * reason: a plugin cannot know the count in advance.
 */
wasm_trap_t *intersect(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(first, "intersect")
    auto *second_node = invocation->getNode(args->data[1].of.i32);
    if (!second_node) {
        return invocation->trap("intersect: not a Node handle");
    }
    auto *second = invocation->itemFor(second_node);

    auto const left = first ? curve_for_item(first) : std::nullopt;
    auto const right = second ? curve_for_item(second) : std::nullopt;
    if (!left || !right) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    Geom::PathVector const a = *left * first->i2doc_affine();
    Geom::PathVector const b = *right * second->i2doc_affine();
    auto const crossings = a.intersect(b);

    auto const count = static_cast<int32_t>(crossings.size());
    auto const capacity = args->data[3].of.i32;
    if (count > 0 && count <= capacity) {
        std::vector<double> points;
        points.reserve(crossings.size() * 2);
        for (auto const &crossing : crossings) {
            Geom::Point const at = crossing.point();
            points.push_back(at[Geom::X]);
            points.push_back(at[Geom::Y]);
        }
        if (!invocation->writeDoubles(args->data[2].of.i32, points.data(), static_cast<int>(points.size()))) {
            return invocation->trap("intersect: result is outside the module's memory");
        }
    }
    results->data[0] = WASM_I32_VAL(count);
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.collidesWith(element, other) -> i32
 *
 * Whether two items visually overlap, tested against their real shapes rather than their boxes.
 * Two rotated objects whose boxes overlap may not touch at all, which is the case every
 * packing or layout plugin has to get right.
 */
wasm_trap_t *collidesWith(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(first, "collidesWith")
    auto *second_node = invocation->getNode(args->data[1].of.i32);
    if (!second_node) {
        return invocation->trap("collidesWith: not a Node handle");
    }
    auto *second = invocation->itemFor(second_node);
    results->data[0] = WASM_I32_VAL(first && second && first->collidesWith(*second) ? 1 : 0);
    return nullptr;
}

// ── Item transforms, and what an item is made of ────────────────────────────────────────

/**
 * org.inkscape.SVGGraphicsElement.transform(element, out_offset) -> i32 present
 *
 * The element's OWN transform, as a DOMMatrix. getCTM composes every ancestor's; this is the
 * one written on this element, which is what a plugin adjusting an object's placement edits.
 */
wasm_trap_t *transform(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "transform")
    if (!item) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    if (!writeAffine(invocation, args->data[1].of.i32, item->transform)) {
        return invocation->trap("transform: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.applyTransform(element, matrix_offset, compensate)
 *
 * Write a transform the way the editor does, rather than setting the attribute. The difference
 * is compensation: Inkscape adjusts stroke width, gradients, patterns and clip paths so the
 * object looks scaled rather than distorted, and honours the user's preferences about which of
 * those should follow. SVGElement.setTransform writes the raw attribute and does none of it
 * -- both are offered because both are wanted, and which is right depends on whether the
 * plugin is editing markup or moving an object.
 */
wasm_trap_t *applyTransform(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_ITEM_ARG(item, "applyTransform")
    int32_t const offset = args->data[1].of.i32;
    if (!invocation->spanIsValid(offset, 6 * static_cast<int32_t>(sizeof(double)))) {
        return invocation->trap("applyTransform: matrix is outside the module's memory");
    }
    double values[6];
    memcpy(values, invocation->memoryBytes() + offset, sizeof values);

    if (item) {
        Geom::Affine const affine(values[0], values[1], values[2], values[3], values[4], values[5]);
        item->doWriteTransform(affine, nullptr, args->data[2].of.i32 != 0);
    }
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.relativeTransform(element, ancestor, out_offset) -> i32
 *
 * The transform taking this element's user space to another element's. Composing it by hand
 * means multiplying every getCTM between them and inverting one -- arithmetic Inkscape has
 * already done, and easy to get subtly wrong in a way that only shows up under nesting.
 */
wasm_trap_t *relativeTransform(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "relativeTransform")
    auto *other_node = invocation->getNode(args->data[1].of.i32);
    if (!other_node) {
        return invocation->trap("relativeTransform: not a Node handle");
    }
    auto *other = invocation->objectFor(other_node);
    if (!item || !other) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    if (!writeAffine(invocation, args->data[2].of.i32, item->getRelativeTransform(other))) {
        return invocation->trap("relativeTransform: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.rotationCenter(element, out_offset) -> i32 set
 *
 * Where rotation happens from. The point is always written -- it defaults to the bounding box
 * centre -- and the result says whether the user MOVED it, which is a different fact and the
 * one a plugin needs before deciding to overwrite it.
 */
wasm_trap_t *rotationCenter(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "rotationCenter")
    if (!item) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    Geom::Point const centre = item->getCenter();
    double const values[2] = {centre[Geom::X], centre[Geom::Y]};
    if (!invocation->writeDoubles(args->data[1].of.i32, values, 2)) {
        return invocation->trap("rotationCenter: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(item->isCenterSet() ? 1 : 0);
    return nullptr;
}

/** org.inkscape.SVGGraphicsElement.setRotationCenter(element, point_offset) */
wasm_trap_t *setRotationCenter(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_ITEM_ARG(item, "setRotationCenter")
    Geom::Point centre;
    if (!readPoint(invocation, args->data[1].of.i32, centre)) {
        return invocation->trap("setRotationCenter: point is outside the module's memory");
    }
    if (item) {
        item->setCenter(centre);
    }
    return nullptr;
}

/** org.inkscape.SVGGraphicsElement.unsetRotationCenter(element) */
wasm_trap_t *unsetRotationCenter(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_ITEM_ARG(item, "unsetRotationCenter")
    if (item) {
        item->unsetCenter();
    }
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.clipPath(element) -> Element?
 * org.inkscape.SVGGraphicsElement.maskObject(element) -> Element?
 *
 * The clip path and mask applied to this element, resolved through their references. Reading
 * clip-path="url(#x)" and looking the id up by hand works until the reference is inherited,
 * relative, or points into another document.
 */
wasm_trap_t *clipPath(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "clipPath")
    auto *clip = item ? item->getClipObject() : nullptr;
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, clip ? clip->getRepr() : nullptr));
    return nullptr;
}

wasm_trap_t *maskObject(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "maskObject")
    auto *mask = item ? item->getMaskObject() : nullptr;
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, mask ? mask->getRepr() : nullptr));
    return nullptr;
}

/**
 * org.inkscape.SVGGeometryElement.markers(element) -> NodeList
 *
 * The marker elements actually drawn on this shape. Reading marker-start, marker-mid and
 * marker-end gives three references and not the answer: which markers appear depends on how
 * many nodes the path has, and Inkscape has already worked that out.
 */
wasm_trap_t *markers(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "markers")
    std::vector<SPObject *> markers;
    if (auto *shape = cast<SPShape>(item)) {
        for (auto const &[location, marker, transform] : shape->get_markers()) {
            if (marker) {
                markers.push_back(marker);
            }
        }
    }
    results->data[0] = WASM_I32_VAL(nodesOfObjects(invocation, markers));
    return nullptr;
}

/**
 * org.inkscape.SVGGraphicsElement.pathEffects(element) -> DOMStringList
 *
 * The live path effects on this element, by name. A plugin editing `d` on an LPE item is
 * editing the effect's input, and the visible result will be recomputed from it -- so knowing
 * whether any effect is present, and which, is what stops a plugin silently fighting one.
 */
wasm_trap_t *pathEffects(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "pathEffects")
    std::vector<std::string> names;
    if (auto *lpe_item = cast<SPLPEItem>(item)) {
        for (auto const *effect : lpe_item->getPathEffects()) {
            if (effect) {
                names.push_back(effect->getName().raw());
            }
        }
    }
    results->data[0] = WASM_I32_VAL(invocation->makeStringList(std::move(names)));
    return nullptr;
}

/**
 * org.inkscape.CSSStyleDeclaration.paintServer(element, which) -> Element?
 *
 * The gradient or pattern an element paints with, resolved. getPropertyValue answers
 * "url(#grad)", which is a string a plugin then has to parse and look up; this is the element.
 *
 * which: 0 fill, 1 stroke.
 */
wasm_trap_t *paintServer(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "paintServer")
    SPPaintServer *server = nullptr;
    if (item && item->style) {
        server = args->data[1].of.i32 == 1 ? item->style->getStrokePaintServer() : item->style->getFillPaintServer();
    }
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, server ? server->getRepr() : nullptr));
    return nullptr;
}

// ── Text metrics ────────────────────────────────────────────────────────────────────────
//
// The gap inkex documents in its own source as impossible: its Text.shape_box comments say
// it returns "a horrible bounding box that just contains the coord points of the text
// without width or height (which is impossible to calculate)", and carries the literal line
// `x2 = self.x + 0  # XXX This is impossible to calculate!`.
//
// It is not impossible; it just requires the shaped layout, which lives in this process.
// Inkscape::Text::Layout already answers every one of these questions -- the correspondence
// with SVG2's SVGTextContentElement is close to exact, only spelled differently.

/** The shaped layout of a text element, or nullptr if it is not one. */
Inkscape::Text::Layout const *layoutOf(SPItem *item)
{
    if (auto *text = cast<SPText>(item)) {
        return &text->layout;
    }
    if (auto *flowtext = cast<SPFlowtext>(item)) {
        return &flowtext->layout;
    }
    return nullptr;
}

/** Unpack the layout of a text element, answering @a fallback when it is not one. */
#define INK_LAYOUT_ARG(name, fallback)                                                    \
    auto *invocation = invocationOf(env);                                                 \
    auto *name##_node = invocation->getNode(args->data[0].of.i32);                        \
    if (!name##_node) {                                                                   \
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str()); \
    }                                                                                     \
    auto const *name = layoutOf(invocation->itemFor(name##_node));                        \
    if (!name) {                                                                          \
        results->data[0] = fallback;                                                      \
        return nullptr;                                                                   \
    }

/** org.inkscape.SVGTextContentElement.getNumberOfChars(element) -> i32 */
wasm_trap_t *getNumberOfChars(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_I32_VAL(0))
    results->data[0] = WASM_I32_VAL(layout->iteratorToCharIndex(layout->end()));
    return nullptr;
}

/** org.inkscape.SVGTextContentElement.getComputedTextLength(element) -> f64 */
wasm_trap_t *getComputedTextLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_F64_VAL(0.0))
    auto const box = layout->bounds(Geom::identity());
    results->data[0] = WASM_F64_VAL(box ? box->width() : 0.0);
    return nullptr;
}

/**
 * org.inkscape.SVGTextContentElement.getSubStringLength(element, charnum, nchars) -> f64
 *
 * Layout::bounds() already takes a start and a length, so the substring case is the same
 * call with the range filled in rather than a separate mechanism.
 */
wasm_trap_t *getSubStringLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_F64_VAL(0.0))
    int32_t const count = args->data[2].of.i32;
    // No characters have no length. Layout::bounds() reads a non-positive length as "to the
    // end", which is the opposite answer, so the empty case is decided here rather than
    // handed to a function that means something else by it.
    if (count <= 0) {
        results->data[0] = WASM_F64_VAL(0.0);
        return nullptr;
    }
    auto const box = layout->bounds(Geom::identity(), false, args->data[1].of.i32, count);
    results->data[0] = WASM_F64_VAL(box ? box->width() : 0.0);
    return nullptr;
}

/** Shared by getStartPositionOfChar and getEndPositionOfChar, which differ only in the index. */
wasm_trap_t *characterPosition(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results, int offset)
{
    INK_LAYOUT_ARG(layout, WASM_I32_VAL(0))
    int const index = args->data[1].of.i32 + offset;
    if (index < 0 || index > layout->iteratorToCharIndex(layout->end())) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    auto const point = layout->characterAnchorPoint(layout->charIndexToIterator(index));
    double const values[2] = {point.x(), point.y()};
    if (!invocation->writeDoubles(args->data[2].of.i32, values, 2)) {
        return invocation->trap("character position: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/** org.inkscape.SVGTextContentElement.getStartPositionOfChar(element, charnum, out) -> i32 present */
wasm_trap_t *getStartPositionOfChar(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    return characterPosition(env, args, results, 0);
}

/** org.inkscape.SVGTextContentElement.getEndPositionOfChar(element, charnum, out) -> i32 present */
wasm_trap_t *getEndPositionOfChar(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    // The end of one character is the anchor of the next, which is what makes the last
    // character's end the layout's end iterator rather than a special case.
    return characterPosition(env, args, results, 1);
}

/** org.inkscape.SVGTextContentElement.getExtentOfChar(element, charnum, out) -> i32 present */
wasm_trap_t *getExtentOfChar(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_I32_VAL(0))
    int const index = args->data[1].of.i32;
    if (index < 0 || index >= layout->iteratorToCharIndex(layout->end())) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    auto const box = layout->characterBoundingBox(layout->charIndexToIterator(index));
    double const rect[4] = {box.left(), box.top(), box.width(), box.height()};
    if (!invocation->writeDoubles(args->data[2].of.i32, rect, 4)) {
        return invocation->trap("getExtentOfChar: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/** org.inkscape.SVGTextContentElement.getRotationOfChar(element, charnum) -> f64 */
wasm_trap_t *getRotationOfChar(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_F64_VAL(0.0))
    int const index = args->data[1].of.i32;
    if (index < 0 || index >= layout->iteratorToCharIndex(layout->end())) {
        results->data[0] = WASM_F64_VAL(0.0);
        return nullptr;
    }

    // The rotation comes back through characterBoundingBox's out-parameter, so the box is
    // computed and discarded; there is no cheaper way to ask.
    double rotation = 0.0;
    layout->characterBoundingBox(layout->charIndexToIterator(index), &rotation);
    results->data[0] = WASM_F64_VAL(rotation);
    return nullptr;
}

/** org.inkscape.SVGTextContentElement.getCharNumAtPosition(element, x, y) -> i32 */
wasm_trap_t *getCharNumAtPosition(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_I32_VAL(-1))
    auto const iterator = layout->getNearestCursorPositionTo(args->data[1].of.f64, args->data[2].of.f64);
    // SVG says -1 for "no character there"; the layout answers with its end iterator.
    int const index = layout->iteratorToCharIndex(iterator);
    results->data[0] = WASM_I32_VAL(index >= layout->iteratorToCharIndex(layout->end()) ? -1 : index);
    return nullptr;
}

/**
 * org.inkscape.SVGTextContentElement.toPath(element, out, cap) -> i32
 *
 * The glyph outlines of the shaped run, as path data. Producing these requires the font, its
 * hinting and the shaping already applied to this text; a plugin has none of that, which is
 * why text-to-path extensions either ship a font engine of their own or run a second Inkscape.
 *
 * Absent, not empty, when the element is not text: an empty path would say the text has no
 * glyphs rather than that there is no text.
 */
wasm_trap_t *textToPath(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_I32_VAL(WasmInvocation::STRING_ABSENT))
    INK_PATH_RESULT(layout->convertToCurves(), 1, "toPath")
    return nullptr;
}

/**
 * org.inkscape.SVGTextContentElement.textString(element, out, cap) -> i32
 *
 * The characters of the element, with line breaks between lines. Reading textContent gives the
 * markup's text nodes, which for text split across tspans is neither in visual order nor
 * separated where the lines are.
 */
wasm_trap_t *textString(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *node = invocation->getNode(args->data[0].of.i32);
    if (!node) {
        return invocation->trap("textString: not a Node handle");
    }
    auto *item = invocation->itemFor(node);
    if (!item || !layoutOf(item)) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }

    std::string const text = sp_te_get_string_multiline(item).raw();
    INK_STRING_RESULT(text.c_str(), 1, "textString")
    return nullptr;
}

/**
 * org.inkscape.SVGTextContentElement.styleAtPosition(element, charnum, out, cap) -> i32
 *
 * The style in force at one character, serialised as CSS. This is not the element's own style:
 * a tspan overriding fill or font-size applies from its first character, so the answer changes
 * along the run.
 */
wasm_trap_t *styleAtPosition(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *node = invocation->getNode(args->data[0].of.i32);
    if (!node) {
        return invocation->trap("styleAtPosition: not a Node handle");
    }
    auto *item = invocation->itemFor(node);
    auto const *layout = layoutOf(item);
    if (!layout) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }

    auto const *style = sp_te_style_at_position(item, layout->charIndexToIterator(args->data[1].of.i32));
    if (!style) {
        results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
        return nullptr;
    }
    // ALWAYS, not the default IFSET: the style at a position is the resolved one, whose
    // properties are not flagged as locally set, so IFSET writes nothing at all. Every
    // property in force is the answer to the question being asked.
    std::string const css = style->write(SP_STYLE_FLAG_ALWAYS).raw();
    INK_STRING_RESULT(css.c_str(), 2, "styleAtPosition")
    return nullptr;
}

/**
 * org.inkscape.SVGTextContentElement.baselines(element, out_offset, out_capacity) -> i32
 *
 * One baseline segment per line, four doubles each: x1, y1, x2, y2. Where the lines fell is a
 * result of shaping and wrapping, so it cannot be derived from the markup even when every
 * tspan carries an explicit y.
 *
 * Returns how many segments there ARE and writes them only when they all fit, the same rule
 * the string results use.
 */
wasm_trap_t *baselines(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_I32_VAL(0))

    auto const baselines = layout->getBaselines();
    auto const count = static_cast<int32_t>(baselines.size());
    if (count > 0 && count <= args->data[2].of.i32) {
        std::vector<double> values;
        values.reserve(baselines.size() * 4);
        for (auto const &segment : baselines) {
            values.push_back(segment.initialPoint()[Geom::X]);
            values.push_back(segment.initialPoint()[Geom::Y]);
            values.push_back(segment.finalPoint()[Geom::X]);
            values.push_back(segment.finalPoint()[Geom::Y]);
        }
        if (!invocation->writeDoubles(args->data[1].of.i32, values.data(), static_cast<int>(values.size()))) {
            return invocation->trap("baselines: result is outside the module's memory");
        }
    }
    results->data[0] = WASM_I32_VAL(count);
    return nullptr;
}

/** org.inkscape.SVGTextContentElement.lineCount(element) -> i32 */
wasm_trap_t *lineCount(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_I32_VAL(0))
    results->data[0] = WASM_I32_VAL(static_cast<int32_t>(layout->getBaselines().size()));
    return nullptr;
}

/**
 * org.inkscape.SVGTextContentElement.fontFamily(element, span, out, cap) -> i32
 *
 * The family a span was actually shaped with, which is the entry of the font-family list that
 * resolved on this machine rather than the list the document asked for.
 */
wasm_trap_t *fontFamily(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_LAYOUT_ARG(layout, WASM_I32_VAL(WasmInvocation::STRING_ABSENT))
    std::string const family = layout->getFontFamily(static_cast<unsigned>(args->data[1].of.i32)).raw();
    INK_STRING_RESULT(family.c_str(), 2, "fontFamily")
    return nullptr;
}

// ── Hit testing ─────────────────────────────────────────────────────────────────────────

/** Turn a list of items into a NodeList handle over their reprs. */
int32_t nodesOf(WasmInvocation *invocation, std::vector<SPItem *> const &items)
{
    std::vector<Inkscape::XML::Node *> nodes;
    nodes.reserve(items.size());
    for (auto *item : items) {
        if (auto *repr = item->getRepr()) {
            nodes.push_back(repr);
        }
    }
    return invocation->makeNodeSnapshot(std::move(nodes));
}

/** Read a DOMRect argument (four doubles in the module's memory). */
bool readRect(WasmInvocation *invocation, int32_t offset, Geom::Rect &out)
{
    if (!invocation->spanIsValid(offset, 4 * static_cast<int32_t>(sizeof(double)))) {
        return false;
    }
    double values[4];
    memcpy(values, invocation->memoryBytes() + offset, sizeof values);
    out = Geom::Rect::from_xywh(values[0], values[1], values[2], values[3]);
    return true;
}

/**
 * org.inkscape.SVGSVGElement.getEnclosureList(rect_offset) -> NodeList
 * org.inkscape.SVGSVGElement.getIntersectionList(rect_offset) -> NodeList
 *
 * SVG passes a reference element to select a subtree; this takes the document as given,
 * which is the whole-document case every caller actually uses. A subtree-scoped version can
 * be added when something needs it rather than guessed at now.
 */
wasm_trap_t *hitTest(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results, bool enclosed)
{
    auto *invocation = invocationOf(env);
    auto *document = invocation->document();
    Geom::Rect box;
    if (!document || !readRect(invocation, args->data[0].of.i32, box)) {
        return invocation->trap("hit test: rectangle is outside the module's memory");
    }

    document->ensureUpToDate();
    auto const items = enclosed ? document->getItemsInBox(0, box) : document->getItemsPartiallyInBox(0, box);
    results->data[0] = WASM_I32_VAL(nodesOf(invocation, items));
    return nullptr;
}

wasm_trap_t *getEnclosureList(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    return hitTest(env, args, results, true);
}

wasm_trap_t *getIntersectionList(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    return hitTest(env, args, results, false);
}

/** org.inkscape.SVGSVGElement.checkEnclosure(element, rect_offset) -> i32 */
wasm_trap_t *checkEnclosure(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    Geom::Rect box;
    if (!readRect(invocation, args->data[1].of.i32, box)) {
        return invocation->trap("checkEnclosure: rectangle is outside the module's memory");
    }
    auto const bounds = item ? item->documentVisualBounds() : Geom::OptRect();
    results->data[0] = WASM_I32_VAL(bounds && box.contains(*bounds) ? 1 : 0);
    return nullptr;
}

/** org.inkscape.SVGSVGElement.checkIntersection(element, rect_offset) -> i32 */
wasm_trap_t *checkIntersection(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    Geom::Rect box;
    if (!readRect(invocation, args->data[1].of.i32, box)) {
        return invocation->trap("checkIntersection: rectangle is outside the module's memory");
    }
    auto const bounds = item ? item->documentVisualBounds() : Geom::OptRect();
    results->data[0] = WASM_I32_VAL(bounds && box.intersects(*bounds) ? 1 : 0);
    return nullptr;
}

// ── Computed style ──────────────────────────────────────────────────────────────────────
//
// The cascade, which inkex approximates with cascaded_style() in four files. SVG2 defers
// computed style to window.getComputedStyle(); there is no window, so the entry point is on
// Document -- the smallest honest adaptation. Values are strings: CSSValue is deprecated and
// was never fully implemented anywhere.

/**
 * The computed value of a style property, as a string.
 *
 * SPIBase::get_value() returns the SPECIFIED value: the notation the author wrote. CSSOM
 * defines a computed value, which for a colour is one notation regardless of how it was
 * spelled, so `rebeccapurple`, `rgb(102,51,153)` and `#663399` must all come back the same.
 *
 * Handing back the author's spelling under the name "computed" would be the wrong value with
 * the right label, and would leave a plugin parsing colour names -- for a value Inkscape had
 * already resolved and was simply not passing on. The resolved colour is right there in
 * SPIPaint; this is the difference between an interface derived from the specification and
 * one that reports whatever the internals happen to hold.
 */
std::string computedValue(SPIBase const *property)
{
    if (auto const *paint = dynamic_cast<SPIPaint const *>(property); paint && paint->isColor()) {
        // Not toString(): a colour parsed from a name lives in the named colour space and
        // serialises back to that name, which is right for saving a document -- the author's
        // `rebeccapurple` survives a round trip -- and wrong for a computed value, which
        // CSSOM defines as rgb(). So the components are taken and formatted directly.
        uint32_t const rgba = paint->getColor().toRGBA();
        return "rgb(" + std::to_string((rgba >> 24) & 0xff) + ", " + std::to_string((rgba >> 16) & 0xff) + ", " +
               std::to_string((rgba >> 8) & 0xff) + ")";
    }
    return property->get_value().raw();
}

/**
 * org.inkscape.Document.getComputedStyle(element) -> DOMStringList of "name:value"
 *
 * Every property, not only the ones this element declares. That is what makes it a computed
 * style: an element inheriting `fill` from a group has a computed fill, and a list that omits
 * it is the SPECIFIED style wearing the computed style's name -- which sends a plugin back to
 * walking ancestors by hand, the exact thing being in-process removes the need for.
 *
 * This filtered on `set` until the case for it was written. getPropertyValue() next door had
 * always been unfiltered and carried the reasoning above in its own comment, so the two
 * operations on one object disagreed about what "the properties" are; the tests missed it
 * because the only case calling this one used an element that declared what it was asked for.
 * "What did this element itself declare" is a real question and has its own operation now,
 * specifiedValue().
 */
wasm_trap_t *getComputedStyle(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    std::vector<std::string> properties;
    if (item && item->style) {
        for (auto const *property : item->style->properties()) {
            if (property) {
                properties.push_back(property->name() + ":" + computedValue(property));
            }
        }
    }
    results->data[0] = WASM_I32_VAL(invocation->makeStringList(std::move(properties)));
    return nullptr;
}

/** org.inkscape.CSSStyleDeclaration.getPropertyValue(element, name_off, name_len, out, cap) -> i32 */
wasm_trap_t *getPropertyValue(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    INK_STRING_ARG(wanted, 1, "getPropertyValue")

    // Resolved before answering, so that the found and absent paths leave through the same
    // write: the span the guest offered is then checked on both, and not only on the path
    // where there happened to be something to say.
    std::string value;
    bool found = false;
    if (item && item->style) {
        for (auto const *property : item->style->properties()) {
            // Deliberately not filtered on `set`. A property the element inherits rather than
            // declares still has a computed value, and reporting nothing for it would make
            // this a specified-style lookup wearing the name of a computed one -- the exact
            // thing that sends a plugin back to walking ancestors itself, which is what
            // being in-process is supposed to make unnecessary.
            if (property && property->name() == wanted.c_str()) {
                value = computedValue(property);
                found = true;
                break;
            }
        }
    }
    INK_STRING_RESULT(found ? value.c_str() : nullptr, 3, "getPropertyValue")
    return nullptr;
}

/**
 * org.inkscape.CSSStyleDeclaration.length(element) -> i32
 * org.inkscape.CSSStyleDeclaration.item(element, index, out_offset, out_capacity) -> i32
 *
 * Enumerate the properties by index, which is CSSOM's own way of walking a declaration.
 *
 * Unfiltered, matching getPropertyValue(): every property has a computed value, whether or
 * not this element declared one. getComputedStyle() above lists only the properties with
 * `set` true, so the two disagree about what "the properties" are -- that predates this work
 * and is left alone here rather than changed underneath the cases that pin it.
 */
wasm_trap_t *styleLength(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "length")
    auto const count = item && item->style ? item->style->properties().size() : 0u;
    results->data[0] = WASM_I32_VAL(static_cast<int32_t>(count));
    return nullptr;
}

wasm_trap_t *styleItem(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_ITEM_ARG(item, "item")

    char const *name = nullptr;
    std::string held;
    if (item && item->style) {
        auto const &properties = item->style->properties();
        int32_t const index = args->data[1].of.i32;
        if (index >= 0 && static_cast<size_t>(index) < properties.size() && properties[index]) {
            held = properties[index]->name().raw();
            name = held.c_str();
        }
    }
    INK_STRING_RESULT(name, 2, "item")
    return nullptr;
}

/**
 * The style operations that WRITE go through the repr, not the object.
 *
 * A plugin setting `fill` is editing the markup, and the object tree follows by observation.
 * Going the other way -- writing into SPStyle -- would put the two out of step until something
 * happened to resync them, and would not survive being saved.
 *
 * They take a Node rather than an Element handle for the same reason setAttribute does: the
 * repr is generic XML, and an element Inkscape has no class for still has a style attribute.
 */
#define INK_CSS_ARG(node, property, operation)                    \
    auto *invocation = invocationOf(env);                         \
    auto *node = invocation->getNode(args->data[0].of.i32);       \
    if (!node) {                                                  \
        return invocation->trap(operation ": not a Node handle"); \
    }                                                             \
    INK_STRING_ARG(property, 1, operation)

/** org.inkscape.CSSStyleDeclaration.setProperty(element, name_off, name_len, value_off, value_len) */
wasm_trap_t *setProperty(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_CSS_ARG(node, property, "setProperty")
    INK_STRING_ARG(value, 3, "setProperty")

    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_set_property(css, property.c_str(), value.c_str());
    // change() rather than set(): it merges into whatever style the element already carries,
    // where set() would replace the lot and silently drop every other property.
    sp_repr_css_change(node, css, "style");
    sp_repr_css_attr_unref(css);
    return nullptr;
}

/**
 * org.inkscape.CSSStyleDeclaration.setPropertyNumber(element, name_off, name_len, value)
 *
 * A number written the way Inkscape writes numbers into CSS. Without it every plugin setting a
 * stroke width or an opacity carries its own float formatter, and they disagree in the last
 * digit.
 */
wasm_trap_t *setPropertyNumber(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_CSS_ARG(node, property, "setPropertyNumber")

    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_set_property_double(css, property.c_str(), args->data[3].of.f64);
    sp_repr_css_change(node, css, "style");
    sp_repr_css_attr_unref(css);
    return nullptr;
}

/**
 * org.inkscape.CSSStyleDeclaration.setPropertyColor(element, name_off, name_len, rgba)
 *
 * A colour from the RGBA word every colour-valued operation here answers with, which is the
 * form paramColor hands a colour parameter over in. Fully opaque colours come out as plain
 * #rrggbb; anything else carries its alpha. That spelling is Color::toString's decision, and
 * not one worth making a second time in every plugin.
 */
wasm_trap_t *setPropertyColor(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_CSS_ARG(node, property, "setPropertyColor")

    auto const rgba = static_cast<uint32_t>(args->data[3].of.i32);
    auto const colour = Inkscape::Colors::Color(rgba, true);
    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_set_property_string(css, property.c_str(), colour.toString(false));
    sp_repr_css_change(node, css, "style");
    sp_repr_css_attr_unref(css);
    return nullptr;
}

/** org.inkscape.CSSStyleDeclaration.removeProperty(element, name_off, name_len) */
wasm_trap_t *removeProperty(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_CSS_ARG(node, property, "removeProperty")

    // unset_property, not set_property(nullptr). Removal here is a sentinel, not an absence:
    // unset_property writes "inkscape:unset" and sp_repr_css_write_string skips any property
    // carrying it, so the property is dropped when the style attribute is rebuilt. Setting
    // null instead leaves the property out of the merge set altogether, which means `change`
    // has nothing to override with and the existing value survives untouched.
    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_unset_property(css, property.c_str());
    sp_repr_css_change(node, css, "style");
    sp_repr_css_attr_unref(css);
    return nullptr;
}

/**
 * org.inkscape.CSSStyleDeclaration.specifiedValue(element, name_off, name_len, out, cap) -> i32
 *
 * What this element itself declares, as opposed to what it computes to. The difference is the
 * whole cascade: getPropertyValue() answers for an element that inherits `fill` from a group,
 * this answers absent. A plugin deciding whether to overwrite something the author set needs
 * the second question, and cannot get it from the first.
 */
wasm_trap_t *specifiedValue(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_CSS_ARG(node, property, "specifiedValue")

    SPCSSAttr *css = sp_repr_css_attr(node, "style");
    char const *value = css ? sp_repr_css_property(css, property.c_str(), nullptr) : nullptr;
    std::string const held(value ? value : "");
    // Arg 3, not 2: the property name occupies args 1 and 2, so the out pair starts after it.
    INK_STRING_RESULT(value ? held.c_str() : nullptr, 3, "specifiedValue")
    if (css) {
        sp_repr_css_attr_unref(css);
    }
    return nullptr;
}

/**
 * org.inkscape.CSSStyleDeclaration.changeRecursive(element, name_off, name_len, value_off, value_len)
 *
 * The same as setProperty, applied down the subtree. Inkscape has this because setting a
 * property on a group does nothing visible when the children declare their own -- which is
 * what "make this whole selection red" runs into, and what BlurEdge does per step.
 */
wasm_trap_t *changeRecursive(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_CSS_ARG(node, property, "changeRecursive")
    INK_STRING_ARG(value, 3, "changeRecursive")

    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_set_property(css, property.c_str(), value.c_str());
    sp_repr_css_change_recursive(node, css, "style");
    sp_repr_css_attr_unref(css);
    return nullptr;
}

// ── Session tier ────────────────────────────────────────────────────────────────────────
//
// Vendor surface, named with an `ink` prefix so it can never be mistaken for standard SVG or
// DOM. The document model is standardised; the editing-session model never has been, by
// anyone, and pretending otherwise would be inventing a specification.
//
// It stays small because most apparent session state turns out to be document state: the
// current layer is an attribute on <sodipodi:namedview>, pages are ordinary elements, and
// undo is owned by the host and never visible here at all.

/** org.inkscape.Params.paramString(name_off, name_len, out, cap) -> i32 */
wasm_trap_t *paramString(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(name, 0, "paramString")
    auto const *effect = invocation->extension();
    char const *value = effect ? effect->get_param_string(name.c_str(), nullptr) : nullptr;
    INK_STRING_RESULT(value, 2, "paramString")
    return nullptr;
}

/**
 * The numeric parameter accessors.
 *
 * Each answers a documented default when the parameter is absent rather than trapping: a
 * missing parameter is a mistake in the .inx, and a plugin that cannot start is less useful
 * than one that starts with zero.
 */
#define INK_PARAM_GETTER(function, operation, getter, wrap, fallback)                        \
    wasm_trap_t *function(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)    \
    {                                                                                        \
        auto *invocation = invocationOf(env);                                                \
        INK_STRING_ARG(name, 0, operation)                                                   \
        auto const *effect = invocation->extension();                                        \
        results->data[0] = wrap(effect ? effect->getter(name.c_str(), fallback) : fallback); \
        return nullptr;                                                                      \
    }

INK_PARAM_GETTER(paramFloat, "paramFloat", get_param_float, WASM_F64_VAL, 0.0)
INK_PARAM_GETTER(paramInt, "paramInt", get_param_int, WASM_I32_VAL, 0)
INK_PARAM_GETTER(paramBool, "paramBool", get_param_bool, WASM_I32_VAL, false)

/**
 * org.inkscape.Selection.isSelected(element) -> i32
 *
 * Per element rather than a list to walk, which is Blender's model: selection is a property
 * of the thing selected (BezTriple.select_control_point), not a separate structure the
 * caller has to keep in step. Only the interface is per-element; the backing is Inkscape's
 * existing Selection, unchanged.
 */
wasm_trap_t *isSelected(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    auto *selection = invocation->selection();
    results->data[0] = WASM_I32_VAL(item && selection && selection->includes(item) ? 1 : 0);
    return nullptr;
}

/** org.inkscape.Selection.selectedIds() -> DOMStringList */
wasm_trap_t *selectedIds(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    std::vector<std::string> ids;
    if (auto *selection = invocation->selection()) {
        for (auto const &id : selection->getState().selected_ids) {
            ids.push_back(id);
        }
    }
    results->data[0] = WASM_I32_VAL(invocation->makeStringList(std::move(ids)));
    return nullptr;
}

/**
 * org.inkscape.Selection.selectionNodes() -> NodeList
 *
 * The selection as elements. "For each selected object" opens nearly every extension ever
 * written, so this is the shape that work actually takes; selectedIds answers the same
 * selection as text, which is the right answer for reporting and the wrong one for editing --
 * it makes every such plugin round-trip through getElementById, and an element carrying no id
 * cannot be reached that way at all.
 *
 * The counterpart to selectionSet, which has always taken a NodeList.
 */
wasm_trap_t *selectionNodes(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    std::vector<SPObject *> objects;
    if (auto *selection = invocation->selection()) {
        for (auto *item : selection->items()) {
            objects.push_back(item);
        }
    }
    results->data[0] = WASM_I32_VAL(nodesOfObjects(invocation, objects));
    return nullptr;
}

/** org.inkscape.Selection.selectedNodeCount(element) -> i32 */
wasm_trap_t *selectedNodeCount(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *node = invocation->getNode(args->data[0].of.i32);
    if (!node) {
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *selection = invocation->selection();
    int32_t count = 0;
    if (selection) {
        char const *id = node->attribute("id");
        for (auto const &selected : selection->getState().selected_nodes) {
            if (id && selected.path_id == id) {
                ++count;
            }
        }
    }
    results->data[0] = WASM_I32_VAL(count);
    return nullptr;
}

/**
 * org.inkscape.Selection.selectedNode(element, index, out_offset) -> i32 present
 *
 * Writes the subpath and node indices as two i32s.
 *
 * These are positional, unavoidably: an SVG path node is an offset into the `d` attribute,
 * not an element, so there is nothing to hang a flag on and Blender's model does not carry
 * over. The indices are therefore only meaningful until `element` is modified -- rewriting
 * `d` with a different node count invalidates them. Documented rather than silently unsafe,
 * which is what --selected-nodes on the command line is today.
 */
wasm_trap_t *selectedNode(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *node = invocation->getNode(args->data[0].of.i32);
    if (!node) {
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *selection = invocation->selection();
    int32_t const wanted = args->data[1].of.i32;

    if (selection && wanted >= 0) {
        char const *id = node->attribute("id");
        int32_t seen = 0;
        for (auto const &selected : selection->getState().selected_nodes) {
            if (!id || selected.path_id != id) {
                continue;
            }
            if (seen++ != wanted) {
                continue;
            }
            int32_t const indices[2] = {static_cast<int32_t>(selected.subpath_index),
                                        static_cast<int32_t>(selected.node_index)};
            if (!invocation->spanIsValid(args->data[2].of.i32, sizeof indices)) {
                return invocation->trap("selectedNode: result is outside the module's memory");
            }
            memcpy(invocation->memoryBytes() + args->data[2].of.i32, indices, sizeof indices);
            results->data[0] = WASM_I32_VAL(1);
            return nullptr;
        }
    }
    results->data[0] = WASM_I32_VAL(0);
    return nullptr;
}

/**
 * org.inkscape.Cancellation.isCancelled() -> i32
 *
 * Cooperative cancellation, which is how every in-process plugin API that cannot kill its
 * guests has always done this: Photoshop's TestAbort, After Effects' PF_ABORT. A plugin doing
 * long work calls this in its outer loop and returns early when it answers true.
 *
 * It also services pending UI events, and that is not incidental -- it is what makes the
 * answer able to change. The guest runs on the main thread inside wasm_func_call, so while it
 * is working the main loop is not: without pumping here the user could never press the Cancel
 * button that sets the flag, and this would always answer false. TestAbort does the same for
 * the same reason.
 *
 * Reentrancy is bounded by the working dialog being modal, so what a user can reach while a
 * plugin runs is that dialog. A plugin that never calls this cannot be cancelled, exactly as
 * a C plugin with an infinite loop cannot be; that is a property of running in-process, not
 * of this design, and it is documented rather than papered over.
 */
wasm_trap_t *isCancelled(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    if (invocation->desktop()) {
        while (g_main_context_pending(nullptr)) {
            g_main_context_iteration(nullptr, FALSE);
        }
    }
    results->data[0] = WASM_I32_VAL(invocation->isCancelled() ? 1 : 0);
    return nullptr;
}

/** org.inkscape.Selection.selectionClear() */
wasm_trap_t *selectionClear(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    if (auto *selection = invocationOf(env)->selection()) {
        selection->clear();
    }
    return nullptr;
}

/**
 * org.inkscape.Selection.selectionAdd(element)
 *
 * Selection is mutable because builtins treat it as one: BlurEdge clears it and re-adds
 * items as a working register. The execution environment restores the user's selection
 * afterwards, so this is parity with a builtin, not extra power.
 */
wasm_trap_t *selectionAdd(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        // __func__ is the operation's ABI name, so the diagnostic names itself and cannot
        // drift out of step with the function it is in.
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    auto *item = invocation->itemFor(item_node);
    if (auto *selection = invocation->selection()) {
        if (item) {
            selection->add(item);
        }
    }
    return nullptr;
}

/** org.inkscape.Selection.selectionRemove(element) */
wasm_trap_t *selectionRemove(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    auto *invocation = invocationOf(env);
    auto *item_node = invocation->getNode(args->data[0].of.i32);
    if (!item_node) {
        return invocation->trap((std::string(__func__) + ": not a Node handle").c_str());
    }
    if (auto *selection = invocation->selection()) {
        selection->remove(item_node);
    }
    return nullptr;
}

/**
 * org.inkscape.Selection.selectionSet(nodes)
 *
 * Replace the selection with the contents of a NodeList in one step. Clearing and adding in a
 * loop is not the same thing: each add emits a selection-changed signal, so a plugin
 * assembling a selection of any size makes the editor rebuild its state once per item.
 */
wasm_trap_t *selectionSet(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    auto *invocation = invocationOf(env);
    int32_t const handle = args->data[0].of.i32;

    // Either kind of node collection is accepted: childNodes gives a live one, a hit test
    // gives a snapshot, and a plugin selecting "everything in this group" or "everything the
    // marquee touched" should not care which it happens to be holding.
    std::vector<Inkscape::XML::Node *> nodes;
    if (auto const *snapshot = invocation->getNodeSnapshot(handle)) {
        nodes = *snapshot;
    } else if (auto *parent = invocation->getNodeList(handle)) {
        for (auto *child = parent->firstChild(); child != nullptr; child = child->next()) {
            nodes.push_back(child);
        }
    } else {
        return invocation->trap("selectionSet: not a NodeList handle");
    }

    if (auto *selection = invocation->selection()) {
        selection->setReprList(nodes);
    }
    return nullptr;
}

/**
 * org.inkscape.Selection.selectionBounds(type, out_offset) -> i32 present
 *
 * The selection's bounds as a whole, which is not the union of getBBox() results: those are
 * each in their own element's user space, and unioning them is only correct when nothing in
 * the selection is transformed. `Grid::effect()` asks for exactly this.
 *
 * type 0 = visual (stroke, markers, filters), 1 = geometric, 2 = the user's preference.
 */
wasm_trap_t *selectionBounds(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *selection = invocation->selection();
    if (!selection) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    Geom::OptRect box;
    switch (args->data[0].of.i32) {
        case 1:
            box = selection->geometricBounds();
            break;
        case 2:
            box = selection->preferredBounds();
            break;
        default:
            box = selection->visualBounds();
            break;
    }
    return answerRect(invocation, results, args->data[1].of.i32, box, "selectionBounds");
}

/**
 * org.inkscape.Selection.toCurves()
 *
 * Object to path, over the selection. `BlurEdge` calls it; so does anything that needs a
 * <rect> or a <star> to have real path data before it can work on the geometry, which is the
 * single most common opening move in the shipped extension corpus.
 */
wasm_trap_t *toCurves(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    if (auto *selection = invocationOf(env)->selection()) {
        // skip_undo: the execution environment owns the undo step for the whole invocation,
        // and a nested one here would commit half the plugin's work as its own entry.
        selection->toCurves(true);
    }
    return nullptr;
}

// ── Pages ───────────────────────────────────────────────────────────────────────────────
//
// Multi-page documents are document state rather than view state, so all of this works from
// the command line. A page is not an ancestor of anything: which items it holds is a
// geometric question about where they are drawn, which is why membership is a query rather
// than a tree walk.

/** Unpack the PageManager, which lives on the document and needs no desktop. */
#define INK_PAGES_ARG(name, operation)                                \
    auto *invocation = invocationOf(env);                             \
    if (!invocation->getDocument(args->data[0].of.i32)) {             \
        return invocation->trap(operation ": not a Document handle"); \
    }                                                                 \
    auto *document = invocation->document();                          \
    if (!document) {                                                  \
        return invocation->trap(operation ": there is no document");  \
    }                                                                 \
    auto &name = document->getPageManager();

/** Unpack the page a per-page operation acts on. */
#define INK_PAGE_ARG(name, operation)                              \
    auto *invocation = invocationOf(env);                          \
    auto *name##_node = invocation->getNode(args->data[0].of.i32); \
    if (!name##_node) {                                            \
        return invocation->trap(operation ": not a Node handle");  \
    }                                                              \
    auto *name = cast<SPPage>(invocation->objectFor(name##_node));

/** org.inkscape.Pages.pageCount(document) -> i32 */
wasm_trap_t *pageCount(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGES_ARG(pages, "pageCount")
    results->data[0] = WASM_I32_VAL(pages.getPageCount());
    return nullptr;
}

/** org.inkscape.Pages.pages(document) -> NodeList */
wasm_trap_t *pages(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGES_ARG(pages, "pages")
    std::vector<SPObject *> objects(pages.getPages().begin(), pages.getPages().end());
    results->data[0] = WASM_I32_VAL(nodesOfObjects(invocation, objects));
    return nullptr;
}

/**
 * org.inkscape.Pages.pageIndex(page) -> i32
 *
 * Where the page falls in print order, or -1 for anything that is not a page. A plugin
 * numbering or reordering pages needs this rather than a position in a list it holds, since
 * that list may be one it filtered.
 */
wasm_trap_t *pageIndex(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGE_ARG(page, "pageIndex")
    results->data[0] = WASM_I32_VAL(page ? page->getPageIndex() : -1);
    return nullptr;
}

/** org.inkscape.Pages.selectedPage(document) -> Element? */
wasm_trap_t *selectedPage(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGES_ARG(pages, "selectedPage")
    auto *page = pages.getSelected();
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, page ? page->getRepr() : nullptr));
    return nullptr;
}

/** org.inkscape.Pages.newPage(document, x, y, width, height) -> Element? */
wasm_trap_t *newPage(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGES_ARG(pages, "newPage")
    auto const rect =
        Geom::Rect::from_xywh(args->data[1].of.f64, args->data[2].of.f64, args->data[3].of.f64, args->data[4].of.f64);
    auto *page = pages.newPage(rect);
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, page ? page->getRepr() : nullptr));
    return nullptr;
}

/**
 * org.inkscape.Pages.deletePage(page, contents)
 *
 * `contents` also removes what the page holds, which is the difference between deleting a page
 * and deleting a page of work.
 */
wasm_trap_t *deletePage(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_PAGE_ARG(page, "deletePage")
    if (page && page->document) {
        bool const contents = args->data[1].of.i32 != 0;
        if (contents) {
            // Which items go is decided by bounding box, so this must not run against a stale
            // one: getting it wrong here deletes the wrong drawing.
            page->document->ensureUpToDate();
        }
        page->document->getPageManager().deletePage(page, contents);
    }
    return nullptr;
}

/**
 * org.inkscape.Pages.pageRect(page, space, out_offset) -> i32 present
 *
 * space 0 = the page's own coordinates, 1 = document coordinates. They differ as soon as the
 * document has a scale or a viewBox, which is the case a plugin laying out across pages hits.
 */
wasm_trap_t *pageRect(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGE_ARG(page, "pageRect")
    if (!page) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    auto const rect = args->data[1].of.i32 == 1 ? page->getDocumentRect() : page->getRect();
    return answerRect(invocation, results, args->data[2].of.i32, Geom::OptRect(rect), "pageRect");
}

/**
 * org.inkscape.Pages.pageMargin(page, which, out_offset) -> i32 present
 *
 * which 0 = the margin area, 1 = the bleed area, both as rectangles in document coordinates.
 * A rectangle rather than four side values because that is what a plugin placing content
 * needs: the usable area, not the arithmetic to reach it. The raw per-side numbers remain
 * readable as attributes for anything that wants them.
 */
wasm_trap_t *pageMargin(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGE_ARG(page, "pageMargin")
    if (!page) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    auto const rect = args->data[1].of.i32 == 1 ? page->getDocumentBleed() : page->getDocumentMargin();
    return answerRect(invocation, results, args->data[2].of.i32, Geom::OptRect(rect), "pageMargin");
}

/** org.inkscape.Pages.resizePage(page, width, height) */
wasm_trap_t *resizePage(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_PAGE_ARG(page, "resizePage")
    if (page) {
        page->setSize(args->data[1].of.f64, args->data[2].of.f64);
    }
    return nullptr;
}

/** org.inkscape.Pages.selectPage(page) -> i32 */
wasm_trap_t *selectPage(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGE_ARG(page, "selectPage")
    bool selected = false;
    if (page && page->document) {
        selected = page->document->getPageManager().selectPage(page);
    }
    results->data[0] = WASM_I32_VAL(selected ? 1 : 0);
    return nullptr;
}

/** Bits of the pageItems() flags argument. */
enum PageItemsFlags
{
    page_items_contained = 1 << 0, //< wholly inside the page rather than merely touching it
    page_items_bleed = 1 << 1,     //< measure against the bleed box instead of the page
    page_items_hidden = 1 << 2,    //< count items that are not displayed
};

/**
 * org.inkscape.Pages.pageItems(page, flags) -> NodeList
 *
 * What a page holds is a geometric question -- pages are not ancestors of anything -- so the
 * answer depends on which box is meant and on how much of an item has to be inside it. Both
 * are choices Inkscape already makes separately (`getExclusiveItems` tests containment,
 * `getOverlappingItems` tests intersection, and either can measure against the bleed), so both
 * are the caller's to make. Bits rather than three arguments, following getBBox() above.
 *
 * Groups answer as whole objects: this does not descend into them, so a group half on the page
 * is one straddling item and not a list of its children.
 */
wasm_trap_t *pageItems(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_PAGE_ARG(page, "pageItems")
    std::vector<SPObject *> objects;
    if (page && page->document) {
        // Both queries test bounding boxes, which are stale until the document is brought up to
        // date -- a plugin that draws something and then asks what is on the page must be told
        // about what it just drew.
        page->document->ensureUpToDate();

        auto const flags = args->data[1].of.i32;
        bool const hidden = (flags & page_items_hidden) != 0;
        bool const bleed = (flags & page_items_bleed) != 0;
        auto const items = (flags & page_items_contained) ? page->getExclusiveItems(hidden, bleed)
                                                          : page->getOverlappingItems(hidden, bleed);
        objects.assign(items.begin(), items.end());
    }
    results->data[0] = WASM_I32_VAL(nodesOfObjects(invocation, objects));
    return nullptr;
}

/** org.inkscape.Pages.fitPageToSelection(document, addMargins) */
wasm_trap_t *fitPageToSelection(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_PAGES_ARG(pages, "fitPageToSelection")
    if (auto *selection = invocation->selection()) {
        // Fitting is to the selection's bounds, so they have to be current.
        document->ensureUpToDate();
        pages.fitToSelection(selection, args->data[1].of.i32 != 0);
    }
    return nullptr;
}

// ── Arranging the selection ─────────────────────────────────────────────────────────────────
//
// ObjectSet operations, so they act on the selection rather than on an element handed in. That
// is the shape the C++ side has and the shape the builtins use, and it works headless because
// the document owns a Selection whether or not a window is showing it.
//
// Every one passes skip_undo: the execution environment owns the undo step for the whole
// invocation, and a nested one would commit half the plugin's work under its own name.

/** The selection, or nothing to do. */
#define INK_SELECTION_OR_RETURN(name)            \
    auto *name = invocationOf(env)->selection(); \
    if (!name) {                                 \
        return nullptr;                          \
    }

/**
 * org.inkscape.Selection.group() -> Element?
 *
 * Answers the group it made, which is otherwise unfindable: it is created with a generated id
 * and the selection afterwards is the group's contents, not the group.
 */
wasm_trap_t *group(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *selection = invocation->selection();
    auto *node = selection ? selection->group() : nullptr;
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, node));
    return nullptr;
}

/** org.inkscape.Selection.ungroup() */
wasm_trap_t *ungroup(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->ungroup(true);
    return nullptr;
}

/**
 * org.inkscape.Selection.ungroupAll()
 *
 * All the way down, rather than one level: a group of groups becomes loose objects in one call.
 */
wasm_trap_t *ungroupAll(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->ungroup_all(true);
    return nullptr;
}

/**
 * org.inkscape.Selection.popFromGroup()
 *
 * Takes the selection out of its group and leaves the group standing, which is the operation
 * ungrouping is not.
 */
wasm_trap_t *popFromGroup(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->popFromGroup();
    return nullptr;
}

/**
 * Z-order. Six operations rather than one taking a direction code, because Inkscape has six and
 * they are not two axes of one idea: raise and lower step over the next OVERLAPPING object,
 * while stackUp and stackDown step exactly one position whatever is or is not underneath. A
 * plugin arranging a diagram wants the first; one rebuilding a known order wants the second.
 */
wasm_trap_t *raise(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->raise(true);
    return nullptr;
}

wasm_trap_t *raiseToTop(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->raiseToTop(true);
    return nullptr;
}

wasm_trap_t *lower(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->lower(true);
    return nullptr;
}

wasm_trap_t *lowerToBottom(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->lowerToBottom(true);
    return nullptr;
}

wasm_trap_t *stackUp(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->stackUp(true);
    return nullptr;
}

wasm_trap_t *stackDown(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->stackDown(true);
    return nullptr;
}

/** org.inkscape.Selection.toLayer(layer) */
wasm_trap_t *toLayer(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_OBJECT_ARG(layer, "toLayer")
    if (auto *selection = invocation->selection()) {
        if (layer) {
            selection->toLayer(layer);
        }
    }
    return nullptr;
}

/**
 * org.inkscape.Selection.duplicate(duplicateLayer, includeHidden)
 *
 * The copy becomes the selection, which is how the builtins chain further work onto it.
 */
wasm_trap_t *duplicate(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->duplicate(true, args->data[0].of.i32 != 0, args->data[1].of.i32 != 0);
    return nullptr;
}

/** org.inkscape.Selection.clone() */
wasm_trap_t *clone(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->clone(true);
    return nullptr;
}

/**
 * org.inkscape.Selection.unlink(recursive) -> i32 changed
 *
 * `recursive` is the difference between unlinking what is selected and unlinking what is inside
 * what is selected -- a group holding clones is untouched by the first and emptied of them by
 * the second. False means nothing was a clone, which is information rather than a failure.
 */
wasm_trap_t *unlink(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *selection = invocationOf(env)->selection();
    bool changed = false;
    if (selection) {
        changed =
            args->data[0].of.i32 != 0 ? selection->unlinkRecursive(true, false, true) : selection->unlink(true, true);
    }
    results->data[0] = WASM_I32_VAL(changed ? 1 : 0);
    return nullptr;
}

/**
 * org.inkscape.Selection.cloneOriginal()
 *
 * Selects what the selected clone points at, which is how a plugin follows a <use> back to its
 * source without parsing the href itself.
 */
wasm_trap_t *cloneOriginal(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->cloneOriginal();
    return nullptr;
}

/**
 * org.inkscape.Selection.fitCanvas() -> i32 changed
 *
 * No margin argument, deliberately. ObjectSet::fitCanvas takes a with_margins flag and passes
 * it to SPDocument::fitToRect, whose definition leaves that parameter unnamed and never reads
 * it (document.cpp SPDocument::fitToRect()) -- so publishing the flag would publish a control that does nothing.
 * PageManager's separate fitToRect does honour margins and is reachable through
 * fitPageToSelection.
 */
wasm_trap_t *fitCanvas(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *selection = invocationOf(env)->selection();
    results->data[0] = WASM_I32_VAL(selection && selection->fitCanvas(false, true) ? 1 : 0);
    return nullptr;
}

/**
 * org.inkscape.Selection.selectionBoolop(op)
 *
 * One operation taking an op code rather than six named ones, because these six differ by
 * exactly the code that is already a parameter elsewhere in this interface: boolop takes the
 * same livarot numbering over two elements' geometry. Two spellings of one idea would be two
 * vocabularies to learn. The z-order six are separate names for the opposite reason -- they
 * differ in behaviour, not in an argument.
 *
 * op is livarot's BooleanOp: 0 union, 1 intersection, 2 difference, 3 symmetric difference,
 * 4 cut, 5 slice.
 */
wasm_trap_t *selectionBoolop(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    auto *invocation = invocationOf(env);
    auto *selection = invocation->selection();
    if (!selection) {
        return nullptr;
    }
    switch (args->data[0].of.i32) {
        case bool_op_union:
            selection->pathUnion(true, true);
            break;
        case bool_op_inters:
            selection->pathIntersect(true, true);
            break;
        case bool_op_diff:
            selection->pathDiff(true, true);
            break;
        case bool_op_symdiff:
            selection->pathSymDiff(true, true);
            break;
        case bool_op_cut:
            selection->pathCut(true, true);
            break;
        case bool_op_slice:
            selection->pathSlice(true, true);
            break;
        default:
            return invocation->trap("selectionBoolop: op is not a BooleanOp");
    }
    return nullptr;
}

/**
 * org.inkscape.Selection.strokesToPaths(legacy) -> i32 changed
 *
 * `legacy` is the pre-1.0 conversion, which refuses groups and text outright
 * (path-outline.cpp item_to_paths()) rather than recursing into them. Exposed because documents
 * produced by the two differ and a plugin reproducing older output needs the older shape.
 */
wasm_trap_t *strokesToPaths(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *selection = invocationOf(env)->selection();
    results->data[0] = WASM_I32_VAL(selection && selection->strokesToPaths(args->data[0].of.i32 != 0, true) ? 1 : 0);
    return nullptr;
}

/**
 * org.inkscape.Selection.simplifyPaths() -> i32 changed
 *
 * No threshold argument: Inkscape takes it from /options/simplifythreshold, and a plugin
 * overriding the user's setting for them is a different decision from exposing the operation.
 */
wasm_trap_t *simplifyPaths(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *selection = invocationOf(env)->selection();
    results->data[0] = WASM_I32_VAL(selection && selection->simplifyPaths(true) ? 1 : 0);
    return nullptr;
}

/**
 * Transforms over the selection as a whole, which is not the same as transforming each member:
 * a scale about the selection's bounding box moves the members relative to one another.
 *
 * Angles are degrees, matching ObjectSet::rotateRelative and the transform= syntax, rather than
 * the radians the underlying 2geom call wants.
 */
wasm_trap_t *selectionMove(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->moveRelative(args->data[0].of.f64, args->data[1].of.f64);
    return nullptr;
}

wasm_trap_t *selectionRotate(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->rotateRelative(Geom::Point(args->data[0].of.f64, args->data[1].of.f64), args->data[2].of.f64);
    return nullptr;
}

wasm_trap_t *selectionScale(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->scaleRelative(Geom::Point(args->data[0].of.f64, args->data[1].of.f64),
                             Geom::Scale(args->data[2].of.f64, args->data[3].of.f64));
    return nullptr;
}

wasm_trap_t *selectionSkew(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->skewRelative(Geom::Point(args->data[0].of.f64, args->data[1].of.f64), args->data[2].of.f64,
                            args->data[3].of.f64);
    return nullptr;
}

/**
 * org.inkscape.Selection.selectionApplyAffine(a, b, c, d, e, f, compensate)
 *
 * `compensate` is what keeps a clone in step with its original; turning it off transforms the
 * geometry and leaves what hangs off it alone.
 *
 * ObjectSet::applyAffine's set_i2d is not exposed and is always true. False is an internal hook
 * for seltrans dragging live: the item has already been moved by other means and the call only
 * syncs the repr from item->transform, DISCARDING the affine passed
 * (selection-chemistry.cpp ObjectSet::applyAffine()). A plugin has no way to have pre-moved
 * the item, so the flag could only silently throw its matrix away.
 */
wasm_trap_t *selectionApplyAffine(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    Geom::Affine const affine(args->data[0].of.f64, args->data[1].of.f64, args->data[2].of.f64, args->data[3].of.f64,
                              args->data[4].of.f64, args->data[5].of.f64);
    selection->applyAffine(affine, true, args->data[6].of.i32 != 0);
    return nullptr;
}

/** org.inkscape.Selection.removeTransform() */
wasm_trap_t *removeTransform(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->removeTransform();
    return nullptr;
}

/**
 * Clips and masks. One Inkscape call takes a flag choosing between them, and this keeps that
 * shape rather than splitting into two names, because the rest of the arguments and the whole
 * of the behaviour are shared -- the flag picks which element the topmost object becomes.
 *
 * org.inkscape.Selection.setMask(clip, applyToLayer, removeOriginal) -> i32 applied
 *
 * The topmost selected object becomes the clip or mask; everything under it is what gets
 * clipped or masked. `removeOriginal` deletes the objects that were consumed instead of
 * leaving them selected.
 *
 * `applyToLayer` needs a desktop -- ObjectSet::setMask returns immediately without one
 * (selection-chemistry.cpp ObjectSet::setMask()) -- so asking for it headless answers false rather than doing
 * nothing quietly.
 */
wasm_trap_t *setMask(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *selection = invocation->selection();
    bool const to_layer = args->data[1].of.i32 != 0;
    if (!selection || (to_layer && !invocation->desktop())) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    selection->setMask(args->data[0].of.i32 != 0, to_layer, args->data[2].of.i32 != 0);
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/** org.inkscape.Selection.unsetMask(clip, deleteHelperGroup, removeOriginal) */
wasm_trap_t *unsetMask(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->unsetMask(args->data[0].of.i32 != 0, args->data[1].of.i32 != 0, args->data[2].of.i32 != 0);
    return nullptr;
}

/**
 * org.inkscape.Selection.setClipGroup()
 *
 * Wraps the selection in a group carrying the clip, rather than clipping each member with its
 * own copy -- a different result from setMask, not a convenience over it.
 */
wasm_trap_t *setClipGroup(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->setClipGroup();
    return nullptr;
}

/**
 * Turning the selection into something reusable: a marker, a pattern, or a symbol.
 *
 * `apply` puts the new definition to use on what it was made from; without it the definition is
 * created and the document left referring to nothing, which is what a plugin building a library
 * of parts wants.
 */
wasm_trap_t *toMarker(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->toMarker(args->data[0].of.i32 != 0);
    return nullptr;
}

/** org.inkscape.Selection.toPattern(apply) -- Inkscape calls this "Object to Pattern". */
wasm_trap_t *toPattern(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->tile(args->data[0].of.i32 != 0);
    return nullptr;
}

/**
 * org.inkscape.Selection.bitmapCopy()
 *
 * Renders the selection and puts the result back into the document as an <image>. The renderer
 * is what makes this the one selection operation that belongs with the file backends rather
 * than with the geometry ones -- and it runs headless, reaching for a desktop only to flash
 * "Rendering bitmap..." and set a cursor.
 */
wasm_trap_t *bitmapCopy(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->createBitmapCopy();
    return nullptr;
}

/** org.inkscape.Selection.untile() */
wasm_trap_t *untile(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->untile();
    return nullptr;
}

/** org.inkscape.Selection.toSymbol() */
wasm_trap_t *toSymbol(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    INK_SELECTION_OR_RETURN(selection)
    selection->toSymbol();
    return nullptr;
}

/**
 * org.inkscape.Selection.unSymbol() -- registered, and refuses.
 *
 * SPSymbol::unSymbol() crashes with no desktop: it takes the SP_ACTIVE_DESKTOP branch at
 * sp-symbol.cpp SPSymbol::unSymbol(), puts the replacement group in <defs>, gives it the symbol's id while
 * the symbol still holds it, then deleteObject()s the symbol. Once that is fixed upstream this
 * becomes selection->unSymbol().
 */
wasm_trap_t *unSymbol(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    return invocationOf(env)->trap("unSymbol: blocked upstream -- SPSymbol::unSymbol() crashes without a "
                                   "desktop (sp-symbol.cpp SPSymbol::unSymbol(), its own \"TODO: Better handle if no "
                                   "desktop\"). Not called, because it would take the host down with it.");
}

/**
 * org.inkscape.Selection.convertUnit(value, from_off, from_len, to_off, to_len) -> f64
 *
 * 27 of the 166 shipped Python extensions call unittouu or viewport_to_unit, and it is almost
 * always the first line of work: a parameter arrives as a number and a unit name and has to
 * become user units before anything can be done with it. addnodes.py opens with exactly that.
 *
 * Inkscape's own table, through Inkscape::Util::Quantity::convert (units.h), so a plugin taking
 * a millimetre parameter does not carry 25.4 written out somewhere.
 *
 * A unit the table does not know answers NaN, and deliberately not Inkscape's own -1. An
 * unknown name reaches Unit::convert as the empty unit, whose type matches nothing, and that
 * function reports incompatibility by returning -1 (units.cpp) -- which is indistinguishable
 * from correctly converting -1px to px, and is a perfectly plausible coordinate to go on and
 * draw with. NaN cannot be mistaken for a measurement, survives arithmetic, and costs the guest
 * one comparison to test.
 */
wasm_trap_t *convertUnit(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(from, 1, "convertUnit")
    INK_STRING_ARG(to, 3, "convertUnit")

    auto const &table = Inkscape::Util::UnitTable::get();
    if (!table.hasUnit(from) || !table.hasUnit(to)) {
        results->data[0] = WASM_F64_VAL(std::numeric_limits<double>::quiet_NaN());
        return nullptr;
    }
    results->data[0] = WASM_F64_VAL(Inkscape::Util::Quantity::convert(args->data[0].of.f64, from.c_str(), to.c_str()));
    return nullptr;
}

// ── Building paths ──────────────────────────────────────────────────────────────────────────
//
// Writing a `d` is the commonest thing a shipped extension does that this interface could not
// do: 36 of the 166 Python extensions assign `node.path = ...`, and every one of them formats
// the string itself. A builder means none of them has to.
//
// It is Inkscape's own machinery rather than a second one. The handle holds a Geom::PathVector,
// the verbs go through Geom::PathBuilder -- 2geom's PathSink, with exactly the verbs SVG has --
// and the result is written by sp_svg_write_path. A plugin's paths therefore come out spelled
// the way every other path in the document is spelled.

/** Unpack the builder a path operation acts on. */
#define INK_BUILDER_ARG(name, operation)                                 \
    auto *invocation = invocationOf(env);                                \
    auto *name = invocation->getPathBuilder(args->data[0].of.i32);       \
    if (!name) {                                                         \
        return invocation->trap(operation ": not a PathBuilder handle"); \
    }

/** org.inkscape.PathBuilder.pathBegin() -> PathBuilder */
wasm_trap_t *pathBegin(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    results->data[0] = WASM_I32_VAL(invocationOf(env)->makePathBuilder());
    return nullptr;
}

/** org.inkscape.PathBuilder.pathMoveTo(builder, x, y) -- starts a new subpath. */
wasm_trap_t *pathMoveTo(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_BUILDER_ARG(pv, "pathMoveTo")
    pv->sink.moveTo(Geom::Point(args->data[1].of.f64, args->data[2].of.f64));
    return nullptr;
}

/** org.inkscape.PathBuilder.pathLineTo(builder, x, y) */
wasm_trap_t *pathLineTo(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_BUILDER_ARG(pv, "pathLineTo")
    pv->sink.lineTo(Geom::Point(args->data[1].of.f64, args->data[2].of.f64));
    return nullptr;
}

/** org.inkscape.PathBuilder.pathCurveTo(builder, x1, y1, x2, y2, x, y) -- cubic. */
wasm_trap_t *pathCurveTo(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_BUILDER_ARG(pv, "pathCurveTo")
    pv->sink.curveTo(Geom::Point(args->data[1].of.f64, args->data[2].of.f64),
                     Geom::Point(args->data[3].of.f64, args->data[4].of.f64),
                     Geom::Point(args->data[5].of.f64, args->data[6].of.f64));
    return nullptr;
}

/** org.inkscape.PathBuilder.pathQuadTo(builder, cx, cy, x, y) -- quadratic. */
wasm_trap_t *pathQuadTo(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_BUILDER_ARG(pv, "pathQuadTo")
    pv->sink.quadTo(Geom::Point(args->data[1].of.f64, args->data[2].of.f64),
                    Geom::Point(args->data[3].of.f64, args->data[4].of.f64));
    return nullptr;
}

/**
 * org.inkscape.PathBuilder.pathArcTo(builder, rx, ry, angle, largeArc, sweep, x, y)
 *
 * The elliptical arc SVG's `A` command describes, with the same two flags. Worth having as a
 * verb rather than left to the guest: an arc approximated with cubics is a different curve, and
 * the difference shows up the moment anything measures or offsets it.
 */
wasm_trap_t *pathArcTo(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_BUILDER_ARG(pv, "pathArcTo")
    pv->sink.arcTo(args->data[1].of.f64, args->data[2].of.f64, args->data[3].of.f64, args->data[4].of.i32 != 0,
                   args->data[5].of.i32 != 0, Geom::Point(args->data[6].of.f64, args->data[7].of.f64));
    return nullptr;
}

/** org.inkscape.PathBuilder.pathClose(builder) */
wasm_trap_t *pathClose(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_BUILDER_ARG(pv, "pathClose")
    pv->sink.closePath();
    return nullptr;
}

/**
 * org.inkscape.PathBuilder.pathFeed(builder, element) -> i32 fed
 *
 * Seed the builder from an element's RESOLVED geometry, which is what makes read-modify-write
 * possible. A rect, a star or a shape under a path effect can be fed in, added to and written
 * back out without the guest handling a single coordinate -- and what goes in is Inkscape's
 * idea of that shape rather than a reimplementation of it.
 */
wasm_trap_t *pathFeed(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_BUILDER_ARG(pv, "pathFeed")
    auto *node = invocation->getNode(args->data[1].of.i32);
    if (!node) {
        return invocation->trap("pathFeed: not a Node handle");
    }
    auto *item = invocation->itemFor(node);
    results->data[0] = WASM_I32_VAL(0);
    if (!item) {
        return nullptr;
    }
    if (auto const curve = curve_for_item(item)) {
        // feed() closes off whatever was open and appends complete paths, so a builder that has
        // been fed is between subpaths rather than in one -- the next verb has to be a moveTo,
        // which is what the implicit-moveto branch of the sink would otherwise silently supply
        // from the origin.
        pv->sink.feed(*curve);
        results->data[0] = WASM_I32_VAL(1);
    }
    return nullptr;
}

/**
 * org.inkscape.PathBuilder.pathParse(builder, text_off, text_len) -> i32 parsed
 *
 * Path data arriving as TEXT -- from a parameter, from a file an <input> module is reading,
 * from anywhere that is not already an element. pathFeed covers the element case; without
 * this there was no other way in, though 2geom's parse_svg_path writes into a PathSink, which
 * is exactly what the builder holds.
 *
 * False leaves the builder as it was: a parse that fails part way through would otherwise
 * append however much of the path it managed before giving up.
 */
wasm_trap_t *pathParse(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_BUILDER_ARG(pv, "pathParse")
    INK_STRING_ARG(text, 1, "pathParse")

    Geom::PathVector parsed;
    try {
        parsed = sp_svg_read_pathv(text.c_str());
    } catch (...) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    if (parsed.empty()) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    pv->sink.feed(parsed);
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.PathBuilder.pathHull(builder, element) -> i32 built
 *
 * The convex hull of an element's geometry, fed into the builder as a closed path. 2geom has
 * ConvexHull and no Inkscape wrapper uses it, so it was unreachable -- which is the shape of
 * every gap on this axis: the geometry library can do it and the application has never needed
 * to.
 */
wasm_trap_t *pathHull(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_BUILDER_ARG(pv, "pathHull")
    auto *node = invocation->getNode(args->data[1].of.i32);
    if (!node) {
        return invocation->trap("pathHull: not a Node handle");
    }
    auto *item = invocation->itemFor(node);
    auto const paths = item ? pathOf(item) : Geom::PathVector();
    results->data[0] = WASM_I32_VAL(0);
    if (paths.empty()) {
        return nullptr;
    }

    // The hull of the nodes, which for anything made of straight lines is the hull of the
    // shape. A curve can bulge past its own control points, so this is the hull of the path's
    // vertices rather than of every point the curve visits -- stated because the difference
    // only shows on curves and would otherwise be found the hard way.
    std::vector<Geom::Point> points;
    for (auto const &path : paths) {
        points.push_back(path.initialPoint());
        for (auto const &curve : path) {
            points.push_back(curve.finalPoint());
        }
    }
    Geom::ConvexHull const hull(points);
    if (hull.empty()) {
        return nullptr;
    }
    bool first = true;
    for (auto const &p : hull) {
        if (first) {
            pv->sink.moveTo(p);
            first = false;
        } else {
            pv->sink.lineTo(p);
        }
    }
    pv->sink.closePath();
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/** org.inkscape.PathBuilder.pathApply(builder, element) -> i32 written */
wasm_trap_t *pathApply(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_BUILDER_ARG(pv, "pathApply")
    auto *node = invocation->getNode(args->data[1].of.i32);
    if (!node) {
        return invocation->trap("pathApply: not a Node handle");
    }
    node->setAttribute("d", sp_svg_write_path(pv->finished()));
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.PathBuilder.pathString(builder, out, cap) -> i32 length
 *
 * The `d` the builder holds, for a plugin that wants to put it somewhere other than an element.
 * An empty builder answers an empty string rather than absence: it exists, it just holds
 * nothing yet.
 */
wasm_trap_t *pathString(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_BUILDER_ARG(pv, "pathString")
    auto const text = sp_svg_write_path(pv->finished());
    INK_STRING_RESULT(text.c_str(), 1, "pathString")
    return nullptr;
}

// ── Host services ───────────────────────────────────────────────────────────────────────────
//
// What a plugin asks of the application rather than of the document: telling the user something,
// keeping its own settings, naming the entry it leaves in the undo history, and reaching the
// action map.

/**
 * org.inkscape.Host.message(type, text_off, text_len) -> i32 id
 *
 * `type` is Inkscape's MessageType (message.h): 0 normal, 1 immediate, 2 warning, 3 error,
 * 4 information.
 *
 * A message is something shown to a person, so with no desktop there is nobody to show it to.
 * Rather than pretend, it goes to the log and the answer is 0 -- "no message is on screen" --
 * which is also what a plugin needs to know before trying to cancel one.
 */
wasm_trap_t *message(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(text, 1, "message")
    auto const type = static_cast<Inkscape::MessageType>(args->data[0].of.i32);
    results->data[0] = WASM_I32_VAL(0);
    if (auto *desktop = invocation->desktop()) {
        results->data[0] = WASM_I32_VAL(static_cast<int32_t>(desktop->messageStack()->push(type, text.c_str())));
    } else {
        g_message("%s", text.c_str());
    }
    return nullptr;
}

/**
 * org.inkscape.Host.flashMessage(type, text_off, text_len) -> i32 id
 *
 * As message, but the message expires on its own. The distinction is the user's attention:
 * a flash is for something they need not act on.
 */
wasm_trap_t *flashMessage(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(text, 1, "flashMessage")
    auto const type = static_cast<Inkscape::MessageType>(args->data[0].of.i32);
    results->data[0] = WASM_I32_VAL(0);
    if (auto *desktop = invocation->desktop()) {
        results->data[0] = WASM_I32_VAL(static_cast<int32_t>(desktop->messageStack()->flash(type, text.c_str())));
    } else {
        g_message("%s", text.c_str());
    }
    return nullptr;
}

/**
 * org.inkscape.Host.cancelMessage(id)
 *
 * Harmless for an id that was never issued, which headless is the only kind there is.
 */
wasm_trap_t *cancelMessage(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    auto *invocation = invocationOf(env);
    if (auto *desktop = invocation->desktop()) {
        if (auto const id = args->data[0].of.i32) {
            desktop->messageStack()->cancel(static_cast<Inkscape::MessageId>(id));
        }
    }
    return nullptr;
}

/**
 * Preferences, scoped to the extension's own id.
 *
 * The scoping is the sandbox boundary, not a convenience. Inkscape's preference tree is one
 * namespace holding every plugin's state, the application's settings, and file paths; letting a
 * plugin address it freely would let one read and rewrite another's, and Inkscape's own. So a
 * key is a leaf name under this extension's branch, and a key that tries to climb out is
 * refused rather than sanitised -- sanitising invites a plugin to keep guessing at a spelling
 * that gets through.
 */
std::string prefPathFor(WasmInvocation *invocation, std::string const &key)
{
    if (key.empty() || key.find('/') != std::string::npos || key.find("..") != std::string::npos) {
        return {};
    }
    auto const *effect = invocation->extension();
    auto const *id = effect ? effect->get_id() : nullptr;
    if (!id || !*id) {
        return {};
    }
    return std::string("/extensions/wasm/") + id + "/" + key;
}

/**
 * org.inkscape.Host.getPref(key_off, key_len, out_off, out_cap) -> i32 length
 *
 * Absent (-1) for a key never written, distinctly from an empty string that was.
 */
wasm_trap_t *getPref(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(key, 0, "getPref")
    auto const path = prefPathFor(invocation, key);
    if (path.empty()) {
        results->data[0] = WASM_I32_VAL(-1);
        return nullptr;
    }
    auto *prefs = Inkscape::Preferences::get();
    // The sentinel distinguishes "never written" from "written empty", which getString cannot
    // do on its own -- both would come back as "".
    static char const *const unset = "\x01wasm-unset";
    auto const value = prefs->getString(path, unset);
    if (value.raw() == unset) {
        results->data[0] = WASM_I32_VAL(-1);
        return nullptr;
    }
    int32_t answer = 0;
    if (!invocation->writeString(args->data[2].of.i32, args->data[3].of.i32, value.c_str(), answer)) {
        return invocation->trap("getPref: buffer is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(answer);
    return nullptr;
}

/**
 * org.inkscape.Host.setPref(key_off, key_len, value_off, value_len) -> i32 written
 *
 * False for a key outside the extension's scope. Reported rather than trapped: asking for a key
 * a plugin may not have is wrong, but not so wrong that the host should end the invocation over
 * it, and a plugin that checks gets a usable answer.
 */
wasm_trap_t *setPref(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(key, 0, "setPref")
    INK_STRING_ARG(value, 2, "setPref")
    auto const path = prefPathFor(invocation, key);
    if (path.empty()) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }
    Inkscape::Preferences::get()->setString(path, value);
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * The undo entry this invocation will leave behind.
 *
 * ExecutionEnv::commit() names it after the extension, which is right for an extension that does
 * one thing and wrong for a plugin that does several -- the user is shown the plugin's name
 * where they expect the operation's. Setting a label overrides that; setting a coalesce key
 * merges the entry with the previous one carrying the same key, which is what keeps a
 * live-preview drag to one history entry instead of one per redraw.
 *
 * Both are read back by ExecutionEnv after the guest returns, through Implementation::undoLabel
 * and undoCoalesceKey.
 */
wasm_trap_t *undoLabel(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto const &label = invocation->undoLabel();
    INK_STRING_RESULT(label.empty() ? nullptr : label.c_str(), 0, "undoLabel")
    return nullptr;
}

wasm_trap_t *setUndoLabel(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(label, 0, "setUndoLabel")
    invocation->setUndoLabel(label);
    return nullptr;
}

wasm_trap_t *undoCoalesceKey(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto const &key = invocation->undoCoalesceKey();
    INK_STRING_RESULT(key.empty() ? nullptr : key.c_str(), 0, "undoCoalesceKey")
    return nullptr;
}

wasm_trap_t *setUndoCoalesceKey(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(key, 0, "setUndoCoalesceKey")
    invocation->setUndoCoalesceKey(key);
    return nullptr;
}

/**
 * org.inkscape.Host.invokeAction(spec_off, spec_len) -> i32 invoked
 *
 * Spelled in Inkscape's own --actions syntax, `name:value`; reusing parse_actions keeps the
 * parameter coercion in step with the command line's.
 *
 * False when no action of that name exists, which is what listActions is for: the CALL is
 * part of this interface's version, but WHICH actions exist is not.
 */
wasm_trap_t *invokeAction(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(spec, 0, "invokeAction")
    results->data[0] = WASM_I32_VAL(0);
    auto *app = InkscapeApplication::instance();
    auto *document = invocation->document();
    if (!app || !document) {
        return nullptr;
    }
    action_vector_t actions;
    // parse_actions reports an unknown name on stderr and adds nothing, so an empty vector is
    // how "no such action" arrives here.
    app->parse_actions(spec, actions);
    if (actions.empty()) {
        return nullptr;
    }
    // Inkscape's own dispatcher, so the search order across the application, window and
    // document action groups is the one the command line uses rather than a second guess at it.
    // The window is whatever is active, which headless is nothing.
    activate_any_actions(actions, Gio::Application::get_default(), app->get_active_window(), document);
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.FileBackend.writeOutput(offset, length) -> i32 accepted
 *
 * The bytes an <input> or <output> module produces. The guest never touches the filesystem --
 * the host holds the path and does the writing -- so this is how a backend hands over its
 * result, and the sandbox boundary stays where it is everywhere else in this interface.
 *
 * Appends, so a module can emit in as many pieces as suits it. False once the total would pass
 * the cap, which is the one thing a guest loop could otherwise use to exhaust host memory from
 * inside the sandbox. Ignored by an <effect>, which produces a document rather than a file.
 */
wasm_trap_t *writeOutput(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto const offset = args->data[0].of.i32;
    auto const length = args->data[1].of.i32;
    if (length < 0 || !invocation->spanIsValid(offset, length)) {
        return invocation->trap("writeOutput: span is outside the module's memory");
    }
    results->data[0] =
        WASM_I32_VAL(invocation->appendOutput(invocation->memoryBytes() + offset, static_cast<size_t>(length)) ? 1 : 0);
    return nullptr;
}

/**
 * org.inkscape.FileBackend.inputBytes(out_offset, out_capacity) -> i32 length
 *
 * The bytes of the file an <input> module is opening, through the same length-first protocol
 * every string result uses: the answer is the length needed, written only when it fits, so a
 * module asks once with a small buffer and again with the right one.
 *
 * Absent (-1) for anything that is not an open() -- an effect has a document, not a file.
 */
wasm_trap_t *inputBytes(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto const *input = invocation->input();
    if (!input) {
        results->data[0] = WASM_I32_VAL(-1);
        return nullptr;
    }
    int32_t written = 0;
    if (!invocation->writeBytes(args->data[0].of.i32, args->data[1].of.i32, input->data(), input->size(), written)) {
        return invocation->trap("inputBytes: buffer is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(written);
    return nullptr;
}

/** org.inkscape.Host.listActions() -> DOMStringList */
wasm_trap_t *listActions(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    std::vector<std::string> names;
    if (auto const gio_app = Gio::Application::get_default()) {
        for (auto const &name : gio_app->list_actions()) {
            names.push_back(name.raw());
        }
    }
    if (auto *document = invocation->document()) {
        if (auto const group = document->getActionGroup()) {
            for (auto const &name : group->list_actions()) {
                names.push_back(name.raw());
            }
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    results->data[0] = WASM_I32_VAL(invocation->makeStringList(std::move(names)));
    return nullptr;
}

// ── Layers ──────────────────────────────────────────────────────────────────────────────────
//
// A layer is a <g> carrying inkscape:groupmode="layer" and nothing more, so layers are document
// state and all of this works headless. LayerManager is built from an SPDesktop and so is not
// available then, but what it does divides cleanly: the hide and lock toggles write `display`
// and `sodipodi:insensitive` onto the objects, which is the document; only the CURRENT layer is
// a per-view idea, and Inkscape persists even that to the namedview and restores the desktop
// from it on open. These therefore call the free functions LayerManager is itself written in
// terms of, with the document root standing in for a desktop's current root.

/** Where layers are counted from: the group the user has entered, or else the document root. */
SPObject *layerRootOf(WasmInvocation *invocation, SPDocument *document)
{
    if (auto *desktop = invocation->desktop()) {
        if (auto *root = desktop->layerManager().currentRoot()) {
            return root;
        }
    }
    return document->getRoot();
}

/**
 * The layer in force, resolved as Inkscape resolves it when opening a document
 * (sp-namedview.cpp sp_namedview_update_layers_from_document()): the layer the namedview
 * names, else the topmost layer, else the root.
 *
 * Never absent, and answers without a desktop: the current layer is written to the namedview and
 * read back from it, so the document carries it whether or not anyone is looking.
 */
SPObject *currentLayerOf(WasmInvocation *invocation, SPDocument *document)
{
    if (auto *desktop = invocation->desktop()) {
        if (auto *layer = desktop->layerManager().currentLayer()) {
            return layer;
        }
    }
    if (auto *view = document->getReprNamedView()) {
        if (auto const *id = view->attribute("inkscape:current-layer")) {
            auto *named = document->getObjectById(id);
            if (is<SPGroup>(named)) {
                return named;
            }
        }
    }
    SPObject *topmost = nullptr;
    for (auto &child : document->getRoot()->children) {
        if (LayerManager::asLayer(&child)) {
            topmost = &child;
        }
    }
    return topmost ? topmost : document->getRoot();
}

/** Every layer under the root, in the order LayerManager::getAllLayers() reports them. */
std::vector<SPObject *> allLayersOf(SPObject *root)
{
    std::vector<SPObject *> layers;
    for (SPObject *obj = Inkscape::previous_layer(root, root); obj; obj = Inkscape::previous_layer(root, obj)) {
        layers.push_back(obj);
    }
    return layers;
}

/**
 * org.inkscape.Layers.isLayer(element) -> i32
 *
 * The document's answer, from SPGroup::isLayer(), rather than LayerManager's: a view can enter a
 * group and treat it as a layer for its own purposes, and that is a fact about the view.
 */
wasm_trap_t *isLayer(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_OBJECT_ARG(object, "isLayer")
    results->data[0] = WASM_I32_VAL(LayerManager::asLayer(object) ? 1 : 0);
    return nullptr;
}

/** org.inkscape.Layers.layers(document) -> NodeList */
wasm_trap_t *layers(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_DOCUMENT_ARG(handle, "layers")
    auto *document = invocation->document();
    if (!document) {
        return invocation->trap("layers: there is no document");
    }
    auto layers = allLayersOf(layerRootOf(invocation, document));
    results->data[0] = WASM_I32_VAL(nodesOfObjects(invocation, layers));
    return nullptr;
}

/**
 * org.inkscape.Layers.createLayer(reference, name_off, name_len, position) -> Element?
 *
 * `position` is Inkscape's own LayerRelativePosition -- 0 above, 1 child, 2 below -- rather than
 * a second numbering invented here to sit on top of it.
 *
 * The name is taken literally. Making it unique is renameLayer's job with its uniquify flag,
 * so that a plugin that wants a collision (two layers deliberately alike) can have one.
 */
wasm_trap_t *createLayer(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_OBJECT_ARG(reference, "createLayer")
    INK_STRING_ARG(name, 1, "createLayer")
    auto const position = args->data[3].of.i32;
    if (position < LPOS_ABOVE || position > LPOS_BELOW) {
        return invocation->trap("createLayer: position is not a LayerRelativePosition");
    }
    auto *document = invocation->document();
    if (!reference || !document) {
        results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, nullptr));
        return nullptr;
    }
    auto *layer = Inkscape::create_layer(layerRootOf(invocation, document), reference,
                                         static_cast<Inkscape::LayerRelativePosition>(position));
    if (layer && !name.empty()) {
        layer->setLabel(name.c_str());
    }
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, layer ? layer->getRepr() : nullptr));
    return nullptr;
}

/**
 * org.inkscape.Layers.layerForObject(element) -> Element?
 *
 * The root answers for anything not inside a layer, which is not the same as answering nothing:
 * a document with no layers still draws everything somewhere. Objects in <defs> are the real
 * absence -- they are not on any layer and are not on the root either.
 */
wasm_trap_t *layerForObject(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_OBJECT_ARG(object, "layerForObject")
    auto *document = invocation->document();
    SPObject *layer = nullptr;
    if (object && document) {
        if (LayerManager::asLayer(object)) {
            layer = object;
        } else {
            auto *root = layerRootOf(invocation, document);
            layer = object->parent;
            while (layer && layer != root && !LayerManager::asLayer(layer)) {
                if (is<SPDefs>(layer)) {
                    layer = nullptr;
                    break;
                }
                layer = layer->parent;
            }
        }
    }
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, layer ? layer->getRepr() : nullptr));
    return nullptr;
}

/**
 * org.inkscape.Layers.renameLayer(layer, name_off, name_len, uniquify)
 *
 * `uniquify` counts up from any trailing number to a name no other layer holds, which is what
 * Inkscape's own Add Layer does. Without it the name is set as given, collisions and all.
 */
wasm_trap_t *renameLayer(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_OBJECT_ARG(layer, "renameLayer")
    INK_STRING_ARG(name, 1, "renameLayer")
    auto *document = invocation->document();
    if (layer && document) {
        if (args->data[3].of.i32 != 0) {
            layer->setLabel(Inkscape::next_layer_name(document, layer, name.c_str()).c_str());
        } else {
            layer->setLabel(name.c_str());
        }
    }
    return nullptr;
}

/** org.inkscape.Layers.currentLayer() -> Element */
wasm_trap_t *currentLayer(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *document = invocation->document();
    auto *layer = document ? currentLayerOf(invocation, document) : nullptr;
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, layer ? layer->getRepr() : nullptr));
    return nullptr;
}

/**
 * org.inkscape.Layers.setCurrentLayer(layer) -> i32
 *
 * Records the choice on the namedview whether or not there is a desktop, because that is where
 * it is kept and where the next reader -- this plugin, the next plugin, or the editor opening
 * the file -- will look for it. With a desktop the view is moved as well, so both agree.
 */
wasm_trap_t *setCurrentLayer(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_OBJECT_ARG(layer, "setCurrentLayer")
    auto *document = invocation->document();
    results->data[0] = WASM_I32_VAL(0);
    if (!layer || !document || !is<SPGroup>(layer)) {
        return nullptr;
    }
    auto const *id = layer->getId();
    auto *view = document->getReprNamedView();
    if (!id || !view) {
        return nullptr;
    }
    view->setAttribute("inkscape:current-layer", id);
    if (auto *desktop = invocation->desktop()) {
        desktop->layerManager().setCurrentLayer(layer);
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/**
 * org.inkscape.Layers.layerSolo(layer, force_hide)
 *
 * Hides every layer beside this one and reveals this one. Without `force_hide` it is a toggle:
 * if the others are already hidden it shows them instead, which is what the layers dialog does.
 */
wasm_trap_t *layerSolo(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_OBJECT_ARG(layer, "layerSolo")
    auto *document = invocation->document();
    if (!layer || !document) {
        return nullptr;
    }
    auto const others = Inkscape::get_layers_to_toggle(layer, layerRootOf(invocation, document));
    if (others.empty()) {
        return nullptr;
    }
    bool const hide_others = args->data[1].of.i32 != 0 ||
                             std::any_of(others.begin(), others.end(), [](SPItem *l) { return !l->isHidden(); });
    if (auto *item = cast<SPItem>(layer)) {
        if (item->isHidden()) {
            item->setHidden(false);
        }
    }
    for (auto *other : others) {
        if (other->isHidden() != hide_others) {
            other->setHidden(hide_others);
        }
    }
    return nullptr;
}

/**
 * org.inkscape.Layers.lockOtherLayers(layer, force_lock)
 *
 * Locking's counterpart to solo, and a toggle in the same way.
 */
wasm_trap_t *lockOtherLayers(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_OBJECT_ARG(layer, "lockOtherLayers")
    auto *document = invocation->document();
    if (!layer || !document) {
        return nullptr;
    }
    auto const others = Inkscape::get_layers_to_toggle(layer, layerRootOf(invocation, document));
    if (others.empty()) {
        return nullptr;
    }
    bool const lock_others = args->data[1].of.i32 != 0 ||
                             std::any_of(others.begin(), others.end(), [](SPItem *l) { return !l->isLocked(); });
    if (auto *item = cast<SPItem>(layer)) {
        if (item->isLocked()) {
            item->setLocked(false);
        }
    }
    for (auto *other : others) {
        if (other->isLocked() != lock_others) {
            other->setLocked(lock_others);
        }
    }
    return nullptr;
}

/** org.inkscape.Layers.hideAllLayers(document, hide) */
wasm_trap_t *hideAllLayers(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_DOCUMENT_ARG(handle, "hideAllLayers")
    auto *document = invocation->document();
    if (!document) {
        return invocation->trap("hideAllLayers: there is no document");
    }
    bool const hide = args->data[1].of.i32 != 0;
    for (auto *layer : allLayersOf(layerRootOf(invocation, document))) {
        if (auto *item = cast<SPItem>(layer)) {
            item->setHidden(hide);
        }
    }
    return nullptr;
}

/** org.inkscape.Layers.lockAllLayers(document, lock) */
wasm_trap_t *lockAllLayers(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
{
    INK_DOCUMENT_ARG(handle, "lockAllLayers")
    auto *document = invocation->document();
    if (!document) {
        return invocation->trap("lockAllLayers: there is no document");
    }
    bool const lock = args->data[1].of.i32 != 0;
    for (auto *layer : allLayersOf(layerRootOf(invocation, document))) {
        if (auto *item = cast<SPItem>(layer)) {
            item->setLocked(lock);
        }
    }
    return nullptr;
}

/**
 * The parameter types the .inx schema declares but the ABI could not read.
 *
 * `color`, `optiongroup`, `path` and `notebook` are all writable in an .inx today, and a
 * plugin had no way to see any of them -- so an extension author could declare a colour
 * picker their own plugin could not read. The typed C++ getters each dynamic_cast to one
 * parameter class, and there is no getter at all for `path` or `notebook`, which is why
 * Extension::get_param_any() had to be added alongside the set_param_any() that was already
 * there.
 */

/** org.inkscape.Params.paramColor(name_off, name_len) -> i32 RGBA */
wasm_trap_t *paramColor(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(name, 0, "paramColor")

    auto const *effect = invocation->extension();
    uint32_t rgba = 0;
    if (effect) {
        try {
            rgba = effect->get_param_color(name.c_str()).toRGBA();
        } catch (...) {
            // A missing parameter is a mistake in the .inx, and a plugin that starts with a
            // transparent black is more use than one that cannot start. Same choice the
            // numeric accessors already make.
            rgba = 0;
        }
    }
    results->data[0] = WASM_I32_VAL(static_cast<int32_t>(rgba));
    return nullptr;
}

/**
 * Read one parameter as the string the .inx would carry, whatever its type.
 *
 * Spelled out rather than built from INK_STRING_ARG/INK_STRING_RESULT because those paste the
 * operation name into a string literal, which needs a literal; this one is shared by three
 * operations and is told its name at run time.
 */
wasm_trap_t *paramAsString(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results, char const *operation)
{
    auto *invocation = invocationOf(env);

    std::string name;
    if (!invocation->readString(args->data[0].of.i32, args->data[1].of.i32, name)) {
        return invocation->trap((std::string(operation) + ": string is outside the module's memory").c_str());
    }

    auto const *effect = invocation->extension();
    std::string value;
    bool found = false;
    if (effect) {
        try {
            value = effect->get_param_any(name.c_str());
            found = true;
        } catch (...) {
            found = false;
        }
    }

    int32_t written = 0;
    if (!invocation->writeString(args->data[2].of.i32, args->data[3].of.i32, found ? value.c_str() : nullptr,
                                 written)) {
        return invocation->trap((std::string(operation) + ": result is outside the module's memory").c_str());
    }
    results->data[0] = WASM_I32_VAL(written);
    return nullptr;
}

/** org.inkscape.Params.paramOptionGroup(name_off, name_len, out, cap) -> i32 */
wasm_trap_t *paramOptionGroup(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    return paramAsString(env, args, results, "paramOptionGroup");
}

/** org.inkscape.Params.paramPath(name_off, name_len, out, cap) -> i32 */
wasm_trap_t *paramPath(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    return paramAsString(env, args, results, "paramPath");
}

/** org.inkscape.Params.paramNotebook(name_off, name_len, out, cap) -> i32 */
wasm_trap_t *paramNotebook(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    return paramAsString(env, args, results, "paramNotebook");
}

/**
 * org.inkscape.Params.setParam(name_off, name_len, value_off, value_len) -> i32 written
 *
 * Parameters are how a plugin persists anything across runs -- the value is written back to
 * the preferences with the extension's own id, which is also the only storage a plugin gets
 * without reaching outside the document. Grid's own preference widgets round-trip through
 * the equivalent C++ setter.
 *
 * A name the .inx never declared answers 0 rather than trapping, matching every parameter
 * GETTER: one contract for the same mistake, so probing for an optional parameter is safe to
 * read and to write.
 */
wasm_trap_t *setParam(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(name, 0, "setParam")
    INK_STRING_ARG(value, 2, "setParam")

    bool written = false;
    if (auto *effect = invocation->extension()) {
        try {
            effect->set_param_any(name.c_str(), value);
            written = true;
        } catch (...) {
            written = false;
        }
    }
    results->data[0] = WASM_I32_VAL(written ? 1 : 0);
    return nullptr;
}

/** The published host surface. Anything not named here is left unresolved, and unresolved
 *  imports make instantiation fail loudly rather than the module running half-wired. */
constexpr HostFunction host_functions[] = {
    // Node -- DOM Living Standard
    {"org.inkscape.Node", "nodeType", nodeType, 1, 1},
    {"org.inkscape.Node", "nodeName", nodeName, 3, 1},
    {"org.inkscape.Node", "localName", localName, 3, 1},
    {"org.inkscape.Node", "namespaceURI", namespaceURI, 3, 1},
    {"org.inkscape.Node", "ownerDocument", ownerDocument, 1, 1},
    {"org.inkscape.Node", "parentNode", parentNode, 1, 1},
    {"org.inkscape.Node", "firstChild", firstChild, 1, 1},
    {"org.inkscape.Node", "lastChild", lastChild, 1, 1},
    {"org.inkscape.Node", "nextSibling", nextSibling, 1, 1},
    {"org.inkscape.Node", "previousSibling", previousSibling, 1, 1},
    {"org.inkscape.Node", "childNodes", childNodes, 1, 1},
    {"org.inkscape.Node", "textContent", getTextContent, 3, 1},
    {"org.inkscape.Node", "setTextContent", setTextContent, 3, 0},
    {"org.inkscape.Node", "insertBefore", insertBefore, 3, 1},
    {"org.inkscape.Node", "appendChild", appendChild, 2, 1},
    {"org.inkscape.Node", "removeChild", removeChild, 2, 1},
    {"org.inkscape.Node", "replaceChild", replaceChild, 3, 1},
    {"org.inkscape.Node", "cloneNode", cloneNode, 2, 1},

    // NodeList / DOMStringList
    {"org.inkscape.NodeList", "length", nodeListLength, 1, 1},
    {"org.inkscape.NodeList", "item", nodeListItem, 2, 1},
    {"org.inkscape.DOMStringList", "length", stringListLength, 1, 1},
    {"org.inkscape.DOMStringList", "item", stringListItem, 4, 1},

    // Element
    {"org.inkscape.Element", "getAttribute", getAttribute, 5, 1},
    {"org.inkscape.Element", "setAttribute", setAttribute, 5, 0},
    {"org.inkscape.Element", "removeAttribute", removeAttribute, 3, 0},
    {"org.inkscape.Element", "hasAttribute", hasAttribute, 3, 1},
    {"org.inkscape.Element", "getAttributeNames", getAttributeNames, 1, 1},

    // Document
    {"org.inkscape.Document", "documentElement", documentElement, 1, 1},
    {"org.inkscape.Document", "createElement", createElement, 3, 1},
    {"org.inkscape.Document", "createElementNS", createElementNS, 5, 1},
    {"org.inkscape.Document", "createTextNode", createTextNode, 3, 1},
    {"org.inkscape.Document", "createComment", createComment, 3, 1},
    {"org.inkscape.Document", "getElementById", getElementById, 3, 1},

    // Lookup and identity
    {"org.inkscape.Document", "getElementsByTagName", getElementsByTagName, 3, 1},
    {"org.inkscape.Document", "getElementsByClassName", getElementsByClassName, 3, 1},
    {"org.inkscape.Document", "querySelector", querySelector, 3, 1},
    {"org.inkscape.Document", "querySelectorAll", querySelectorAll, 3, 1},
    {"org.inkscape.Document", "generateId", generateId, 5, 1},
    {"org.inkscape.Document", "resourceList", resourceList, 3, 1},
    {"org.inkscape.Document", "resolveHref", resolveHref, 3, 1},
    {"org.inkscape.SVGElement", "label", label, 3, 1},
    {"org.inkscape.SVGElement", "defaultLabel", defaultLabel, 3, 1},
    {"org.inkscape.SVGElement", "setLabel", setLabel, 3, 0},
    {"org.inkscape.SVGElement", "title", title, 3, 1},
    {"org.inkscape.SVGElement", "setTitle", setTitle, 3, 0},
    {"org.inkscape.SVGElement", "desc", desc, 3, 1},
    {"org.inkscape.SVGElement", "setDesc", setDesc, 3, 0},
    {"org.inkscape.SVGElement", "linkedObjects", linkedObjects, 2, 1},
    {"org.inkscape.SVGGraphicsElement", "isHidden", isHidden, 1, 1},
    {"org.inkscape.SVGGraphicsElement", "isLocked", isLocked, 1, 1},

    // SVGElement typed attributes -- the parsing behind SVG2's SVGAnimated* layer, without
    // the object model.
    {"org.inkscape.SVGElement", "getLength", getLength, 4, 1},
    {"org.inkscape.SVGElement", "getNumber", getNumber, 4, 1},
    {"org.inkscape.SVGElement", "getTransform", getTransform, 4, 1},
    {"org.inkscape.SVGElement", "setLength", setLength, 5, 0},
    {"org.inkscape.SVGElement", "setNumber", setNumber, 4, 0},
    {"org.inkscape.SVGElement", "setTransform", setTransform, 4, 0},

    // SVGGraphicsElement / SVGGeometryElement -- SVG2
    {"org.inkscape.SVGGraphicsElement", "getBBox", getBBox, 3, 1},
    {"org.inkscape.SVGGraphicsElement", "documentBBox", documentBBox, 3, 1},
    {"org.inkscape.SVGGraphicsElement", "getCTM", getCTM, 2, 1},
    {"org.inkscape.SVGGraphicsElement", "getScreenCTM", getScreenCTM, 2, 1},
    {"org.inkscape.SVGGeometryElement", "getTotalLength", getTotalLength, 1, 1},
    {"org.inkscape.SVGGeometryElement", "getPointAtLength", getPointAtLength, 3, 1},
    {"org.inkscape.SVGGeometryElement", "isPointInFill", isPointInFill, 3, 1},
    {"org.inkscape.SVGGeometryElement", "isPointInStroke", isPointInStroke, 3, 1},

    // Resolved geometry -- corpus rank 1. `d` is not the shape: a <rect> has none, and an LPE
    // item's is its input rather than what the canvas shows.
    {"org.inkscape.SVGGeometryElement", "pathData", pathData, 3, 1},
    {"org.inkscape.SVGGeometryElement", "pathDataBeforeLPE", pathDataBeforeLPE, 3, 1},
    {"org.inkscape.SVGGeometryElement", "outline", outline, 4, 1},
    {"org.inkscape.SVGGeometryElement", "toPath", toPath, 2, 1},
    {"org.inkscape.SVGGeometryElement", "simplify", simplify, 3, 1},
    {"org.inkscape.SVGGeometryElement", "boolop", boolop, 5, 1},
    {"org.inkscape.SVGGeometryElement", "offset", offset, 6, 1},
    {"org.inkscape.SVGGeometryElement", "nearestPoint", nearestPoint, 3, 1},
    {"org.inkscape.SVGGeometryElement", "winding", winding, 2, 1},
    {"org.inkscape.SVGGeometryElement", "intersect", intersect, 4, 1},
    {"org.inkscape.SVGGeometryElement", "markers", markers, 1, 1},

    // Transforms, and what an item is made of
    {"org.inkscape.SVGGraphicsElement", "exactBounds", exactBounds, 3, 1},
    {"org.inkscape.SVGGraphicsElement", "collidesWith", collidesWith, 2, 1},
    {"org.inkscape.SVGGraphicsElement", "transform", transform, 2, 1},
    {"org.inkscape.SVGGraphicsElement", "applyTransform", applyTransform, 3, 0},
    {"org.inkscape.SVGGraphicsElement", "relativeTransform", relativeTransform, 3, 1},
    {"org.inkscape.SVGGraphicsElement", "rotationCenter", rotationCenter, 2, 1},
    {"org.inkscape.SVGGraphicsElement", "setRotationCenter", setRotationCenter, 2, 0},
    {"org.inkscape.SVGGraphicsElement", "unsetRotationCenter", unsetRotationCenter, 1, 0},
    {"org.inkscape.SVGGraphicsElement", "clipPath", clipPath, 1, 1},
    {"org.inkscape.SVGGraphicsElement", "maskObject", maskObject, 1, 1},
    {"org.inkscape.SVGGraphicsElement", "pathEffects", pathEffects, 1, 1},

    // SVGTextContentElement -- SVG2. Backed by Inkscape::Text::Layout.
    {"org.inkscape.SVGTextContentElement", "getNumberOfChars", getNumberOfChars, 1, 1},
    {"org.inkscape.SVGTextContentElement", "getComputedTextLength", getComputedTextLength, 1, 1},
    {"org.inkscape.SVGTextContentElement", "getSubStringLength", getSubStringLength, 3, 1},
    {"org.inkscape.SVGTextContentElement", "getStartPositionOfChar", getStartPositionOfChar, 3, 1},
    {"org.inkscape.SVGTextContentElement", "getEndPositionOfChar", getEndPositionOfChar, 3, 1},
    {"org.inkscape.SVGTextContentElement", "getExtentOfChar", getExtentOfChar, 3, 1},
    {"org.inkscape.SVGTextContentElement", "getRotationOfChar", getRotationOfChar, 2, 1},
    {"org.inkscape.SVGTextContentElement", "getCharNumAtPosition", getCharNumAtPosition, 3, 1},
    {"org.inkscape.SVGTextContentElement", "toPath", textToPath, 3, 1},
    {"org.inkscape.SVGTextContentElement", "textString", textString, 3, 1},
    {"org.inkscape.SVGTextContentElement", "styleAtPosition", styleAtPosition, 4, 1},
    {"org.inkscape.SVGTextContentElement", "baselines", baselines, 3, 1},
    {"org.inkscape.SVGTextContentElement", "lineCount", lineCount, 1, 1},
    {"org.inkscape.SVGTextContentElement", "fontFamily", fontFamily, 4, 1},

    // SVGSVGElement hit testing -- SVG2
    {"org.inkscape.SVGSVGElement", "getEnclosureList", getEnclosureList, 1, 1},
    {"org.inkscape.SVGSVGElement", "getIntersectionList", getIntersectionList, 1, 1},
    {"org.inkscape.SVGSVGElement", "checkEnclosure", checkEnclosure, 2, 1},
    {"org.inkscape.SVGSVGElement", "checkIntersection", checkIntersection, 2, 1},

    // SVGSVGElement document metrics -- what Grid::effect() opens with.
    {"org.inkscape.SVGSVGElement", "viewBox", viewBox, 2, 1},
    {"org.inkscape.SVGSVGElement", "preferredBounds", preferredBounds, 2, 1},
    {"org.inkscape.SVGSVGElement", "pageBounds", pageBounds, 2, 1},
    {"org.inkscape.SVGSVGElement", "documentScale", documentScale, 2, 1},
    {"org.inkscape.SVGSVGElement", "documentSize", documentSize, 2, 1},
    {"org.inkscape.SVGSVGElement", "displayUnit", displayUnit, 3, 1},
    {"org.inkscape.SVGSVGElement", "defs", defs, 1, 1},
    {"org.inkscape.SVGSVGElement", "namedView", namedView, 1, 1},

    // Computed style -- CSSOM
    {"org.inkscape.Document", "getComputedStyle", getComputedStyle, 1, 1},
    {"org.inkscape.CSSStyleDeclaration", "getPropertyValue", getPropertyValue, 5, 1},
    {"org.inkscape.CSSStyleDeclaration", "length", styleLength, 1, 1},
    {"org.inkscape.CSSStyleDeclaration", "item", styleItem, 4, 1},
    {"org.inkscape.CSSStyleDeclaration", "setProperty", setProperty, 5, 0},
    {"org.inkscape.CSSStyleDeclaration", "setPropertyNumber", setPropertyNumber, 4, 0},
    {"org.inkscape.CSSStyleDeclaration", "setPropertyColor", setPropertyColor, 4, 0},
    {"org.inkscape.CSSStyleDeclaration", "removeProperty", removeProperty, 3, 0},
    {"org.inkscape.CSSStyleDeclaration", "specifiedValue", specifiedValue, 5, 1},
    {"org.inkscape.CSSStyleDeclaration", "changeRecursive", changeRecursive, 5, 0},
    {"org.inkscape.CSSStyleDeclaration", "paintServer", paintServer, 2, 1},

    // Session tier -- vendor, `ink` prefixed. Not standard surface.
    {"org.inkscape.Params", "paramString", paramString, 4, 1},
    {"org.inkscape.Params", "paramFloat", paramFloat, 2, 1},
    {"org.inkscape.Params", "paramInt", paramInt, 2, 1},
    {"org.inkscape.Params", "paramBool", paramBool, 2, 1},
    {"org.inkscape.Layers", "currentLayer", currentLayer, 0, 1},
    {"org.inkscape.Layers", "setCurrentLayer", setCurrentLayer, 1, 1},
    {"org.inkscape.Selection", "group", group, 0, 1},
    {"org.inkscape.Selection", "ungroup", ungroup, 0, 0},
    {"org.inkscape.Selection", "ungroupAll", ungroupAll, 0, 0},
    {"org.inkscape.Selection", "popFromGroup", popFromGroup, 0, 0},
    {"org.inkscape.Selection", "raise", raise, 0, 0},
    {"org.inkscape.Selection", "raiseToTop", raiseToTop, 0, 0},
    {"org.inkscape.Selection", "lower", lower, 0, 0},
    {"org.inkscape.Selection", "lowerToBottom", lowerToBottom, 0, 0},
    {"org.inkscape.Selection", "stackUp", stackUp, 0, 0},
    {"org.inkscape.Selection", "stackDown", stackDown, 0, 0},
    {"org.inkscape.Selection", "toLayer", toLayer, 1, 0},
    {"org.inkscape.Selection", "duplicate", duplicate, 2, 0},
    {"org.inkscape.Selection", "clone", clone, 0, 0},
    {"org.inkscape.Selection", "unlink", unlink, 1, 1},
    {"org.inkscape.Selection", "cloneOriginal", cloneOriginal, 0, 0},
    {"org.inkscape.Selection", "fitCanvas", fitCanvas, 0, 1},
    {"org.inkscape.Selection", "selectionBoolop", selectionBoolop, 1, 0},
    {"org.inkscape.Selection", "strokesToPaths", strokesToPaths, 1, 1},
    {"org.inkscape.Selection", "simplifyPaths", simplifyPaths, 0, 1},
    {"org.inkscape.Selection", "selectionMove", selectionMove, 2, 0},
    {"org.inkscape.Selection", "selectionRotate", selectionRotate, 3, 0},
    {"org.inkscape.Selection", "selectionScale", selectionScale, 4, 0},
    {"org.inkscape.Selection", "selectionSkew", selectionSkew, 4, 0},
    {"org.inkscape.Selection", "selectionApplyAffine", selectionApplyAffine, 7, 0},
    {"org.inkscape.Selection", "removeTransform", removeTransform, 0, 0},
    {"org.inkscape.Selection", "setMask", setMask, 3, 1},
    {"org.inkscape.Selection", "unsetMask", unsetMask, 3, 0},
    {"org.inkscape.Selection", "setClipGroup", setClipGroup, 0, 0},
    {"org.inkscape.Selection", "toMarker", toMarker, 1, 0},
    {"org.inkscape.Selection", "toPattern", toPattern, 1, 0},
    {"org.inkscape.Selection", "untile", untile, 0, 0},
    {"org.inkscape.Selection", "toSymbol", toSymbol, 0, 0},
    {"org.inkscape.Selection", "bitmapCopy", bitmapCopy, 0, 0},
    {"org.inkscape.Selection", "unSymbol", unSymbol, 0, 0},
    {"org.inkscape.Host", "message", message, 3, 1},
    {"org.inkscape.Host", "flashMessage", flashMessage, 3, 1},
    {"org.inkscape.Host", "cancelMessage", cancelMessage, 1, 0},
    {"org.inkscape.Host", "getPref", getPref, 4, 1},
    {"org.inkscape.Host", "setPref", setPref, 4, 1},
    {"org.inkscape.Host", "undoLabel", undoLabel, 2, 1},
    {"org.inkscape.Host", "setUndoLabel", setUndoLabel, 2, 0},
    {"org.inkscape.Host", "undoCoalesceKey", undoCoalesceKey, 2, 1},
    {"org.inkscape.Host", "setUndoCoalesceKey", setUndoCoalesceKey, 2, 0},
    {"org.inkscape.Host", "invokeAction", invokeAction, 2, 1},
    {"org.inkscape.Host", "listActions", listActions, 0, 1},
    {"org.inkscape.FileBackend", "writeOutput", writeOutput, 2, 1},
    {"org.inkscape.FileBackend", "inputBytes", inputBytes, 2, 1},
    {"org.inkscape.Layers", "isLayer", isLayer, 1, 1},
    {"org.inkscape.Layers", "layers", layers, 1, 1},
    {"org.inkscape.Layers", "createLayer", createLayer, 4, 1},
    {"org.inkscape.Layers", "layerForObject", layerForObject, 1, 1},
    {"org.inkscape.Layers", "renameLayer", renameLayer, 4, 0},
    {"org.inkscape.Layers", "layerSolo", layerSolo, 2, 0},
    {"org.inkscape.Layers", "lockOtherLayers", lockOtherLayers, 2, 0},
    {"org.inkscape.Layers", "hideAllLayers", hideAllLayers, 2, 0},
    {"org.inkscape.Layers", "lockAllLayers", lockAllLayers, 2, 0},
    {"org.inkscape.Selection", "isSelected", isSelected, 1, 1},
    {"org.inkscape.Selection", "selectedIds", selectedIds, 0, 1},
    {"org.inkscape.Selection", "selectionNodes", selectionNodes, 0, 1},
    {"org.inkscape.Selection", "convertUnit", convertUnit, 5, 1},

    {"org.inkscape.PathBuilder", "pathBegin", pathBegin, 0, 1},
    {"org.inkscape.PathBuilder", "pathMoveTo", pathMoveTo, 3, 0},
    {"org.inkscape.PathBuilder", "pathLineTo", pathLineTo, 3, 0},
    {"org.inkscape.PathBuilder", "pathCurveTo", pathCurveTo, 7, 0},
    {"org.inkscape.PathBuilder", "pathQuadTo", pathQuadTo, 5, 0},
    {"org.inkscape.PathBuilder", "pathArcTo", pathArcTo, 8, 0},
    {"org.inkscape.PathBuilder", "pathClose", pathClose, 1, 0},
    {"org.inkscape.PathBuilder", "pathFeed", pathFeed, 2, 1},
    {"org.inkscape.PathBuilder", "pathApply", pathApply, 2, 1},
    {"org.inkscape.PathBuilder", "pathString", pathString, 3, 1},
    {"org.inkscape.PathBuilder", "pathParse", pathParse, 3, 1},
    {"org.inkscape.PathBuilder", "pathHull", pathHull, 2, 1},
    {"org.inkscape.SVGGeometryElement", "tangentAtLength", tangentAtLength, 3, 1},
    {"org.inkscape.SVGGeometryElement", "selfIntersections", selfIntersections, 1, 1},
    {"org.inkscape.Selection", "selectedNodeCount", selectedNodeCount, 1, 1},
    {"org.inkscape.Selection", "selectedNode", selectedNode, 3, 1},
    {"org.inkscape.Cancellation", "isCancelled", isCancelled, 0, 1},
    {"org.inkscape.Selection", "selectionClear", selectionClear, 0, 0},
    {"org.inkscape.Selection", "selectionAdd", selectionAdd, 1, 0},
    {"org.inkscape.Selection", "selectionRemove", selectionRemove, 1, 0},
    {"org.inkscape.Selection", "selectionSet", selectionSet, 1, 0},
    {"org.inkscape.Selection", "selectionBounds", selectionBounds, 2, 1},
    {"org.inkscape.Selection", "toCurves", toCurves, 0, 0},
    {"org.inkscape.Params", "paramColor", paramColor, 2, 1},
    {"org.inkscape.Params", "paramOptionGroup", paramOptionGroup, 4, 1},
    {"org.inkscape.Params", "paramPath", paramPath, 4, 1},
    {"org.inkscape.Params", "paramNotebook", paramNotebook, 4, 1},
    {"org.inkscape.Params", "setParam", setParam, 4, 1},

    // Pages -- document state, so these work headless.
    {"org.inkscape.Pages", "pageCount", pageCount, 1, 1},
    {"org.inkscape.Pages", "pages", pages, 1, 1},
    {"org.inkscape.Pages", "pageIndex", pageIndex, 1, 1},
    {"org.inkscape.Pages", "selectedPage", selectedPage, 1, 1},
    {"org.inkscape.Pages", "newPage", newPage, 5, 1},
    {"org.inkscape.Pages", "deletePage", deletePage, 2, 0},
    {"org.inkscape.Pages", "pageRect", pageRect, 3, 1},
    {"org.inkscape.Pages", "pageMargin", pageMargin, 3, 1},
    {"org.inkscape.Pages", "resizePage", resizePage, 3, 0},
    {"org.inkscape.Pages", "selectPage", selectPage, 1, 1},
    {"org.inkscape.Pages", "pageItems", pageItems, 2, 1},
    {"org.inkscape.Pages", "fitPageToSelection", fitPageToSelection, 2, 0},
};

/** @return true if @a name is exactly @a text. */
bool nameIs(wasm_name_t const *name, char const *text)
{
    size_t const length = strlen(text);
    return name->size == length && memcmp(name->data, text, length) == 0;
}

/**
 * Look one export up by name -- §7.1.7 instance_export.
 *
 * Total, and unique, because step 1 of that operation asserts a valid module instance's
 * export names are distinct. Note the name carries no trailing NUL: a WebAssembly name is a
 * byte string of its own length (§5.5.5), and one built with a NUL on the end would match no
 * export a module ever declares.
 *
 * @return an owned handle onto the export, or nullptr. The caller releases it, and not
 *         before it has finished with whatever it took out of it: wasm_extern_as_func and
 *         wasm_extern_as_memory hand back a borrow of the handle, not a copy.
 */
wasm_extern_t *exportNamed(wasm_instance_t const *instance, char const *name)
{
    wasm_name_t query;
    wasm_name_new_from_string(&query, name);
    wasm_extern_t *found = wasm_instance_export(instance, &query);
    wasm_name_delete(&query);
    return found;
}

/**
 * Render one thrown value.
 *
 * A reference is reported as a reference and nothing more. WasmGC aggregates are opaque to
 * the host (§7.1), so a plugin that throws an object really has handed over something the
 * host cannot read; saying so is the honest answer, where inventing a rendering would be
 * describing a value nobody looked at.
 */
std::string describeValue(wasm_val_t const &value)
{
    switch (value.kind) {
        case WASM_I32:
            return std::to_string(value.of.i32);
        case WASM_I64:
            return std::to_string(value.of.i64);
        case WASM_F32:
            return std::to_string(value.of.f32);
        case WASM_F64:
            return std::to_string(value.of.f64);
        case WASM_V128:
            return "<v128>";
        default:
            return value.of.ref ? "<ref>" : "null";
    }
}

/**
 * Say what ended the call.
 *
 * §7.1.8 gives an invocation two ways to fail, and they are different facts about the
 * plugin: a trap is a fault in the module, while an escaped exception is a `throw` it never
 * caught. Reporting both as "trapped" would tell whoever wrote the plugin the wrong thing
 * about their own code.
 *
 * §7.1.12 exn_read is what makes the second one legible -- it is the operation that gets the
 * thrown values back out -- and it is a spec embedding operation, not an engine extension.
 * The community wasm-c-api header renders the pre-3.0 appendix and omits it; the spec is the
 * interface, so this uses it.
 */
std::string describeFailure(wasm_trap_t *trap)
{
    if (wasm_trap_is_exception(trap)) {
        if (auto const *exception = wasm_trap_exception(trap)) {
            wasm_val_vec_t payload = WASM_EMPTY_VEC;
            wasm_exception_read(exception, &payload);

            std::string detail("uncaught exception");
            for (size_t i = 0; i < payload.size; ++i) {
                detail += (i == 0) ? " (" : ", ";
                detail += describeValue(payload.data[i]);
            }
            if (payload.size) {
                detail += ")";
            }
            wasm_val_vec_delete(&payload);
            return detail;
        }
    }

    wasm_message_t message = WASM_EMPTY_VEC;
    wasm_trap_message(trap, &message);
    // The message is null-terminated by convention, and the terminator is inside `size`;
    // trimming it keeps it out of the middle of the sentence this ends up in.
    size_t length = message.size;
    while (length && message.data[length - 1] == '\0') {
        --length;
    }
    std::string detail(message.data ? message.data : "", length);
    if (message.size) {
        wasm_byte_vec_delete(&message);
    }
    return detail;
}

} // namespace

WasmBackend::WasmBackend() = default;

WasmBackend::~WasmBackend()
{
    unload(nullptr);
}

/**
 * Read the <wasm> element of the .inx, then compile the module it names.
 *
 * A nested <module> element carries the entry point's name as an attribute and the module file
 * as its text content, resolved through the same `location` vocabulary <script> and <xslt>
 * dependencies use:
 *
 *     <wasm>
 *         <module entry="org.inkscape.Plugin.effect(I)I" location="inx">gnome.wasm</module>
 *     </wasm>
 *
 * The entry point is named in the .inx because a guest language decorates the symbol however it
 * likes, so there is no name the host could guess.
 *
 * Compilation happens here rather than per invocation because it is the expensive half;
 * each run then only needs a fresh store and instance.
 */
bool WasmBackend::load(Inkscape::Extension::Extension *module)
{
    if (module->loaded()) {
        return true;
    }

    std::string filename;
    for (auto child = module->get_repr()->firstChild(); child != nullptr; child = child->next()) {
        if (strcmp(child->name(), INKSCAPE_EXTENSION_NS "wasm") != 0) {
            continue;
        }
        for (auto element = child->firstChild(); element != nullptr; element = element->next()) {
            if (element->type() != Inkscape::XML::NodeType::ELEMENT_NODE) {
                continue; // skip non-element nodes (see LP #1372200)
            }
            // Named, the way <script> looks for <command> and <xslt> for <file>. Taking
            // whatever element came first would accept a misspelling that the .inx schema
            // rejects, so the two would disagree about the same file.
            if (strcmp(element->name(), INKSCAPE_EXTENSION_NS "module") != 0) {
                continue;
            }
            if (auto entry = element->attribute("entry")) {
                _entry = entry;
            }
            if (auto content = element->firstChild()) {
                // Resolved through the dependency the extension registered for this element,
                // so a module that is missing is reported by check() like any other file.
                filename = module->get_dependency_location(content->content());
            }
            break;
        }
        break;
    }

    if (filename.empty() || _entry.empty()) {
        g_warning("WasmBackend::load: <wasm> needs a <module entry=\"...\"> naming the .wasm file");
        return false;
    }

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        g_warning("WasmBackend::load: cannot read '%s'", filename.c_str());
        return false;
    }
    auto const size = file.tellg();
    file.seekg(0);
    _module_bytes.resize(static_cast<size_t>(size));
    if (!file.read(_module_bytes.data(), size)) {
        g_warning("WasmBackend::load: short read on '%s'", filename.c_str());
        return false;
    }

    _engine = wasm_engine_new();
    if (!_engine) {
        g_warning("WasmBackend::load: could not create a WebAssembly engine");
        return false;
    }
    _store = wasm_store_new(_engine);

    wasm_byte_vec_t binary;
    wasm_byte_vec_new_uninitialized(&binary, _module_bytes.size());
    memcpy(binary.data, _module_bytes.data(), _module_bytes.size());
    _compiled = wasm_module_new(_store, &binary);
    wasm_byte_vec_delete(&binary);

    if (!_compiled) {
        g_warning("WasmBackend::load: '%s' was rejected by the engine", filename.c_str());
        return false;
    }

    return true;
}

void WasmBackend::unload(Inkscape::Extension::Extension * /*module*/)
{
    // Deletion order matters: the module and store belong to the engine, so the engine goes last.
    if (_compiled) {
        wasm_module_delete(_compiled);
        _compiled = nullptr;
    }
    if (_store) {
        wasm_store_delete(_store);
        _store = nullptr;
    }
    if (_engine) {
        wasm_engine_delete(_engine);
        _engine = nullptr;
    }
    _module_bytes.clear();
    _entry.clear();
}

void WasmBackend::effect(Inkscape::Extension::Effect *module, ExecutionEnv *executionEnv, SPDesktop *desktop,
                         ImplementationDocumentCache * /*docCache*/)
{
    if (!desktop) {
        g_warning("WasmBackend::effect: no desktop");
        return;
    }
    _run(module, executionEnv, desktop->getDocument(), desktop);
}

void WasmBackend::effect(Inkscape::Extension::Effect *module, ExecutionEnv *executionEnv, SPDocument *document)
{
    // No desktop: the session accessors report absence rather than guessing.
    _run(module, executionEnv, document, nullptr);
}

/**
 * Roll the invocation back and tell the user why.
 *
 * Both halves matter. Rolling back without saying anything leaves someone who picked a menu
 * item watching nothing happen, with the reason on a console they are not reading -- which is
 * how a broken extension gets reported as "the menu item does nothing". Script puts its
 * failures in a dialog for the same reason, so this is parity rather than embellishment.
 *
 * Silent on the command line, where a dialog cannot be shown and the warning already went to
 * stderr, which is where a script driving Inkscape will look for it.
 *
 * Silent under a live preview too, which asks for it through ExecutionEnv::show_errors(). The
 * preview re-runs the effect on every parameter change, and gui_warning() runs the dialog to
 * response -- so reporting there raises a modal on each pass, from inside the main loop the
 * preview is driven by, and the parameter dialog cannot be reached to correct the parameter
 * that is failing. The rollback still happens; only the dialog is withheld.
 */
void WasmBackend::_reportFailure(ExecutionEnv *executionEnv, SPDesktop *desktop, Glib::ustring const &message)
{
    if (executionEnv) {
        executionEnv->undo();
    }
    if (desktop && (!executionEnv || executionEnv->show_errors())) {
        Inkscape::UI::gui_warning(message.raw());
    }
}

bool WasmBackend::_run(Inkscape::Extension::Extension *module, ExecutionEnv *executionEnv, SPDocument *document,
                       SPDesktop *desktop, std::string *output, std::string const *input)
{
    if (!_compiled || !document) {
        return false;
    }
    bool succeeded = false;

    // Cleared per run: a cancellation belongs to the invocation that was cancelled, and
    // leaving it set would stop the next one before it started.
    _cancelled = false;

    // Likewise the undo naming: it belongs to one invocation, and a module that set a label
    // last time must not have it applied to a run where it said nothing.
    _undo_label.clear();
    _undo_coalesce_key.clear();

    WasmInvocation invocation(_store, document);
    // The document's own selection when there is no desktop, rather than nothing. SPDocument
    // builds one in its constructor and every ObjectSet operation guards its desktop use --
    // toCurves touches the desktop only to flash a message and set a cursor -- so the selection
    // is usable headless, which is where the shipped effect extensions run. Passing null
    // instead makes every selection operation a silent no-op on the command line.
    auto *selection = desktop ? desktop->getSelection() : document->getSelection();
    invocation.setSession(desktop, selection, module);
    invocation.setCancelFlag(&_cancelled);
    invocation.setInput(input);

    // Satisfy the module's imports in the order it declares them, which is the order §7.1.6
    // instantiate takes them in. Imports are positional by construction -- unlike exports,
    // which are looked up by name below -- so the vector is built by walking the module's
    // own declarations rather than by matching a table of ours against them.
    wasm_importtype_vec_t import_types = WASM_EMPTY_VEC;
    wasm_module_imports(_compiled, &import_types);

    wasm_extern_vec_t imports = WASM_EMPTY_VEC;
    wasm_extern_vec_new_uninitialized(&imports, import_types.size);

    bool resolved_all = true;
    for (size_t i = 0; i < import_types.size; ++i) {
        auto const *module_name = wasm_importtype_module(import_types.data[i]);
        auto const *field_name = wasm_importtype_name(import_types.data[i]);
        auto const *type = wasm_externtype_as_functype_const(wasm_importtype_type(import_types.data[i]));

        imports.data[i] = nullptr;
        if (!type) {
            resolved_all = false;
            g_warning("WasmBackend: import '%.*s.%.*s' is not a function", (int)module_name->size, module_name->data,
                      (int)field_name->size, field_name->data);
            continue;
        }

        for (auto const &candidate : host_functions) {
            if (!nameIs(module_name, candidate.module) || !nameIs(field_name, candidate.name)) {
                continue;
            }
            // A name that matches but a signature that does not is left unresolved rather
            // than bound: binding it would hand the module a function it did not describe.
            if (wasm_functype_params(type)->size != candidate.param_count ||
                wasm_functype_results(type)->size != candidate.result_count) {
                break;
            }
            auto *func = wasm_func_new_with_env(_store, type, candidate.callback, &invocation, nullptr);
            imports.data[i] = wasm_func_as_extern(func);
            break;
        }

        if (!imports.data[i]) {
            resolved_all = false;
            g_warning("WasmBackend: unresolved import '%.*s.%.*s'", (int)module_name->size, module_name->data,
                      (int)field_name->size, field_name->data);
        }
    }
    wasm_importtype_vec_delete(&import_types);

    if (!resolved_all) {
        wasm_extern_vec_delete(&imports);
        return false;
    }

    wasm_trap_t *trap = nullptr;
    wasm_instance_t *instance = wasm_instance_new(_store, _compiled, &imports, &trap);
    wasm_extern_vec_delete(&imports);
    if (!instance) {
        g_warning("WasmBackend: instantiation failed");
        if (trap) {
            wasm_trap_delete(trap);
        }
        return false;
    }

    // Both exports are looked up by name, which §7.1.7 instance_export is: a valid instance's
    // export names are distinct, so the answer is unique. The staging memory is named rather
    // than found by kind because kind stopped identifying it -- WebAssembly 3.0 allows an
    // instance more than one memory, and "the one that is a memory" then picks arbitrarily
    // among them.
    wasm_extern_t *memory_export = exportNamed(instance, "memory");
    wasm_extern_t *entry_export = exportNamed(instance, _entry.c_str());

    auto *memory = memory_export ? wasm_extern_as_memory(memory_export) : nullptr;
    auto *entry = entry_export ? wasm_extern_as_func(entry_export) : nullptr;
    invocation.setMemory(memory);

    if (!memory) {
        g_warning("WasmBackend: the module exports no memory named 'memory'");
    } else if (!entry) {
        g_warning("WasmBackend: the module exports no '%s'", _entry.c_str());
    } else {
        wasm_val_t argv[1] = {WASM_I32_VAL(invocation.makeHandle(WasmHandleKind::Document, document->getReprDoc()))};
        wasm_val_t resultv[1] = {WASM_INIT_VAL};
        wasm_val_vec_t args = WASM_ARRAY_VEC(argv);
        wasm_val_vec_t results = WASM_ARRAY_VEC(resultv);

        // A trap leaves the document part-modified, so hand it back to the execution
        // environment to roll back. Committing is never ours to do: ExecutionEnv::commit()
        // owns the undo step, and calling DocumentUndo::done() here would double-commit.
        if (wasm_trap_t *call_trap = wasm_func_call(entry, &args, &results)) {
            std::string const detail = describeFailure(call_trap);
            g_warning("WasmBackend: '%s' failed: %s", _entry.c_str(), detail.c_str());
            wasm_trap_delete(call_trap);
            _reportFailure(executionEnv, desktop, Glib::ustring::compose(_("The extension stopped: %1"), detail));
        } else if (resultv[0].of.i32 != 0) {
            g_warning("WasmBackend: '%s' reported failure (%d)", _entry.c_str(), resultv[0].of.i32);
            _reportFailure(executionEnv, desktop,
                           Glib::ustring::compose(_("The extension reported an error (code %1)."), resultv[0].of.i32));
        } else {
            succeeded = true;
            if (output) {
                *output = invocation.output();
            }
        }
    }

    // Carried off the invocation before it goes: ExecutionEnv::commit() asks for these after
    // this function has returned, by which point the invocation no longer exists.
    _undo_label = invocation.undoLabel();
    _undo_coalesce_key = invocation.undoCoalesceKey();

    // The two handles are released after the call and before the instance they denote, and
    // the invocation gives up its borrow of the memory first: an extern is a handle minted
    // per lookup, so it is this code's to free, and nothing may still be pointing through it
    // when it goes.
    invocation.setMemory(nullptr);
    if (memory_export) {
        wasm_extern_delete(memory_export);
    }
    if (entry_export) {
        wasm_extern_delete(entry_export);
    }
    wasm_instance_delete(instance);
    return succeeded;
}

/**
 * Open a file through the module, as an <input> extension.
 *
 * The module is handed the file's bytes and a fresh, empty document, and builds into it with
 * the same DOM API an effect uses. No SVG text passes between them: an importer that had to
 * serialise its result for the host to re-parse would be doing the work twice and throwing away
 * everything the object tree knows on the way.
 *
 * Null on failure, which is what Input::open's caller already treats as "this extension could
 * not read it" -- returning a half-built document would be worse than returning nothing.
 */
std::unique_ptr<SPDocument> WasmBackend::open(Inkscape::Extension::Input *module, char const *filename,
                                              bool /*is_importing*/)
{
    if (!_compiled || !filename) {
        return {};
    }

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        g_warning("WasmBackend::open: cannot read '%s'", filename);
        return {};
    }
    auto const size = file.tellg();
    if (size < 0) {
        return {};
    }
    std::string bytes(static_cast<size_t>(size), '\0');
    file.seekg(0);
    if (size > 0 && !file.read(bytes.data(), size)) {
        g_warning("WasmBackend::open: reading '%s' failed", filename);
        return {};
    }

    // A blank <svg:svg> built in memory, NOT createNewDoc(filename): that one reads the path it
    // is given as SVG, which for an importer is the one thing the file is guaranteed not to be.
    auto document = SPDocument::createNewDoc(nullptr, true);
    if (!document) {
        return {};
    }
    if (!_run(module, nullptr, document.get(), nullptr, nullptr, &bytes)) {
        return {};
    }
    return document;
}

/**
 * Save the document through the module, as an <output> extension.
 *
 * The guest is handed the document and walks it with the same API an effect uses; what it emits
 * through writeOutput is what lands in the file. No ExecutionEnv and no desktop: saving is
 * not an edit, so there is no undo step to own and nothing to roll back.
 *
 * A module that traps or reports failure gets save_failed, which is what the caller already
 * handles -- the command line prints "Failed to save" and the GUI says so -- rather than
 * leaving a half-written or empty file behind as a success.
 */
void WasmBackend::save(Inkscape::Extension::Output *module, SPDocument *doc, gchar const *filename)
{
    std::string bytes;
    if (!doc || !filename || !_run(module, nullptr, doc, nullptr, &bytes)) {
        throw Inkscape::Extension::Output::save_failed();
    }

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) {
        g_warning("WasmBackend::save: cannot open '%s'", filename);
        throw Inkscape::Extension::Output::save_failed();
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        g_warning("WasmBackend::save: writing '%s' failed", filename);
        throw Inkscape::Extension::Output::save_failed();
    }
}

/**
 * Convert Inkscape's rendered PNG into some other raster format, as a raster <output> extension.
 *
 * Bytes in, bytes out: the module gets the PNG through inputBytes and answers through
 * writeOutput, exactly as the other two backends do. It is handed a blank document rather
 * than the one being exported -- the hook receives that const, and a raster conversion has no
 * business walking it anyway; Script uses it only to set environment variables for its
 * subprocess, which a sandboxed module does not have.
 */
void WasmBackend::export_raster(Inkscape::Extension::Output *module, SPDocument const * /*doc*/,
                                std::string const &png_file, gchar const *filename)
{
    if (!module || !module->is_raster()) {
        g_warning("WasmBackend::export_raster: not a raster extension");
        throw Inkscape::Extension::Output::save_failed();
    }

    std::ifstream png(png_file, std::ios::binary | std::ios::ate);
    if (!png) {
        g_warning("WasmBackend::export_raster: cannot read '%s'", png_file.c_str());
        throw Inkscape::Extension::Output::save_failed();
    }
    auto const size = png.tellg();
    std::string source(size > 0 ? static_cast<size_t>(size) : 0u, '\0');
    png.seekg(0);
    if (size > 0 && !png.read(source.data(), size)) {
        throw Inkscape::Extension::Output::save_failed();
    }

    auto scratch = SPDocument::createNewDoc(nullptr, true);
    std::string bytes;
    if (!scratch || !filename || !_run(module, nullptr, scratch.get(), nullptr, &bytes, &source)) {
        throw Inkscape::Extension::Output::save_failed();
    }

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        g_warning("WasmBackend::export_raster: writing '%s' failed", filename);
        throw Inkscape::Extension::Output::save_failed();
    }
}

/**
 * Run the effect against one item, ignoring the selection.
 *
 * The Extensions Gallery uses this to draw a thumbnail: it opens a sample document, finds the
 * item called "test-object" and asks each effect to apply itself to that one thing
 * (extensions-gallery.cpp render_icon()). Without it a wasm effect gets the blank placeholder
 * every other extension avoids.
 *
 * The item's own document is used and the item is made the selection for the duration, which is
 * what "apply yourself to this" means for an interface whose operations all act on a selection.
 */
bool WasmBackend::apply_filter(Inkscape::Extension::Effect *module, SPItem *item)
{
    if (!item || !item->document) {
        return false;
    }
    auto *selection = item->document->getSelection();
    if (!selection) {
        return false;
    }
    selection->set(item);
    return _run(module, nullptr, item->document, nullptr);
}

bool WasmBackend::cancelProcessing()
{
    _cancelled = true;
    return true;
}

bool WasmBackend::check(Inkscape::Extension::Extension *module)
{
    for (auto child = module->get_repr()->firstChild(); child != nullptr; child = child->next()) {
        if (strcmp(child->name(), INKSCAPE_EXTENSION_NS "wasm") == 0) {
            return true;
        }
    }
    return false;
}

} // namespace Inkscape::Extension::Implementation

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
