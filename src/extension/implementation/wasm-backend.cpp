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
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>
#include <wasm.h>

#include "desktop.h"
#include "document.h"
#include "extension/effect.h"
#include "extension/execution-env.h"
#include "extension/extension.h"
#include "gc-anchored.h"
#include "layer-manager.h"
#include "libnrtype/Layout-TNG.h"
#include "object/sp-flowtext.h"
#include "object/sp-item-group.h"
#include "object/sp-shape.h"
#include "object/sp-text.h"
#include "selection.h"
#include "style.h"
#include "ui/util.h"
#include "util/cast.h"
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
    results->data[0] = WASM_I32_VAL(invocation->writeString(args->data[1].of.i32, args->data[2].of.i32, node->name()));
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
    results->data[0] =
        WASM_I32_VAL(invocation->writeString(args->data[1].of.i32, args->data[2].of.i32, colon ? colon + 1 : name));
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
    results->data[0] = WASM_I32_VAL(invocation->writeString(args->data[1].of.i32, args->data[2].of.i32, uri));
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
    results->data[0] = WASM_I32_VAL(invocation->writeString(args->data[2].of.i32, args->data[3].of.i32, value));
    return nullptr;
}

/** org.inkscape.Node.textContent(node, out_offset, out_capacity) -> i32 */
wasm_trap_t *getTextContent(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    INK_NODE_ARG(node, "textContent")
    results->data[0] =
        WASM_I32_VAL(invocation->writeString(args->data[1].of.i32, args->data[2].of.i32, node->content()));
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
 * The result tells absence and overflow apart; @see WasmInvocation::writeString().
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

    results->data[0] =
        WASM_I32_VAL(invocation->writeString(args->data[3].of.i32, args->data[4].of.i32, node->attribute(key.c_str())));
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
    Geom::OptRect box = visual || clipped
                            ? item->visualBounds(Geom::identity(), true, clipped, clipped)
                            : item->geometricBounds();

    if (!box) {
        results->data[0] = WASM_I32_VAL(0);
        return nullptr;
    }

    double const rect[4] = {box->left(), box->top(), box->width(), box->height()};
    if (!invocation->writeDoubles(args->data[2].of.i32, rect, 4)) {
        return invocation->trap("getBBox: result is outside the module's memory");
    }
    results->data[0] = WASM_I32_VAL(1);
    return nullptr;
}

/** Write a Geom::Affine as the six numbers of a DOMMatrix. */
bool writeAffine(WasmInvocation *invocation, int32_t offset, Geom::Affine const &affine)
{
    double const values[6] = {affine[0], affine[1], affine[2], affine[3], affine[4], affine[5]};
    return invocation->writeDoubles(offset, values, 6);
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

/** org.inkscape.Document.getComputedStyle(element) -> DOMStringList of "name:value" */
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
            if (property && property->set) {
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

    if (item && item->style) {
        for (auto const *property : item->style->properties()) {
            // Deliberately not filtered on `set`. A property the element inherits rather than
            // declares still has a computed value, and reporting nothing for it would make
            // this a specified-style lookup wearing the name of a computed one -- the exact
            // thing that sends a plugin back to walking ancestors itself, which is what
            // being in-process is supposed to make unnecessary.
            if (property && property->name() == wanted.c_str()) {
                auto const value = computedValue(property);
                results->data[0] =
                    WASM_I32_VAL(invocation->writeString(args->data[3].of.i32, args->data[4].of.i32, value.c_str()));
                return nullptr;
            }
        }
    }
    results->data[0] = WASM_I32_VAL(WasmInvocation::STRING_ABSENT);
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

/** org.inkscape.Inkscape.inkParamString(name_off, name_len, out, cap) -> i32 */
wasm_trap_t *inkParamString(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    INK_STRING_ARG(name, 0, "inkParamString")
    auto const *effect = invocation->effect();
    char const *value = effect ? effect->get_param_string(name.c_str(), nullptr) : nullptr;
    results->data[0] = WASM_I32_VAL(invocation->writeString(args->data[2].of.i32, args->data[3].of.i32, value));
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
        auto const *effect = invocation->effect();                                           \
        results->data[0] = wrap(effect ? effect->getter(name.c_str(), fallback) : fallback); \
        return nullptr;                                                                      \
    }

INK_PARAM_GETTER(inkParamFloat, "inkParamFloat", get_param_float, WASM_F64_VAL, 0.0)
INK_PARAM_GETTER(inkParamInt, "inkParamInt", get_param_int, WASM_I32_VAL, 0)
INK_PARAM_GETTER(inkParamBool, "inkParamBool", get_param_bool, WASM_I32_VAL, false)

/**
 * org.inkscape.Inkscape.inkCurrentLayer() -> Node
 *
 * Reachable as getElementById(namedview/@inkscape:current-layer), but open-coding that in
 * every plugin is worse than one accessor. Absent without a desktop, which is the honest
 * answer: a headless run has no current layer, only a document.
 */
wasm_trap_t *inkCurrentLayer(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
{
    auto *invocation = invocationOf(env);
    auto *desktop = invocation->desktop();
    auto *layer = desktop ? desktop->layerManager().currentLayer() : nullptr;
    results->data[0] = WASM_I32_VAL(invocation->makeHandle(WasmHandleKind::Node, layer ? layer->getRepr() : nullptr));
    return nullptr;
}

/**
 * org.inkscape.Inkscape.inkIsSelected(element) -> i32
 *
 * Per element rather than a list to walk, which is Blender's model: selection is a property
 * of the thing selected (BezTriple.select_control_point), not a separate structure the
 * caller has to keep in step. Only the interface is per-element; the backing is Inkscape's
 * existing Selection, unchanged.
 */
wasm_trap_t *inkIsSelected(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
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

/** org.inkscape.Inkscape.inkSelectedIds() -> DOMStringList */
wasm_trap_t *inkSelectedIds(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
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

/** org.inkscape.Inkscape.inkSelectedNodeCount(element) -> i32 */
wasm_trap_t *inkSelectedNodeCount(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
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
 * org.inkscape.Inkscape.inkSelectedNode(element, index, out_offset) -> i32 present
 *
 * Writes the subpath and node indices as two i32s.
 *
 * These are positional, unavoidably: an SVG path node is an offset into the `d` attribute,
 * not an element, so there is nothing to hang a flag on and Blender's model does not carry
 * over. The indices are therefore only meaningful until `element` is modified -- rewriting
 * `d` with a different node count invalidates them. Documented rather than silently unsafe,
 * which is what --selected-nodes on the command line is today.
 */
wasm_trap_t *inkSelectedNode(void *env, wasm_val_vec_t const *args, wasm_val_vec_t *results)
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
                return invocation->trap("inkSelectedNode: result is outside the module's memory");
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
 * org.inkscape.Inkscape.inkIsCancelled() -> i32
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
wasm_trap_t *inkIsCancelled(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t *results)
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

/** org.inkscape.Inkscape.inkSelectionClear() */
wasm_trap_t *inkSelectionClear(void *env, wasm_val_vec_t const * /*args*/, wasm_val_vec_t * /*results*/)
{
    if (auto *selection = invocationOf(env)->selection()) {
        selection->clear();
    }
    return nullptr;
}

/**
 * org.inkscape.Inkscape.inkSelectionAdd(element)
 *
 * Selection is mutable because builtins treat it as one: BlurEdge clears it and re-adds
 * items as a working register. The execution environment restores the user's selection
 * afterwards, so this is parity with a builtin, not extra power.
 */
wasm_trap_t *inkSelectionAdd(void *env, wasm_val_vec_t const *args, wasm_val_vec_t * /*results*/)
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

    // SVGGraphicsElement / SVGGeometryElement -- SVG2
    {"org.inkscape.SVGGraphicsElement", "getBBox", getBBox, 3, 1},
    {"org.inkscape.SVGGraphicsElement", "getCTM", getCTM, 2, 1},
    {"org.inkscape.SVGGraphicsElement", "getScreenCTM", getScreenCTM, 2, 1},
    {"org.inkscape.SVGGeometryElement", "getTotalLength", getTotalLength, 1, 1},
    {"org.inkscape.SVGGeometryElement", "getPointAtLength", getPointAtLength, 3, 1},
    {"org.inkscape.SVGGeometryElement", "isPointInFill", isPointInFill, 3, 1},
    {"org.inkscape.SVGGeometryElement", "isPointInStroke", isPointInStroke, 3, 1},

    // SVGTextContentElement -- SVG2. Backed by Inkscape::Text::Layout.
    {"org.inkscape.SVGTextContentElement", "getNumberOfChars", getNumberOfChars, 1, 1},
    {"org.inkscape.SVGTextContentElement", "getComputedTextLength", getComputedTextLength, 1, 1},
    {"org.inkscape.SVGTextContentElement", "getSubStringLength", getSubStringLength, 3, 1},
    {"org.inkscape.SVGTextContentElement", "getStartPositionOfChar", getStartPositionOfChar, 3, 1},
    {"org.inkscape.SVGTextContentElement", "getEndPositionOfChar", getEndPositionOfChar, 3, 1},
    {"org.inkscape.SVGTextContentElement", "getExtentOfChar", getExtentOfChar, 3, 1},
    {"org.inkscape.SVGTextContentElement", "getRotationOfChar", getRotationOfChar, 2, 1},
    {"org.inkscape.SVGTextContentElement", "getCharNumAtPosition", getCharNumAtPosition, 3, 1},

    // SVGSVGElement hit testing -- SVG2
    {"org.inkscape.SVGSVGElement", "getEnclosureList", getEnclosureList, 1, 1},
    {"org.inkscape.SVGSVGElement", "getIntersectionList", getIntersectionList, 1, 1},
    {"org.inkscape.SVGSVGElement", "checkEnclosure", checkEnclosure, 2, 1},
    {"org.inkscape.SVGSVGElement", "checkIntersection", checkIntersection, 2, 1},

    // Computed style -- CSSOM
    {"org.inkscape.Document", "getComputedStyle", getComputedStyle, 1, 1},
    {"org.inkscape.CSSStyleDeclaration", "getPropertyValue", getPropertyValue, 5, 1},

    // Session tier -- vendor, `ink` prefixed. Not standard surface.
    {"org.inkscape.Inkscape", "inkParamString", inkParamString, 4, 1},
    {"org.inkscape.Inkscape", "inkParamFloat", inkParamFloat, 2, 1},
    {"org.inkscape.Inkscape", "inkParamInt", inkParamInt, 2, 1},
    {"org.inkscape.Inkscape", "inkParamBool", inkParamBool, 2, 1},
    {"org.inkscape.Inkscape", "inkCurrentLayer", inkCurrentLayer, 0, 1},
    {"org.inkscape.Inkscape", "inkIsSelected", inkIsSelected, 1, 1},
    {"org.inkscape.Inkscape", "inkSelectedIds", inkSelectedIds, 0, 1},
    {"org.inkscape.Inkscape", "inkSelectedNodeCount", inkSelectedNodeCount, 1, 1},
    {"org.inkscape.Inkscape", "inkSelectedNode", inkSelectedNode, 3, 1},
    {"org.inkscape.Inkscape", "inkIsCancelled", inkIsCancelled, 0, 1},
    {"org.inkscape.Inkscape", "inkSelectionClear", inkSelectionClear, 0, 0},
    {"org.inkscape.Inkscape", "inkSelectionAdd", inkSelectionAdd, 1, 0},
};

/** @return true if @a name is exactly @a text. */
bool nameIs(wasm_name_t const *name, char const *text)
{
    size_t const length = strlen(text);
    return name->size == length && memcmp(name->data, text, length) == 0;
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
 * The element carries the module file and the name of its entry point:
 *
 *     <wasm module="gnome.wasm" entry="org.inkscape.Plugin.effect(I)I" />
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
 */
void WasmBackend::_reportFailure(ExecutionEnv *executionEnv, SPDesktop *desktop, Glib::ustring const &message)
{
    if (executionEnv) {
        executionEnv->undo();
    }
    if (desktop) {
        Inkscape::UI::gui_warning(message.raw());
    }
}

void WasmBackend::_run(Inkscape::Extension::Effect *module, ExecutionEnv *executionEnv, SPDocument *document,
                       SPDesktop *desktop)
{
    if (!_compiled || !document) {
        return;
    }

    // Cleared per run: a cancellation belongs to the invocation that was cancelled, and
    // leaving it set would stop the next one before it started.
    _cancelled = false;

    WasmInvocation invocation(_store, document);
    invocation.setSession(desktop, desktop ? desktop->getSelection() : nullptr, module);
    invocation.setCancelFlag(&_cancelled);

    // Satisfy the module's imports in the order it declares them, which is the order the
    // instance expects; there is no by-name lookup in the C API to do this any other way.
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
        return;
    }

    wasm_trap_t *trap = nullptr;
    wasm_instance_t *instance = wasm_instance_new(_store, _compiled, &imports, &trap);
    wasm_extern_vec_delete(&imports);
    if (!instance) {
        g_warning("WasmBackend: instantiation failed");
        if (trap) {
            wasm_trap_delete(trap);
        }
        return;
    }

    // Find the staging memory and the entry point together: the C API offers no lookup by
    // name, only two vectors in the same order -- names from the module, values from the
    // instance -- so both are picked out of one walk.
    wasm_exporttype_vec_t export_types = WASM_EMPTY_VEC;
    wasm_extern_vec_t exports = WASM_EMPTY_VEC;
    wasm_module_exports(_compiled, &export_types);
    wasm_instance_exports(instance, &exports);

    wasm_func_t *entry = nullptr;
    for (size_t i = 0; i < export_types.size && i < exports.size; ++i) {
        auto const *name = wasm_exporttype_name(export_types.data[i]);
        if (wasm_extern_kind(exports.data[i]) == WASM_EXTERN_MEMORY) {
            invocation.setMemory(wasm_extern_as_memory(exports.data[i]));
        } else if (!entry && nameIs(name, _entry.c_str())) {
            entry = wasm_extern_as_func(exports.data[i]);
        }
    }
    wasm_exporttype_vec_delete(&export_types);

    if (!entry) {
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
            wasm_message_t message = WASM_EMPTY_VEC;
            wasm_trap_message(call_trap, &message);
            std::string const detail(message.data ? message.data : "", message.size);
            g_warning("WasmBackend: '%s' trapped: %s", _entry.c_str(), detail.c_str());
            if (message.size) {
                wasm_byte_vec_delete(&message);
            }
            wasm_trap_delete(call_trap);
            _reportFailure(executionEnv, desktop, Glib::ustring::compose(_("The extension stopped: %1"), detail));
        } else if (resultv[0].of.i32 != 0) {
            g_warning("WasmBackend: '%s' reported failure (%d)", _entry.c_str(), resultv[0].of.i32);
            _reportFailure(executionEnv, desktop,
                           Glib::ustring::compose(_("The extension reported an error (code %1)."),
                                                  resultv[0].of.i32));
        }
    }

    wasm_extern_vec_delete(&exports);
    wasm_instance_delete(instance);
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
