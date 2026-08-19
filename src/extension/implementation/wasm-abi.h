// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Marshalling between a WebAssembly guest and the document.
 *
 * Authors:
 *   Dan Eicher <dan.eicher@gmail.com>
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_EXTENSION_IMPLEMENTATION_WASM_ABI_H
#define INKSCAPE_EXTENSION_IMPLEMENTATION_WASM_ABI_H

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <wasm.h>

class SPDesktop;
class SPDocument;
class SPItem;

namespace Inkscape {
class Selection;
namespace XML {
class Document;
class Node;
} // namespace XML
namespace Extension {
class Effect;
} // namespace Extension
} // namespace Inkscape

namespace Inkscape::Extension::Implementation {

/**
 * What a handle refers to.
 *
 * The kind is checked on every lookup, so a guest cannot take the integer it was given for
 * a Node and present it where an Item is expected. Without that check the two would be
 * interchangeable, since both are just table indices.
 */
enum class WasmHandleKind
{
    None,
    Document,
    Node,
    NodeList,
    NodeSnapshot,
    StringList
};

/**
 * The state one invocation of a wasm extension may touch.
 *
 * Passed as the environment of every host function, rather than held in globals: Inkscape
 * has several documents open, re-runs an extension on each live-preview change, and may have
 * more than one wasm extension loaded, so there is no single "current" anything.
 *
 * Guests never see a pointer. They get i32 handles into a table that lives exactly as long as
 * the invocation, which is what stops a module stashing a Node in a global and dereferencing
 * it after the document has moved on.
 */
class WasmInvocation
{
public:
    WasmInvocation(wasm_store_t *store, SPDocument *document);
    ~WasmInvocation();

    /**
     * Adopt the staging memory, once instantiation has revealed which one it is.
     *
     * Host functions are created before the instance exists, because they are its imports,
     * so the memory cannot be known at construction time.
     */
    void setMemory(wasm_memory_t *memory) { _memory = memory; }

    /** Result of a string-valued operation; @see writeString(). */
    static constexpr int32_t STRING_ABSENT = -1;

    /**
     * Hand out a handle for @a ptr, or 0 if it is null.
     *
     * The same object always gets the same handle within one invocation. That is what makes
     * identity comparable: a plugin that reaches a node two ways -- getElementById here,
     * parentNode there -- must be able to see that they are the same node, exactly as DOM's
     * `===` does. Without interning, handles would compare unequal for the same element and
     * every plugin would have to fall back on comparing id attributes.
     *
     * Handles are not stable ACROSS invocations, and are not pointers.
     */
    int32_t makeHandle(WasmHandleKind kind, void *ptr);

    /**
     * Take responsibility for a node the guest asked us to create.
     *
     * Inkscape's collector anchors a newly created node with a refcount of one, and the
     * convention is that a node inside the document sits at zero -- its parent holds it
     * alive. So the reference has to be dropped once, by whoever created it.
     *
     * Doing that here, at the end of the invocation, rather than at the point of insertion,
     * because insertion is not the only ending: a node the guest creates and never parents
     * would otherwise stay anchored forever, and one it parents twice would be released
     * twice. One anchor, one release, regardless of what happens in between.
     */
    void own(Inkscape::XML::Node *node);

    /** @return the document for @a handle, or nullptr if it is unknown or not a document. */
    Inkscape::XML::Document *getDocument(int32_t handle) const;

    /** @return the node for @a handle, or nullptr if it is unknown, stale or not a node. */
    Inkscape::XML::Node *getNode(int32_t handle) const;

    /**
     * Resolve a Node handle to the item that draws it.
     *
     * The query half takes Element handles, not a separate Item handle, because in SVG2 an
     * SVGGraphicsElement *is* an Element -- splitting them in the ABI would be an invention.
     *
     * The object tree lags the repr, so this brings the document up to date first: a plugin
     * that appends a node and immediately measures it must not read a stale answer. Doing it
     * here rather than making plugins call a flush is deliberate; a required call that is
     * only sometimes needed is a trap for every author who does not know about it.
     *
     * @return nullptr when the node draws nothing -- a comment, a <defs> child, an element
     *         Inkscape has no class for. That is a real answer, not an error.
     *
     * Takes a node rather than a handle so that the two ways of getting nothing stay apart:
     * a handle that is not a Node at all is a mistake in the plugin and traps at the call
     * site, while a Node that draws nothing answers "absent". Folding them together would
     * leave a plugin unable to tell a bug in its own code from a fact about the drawing.
     */
    SPItem *itemFor(Inkscape::XML::Node *node) const;

    /**
     * @return the node a NodeList handle stands for, or nullptr if it is not one.
     *
     * A NodeList is its parent node: DOM wants a live collection, and re-reading the node on
     * each access is that, with nothing to invalidate. The kind still differs from Node, so
     * the two cannot be used interchangeably.
     */
    Inkscape::XML::Node *getNodeList(int32_t handle) const;

    /**
     * Take ownership of a list of strings and hand back a DOMStringList handle.
     *
     * A NodeList is live and needs no storage -- it is just the parent node, re-read on each
     * access, as DOM requires. A DOMStringList is a snapshot, so the strings have to live
     * somewhere; they live here, and die with the invocation like everything else.
     */
    int32_t makeStringList(std::vector<std::string> strings);

    /** @return the string list for @a handle, or nullptr if it is unknown or not one. */
    std::vector<std::string> const *getStringList(int32_t handle) const;

    /**
     * Take ownership of a set of nodes and hand back a NodeList handle.
     *
     * The results of a hit test are a snapshot, unlike childNodes which is live: there is no
     * node to re-read them from, and re-running the query on every access would be a
     * different answer each time rather than a live view of one thing.
     */
    int32_t makeNodeSnapshot(std::vector<Inkscape::XML::Node *> nodes);

    /** @return the snapshot for @a handle, or nullptr if it is unknown or not one. */
    std::vector<Inkscape::XML::Node *> const *getNodeSnapshot(int32_t handle) const;

    /** @return the document being edited, for queries that are document-wide. */
    SPDocument *document() const { return _document; }

    /**
     * The desktop and selection, which exist only in a windowed run.
     *
     * The headless effect() overload has neither, so operations that need them report
     * absence rather than inventing a plausible answer -- there genuinely is no current
     * layer when nothing is being looked at.
     */
    void setSession(SPDesktop *desktop, Inkscape::Selection *selection, Effect *effect);

    /**
     * Point the invocation at the backend's cancellation flag.
     *
     * A pointer rather than a copy: the flag is set from the working dialog's Cancel handler
     * while this invocation is still running, so the guest has to see the change.
     */
    void setCancelFlag(bool const *cancelled) { _cancelled = cancelled; }
    bool isCancelled() const { return _cancelled && *_cancelled; }
    SPDesktop *desktop() const { return _desktop; }
    Inkscape::Selection *selection() const { return _selection; }
    Effect *effect() const { return _effect; }

    /**
     * Whether the guest may be given access to [offset, offset + length) of its own memory.
     *
     * Written against the space remaining rather than as offset + length, because that sum
     * overflows. Every read and write goes through here; a host function that indexes the
     * memory without asking is a bug, not an optimisation.
     */
    bool spanIsValid(int32_t offset, int32_t length) const;

    /** Read a UTF-8 string out of the guest's memory. @return false if the span is invalid. */
    bool readString(int32_t offset, int32_t length, std::string &out) const;

    /** The base of the guest's memory. Only valid for spans spanIsValid() has approved. */
    char *memoryBytes() const;

    /**
     * Write a string result into the guest's memory.
     *
     * The return value distinguishes three outcomes that callers must be able to tell apart:
     *
     *   >= 0             bytes written at @a offset
     *   STRING_ABSENT    no such value (@a value was null)
     *   < -1             would not fit; the length needed is (-result - 1), nothing written
     *
     * Collapsing the last two into one code would leave a guest unable to distinguish an
     * attribute that is absent from an attribute whose value was too long for the buffer it
     * offered, and it would silently truncate. Instead the guest sizes its buffer, retries
     * once, and always gets the whole value.
     */
    int32_t writeString(int32_t offset, int32_t capacity, char const *value) const;

    /** Write @a count doubles at @a offset. @return false if the span is invalid. */
    bool writeDoubles(int32_t offset, double const *values, int count) const;

    /** Build a trap carrying @a message, to be returned from a host function. */
    wasm_trap_t *trap(char const *message) const;

private:
    struct Handle
    {
        WasmHandleKind kind;
        void *ptr;
    };

    wasm_store_t *_store;
    SPDocument *_document;
    wasm_memory_t *_memory = nullptr;
    SPDesktop *_desktop = nullptr;
    Inkscape::Selection *_selection = nullptr;
    Effect *_effect = nullptr;
    bool const *_cancelled = nullptr;

    /** Index 0 is reserved so that a zero handle is always null. */
    std::vector<Handle> _handles;

    /** Reverse index, so the same object is handed the same handle; @see makeHandle(). */
    std::map<std::pair<WasmHandleKind, void *>, int32_t> _by_object;

    /**
     * Backing store for DOMStringList handles.
     *
     * A deque rather than a vector because handles point into it: growing a vector would
     * move the elements and leave every outstanding handle dangling.
     */
    std::deque<std::vector<std::string>> _string_lists;

    /** Backing store for hit-test results; a deque for the same reason as above. */
    std::deque<std::vector<Inkscape::XML::Node *>> _node_snapshots;

    /** Nodes created during this invocation, released when it ends; @see own(). */
    std::vector<Inkscape::XML::Node *> _owned;

    void *lookup(int32_t handle, WasmHandleKind kind) const;
};

} // namespace Inkscape::Extension::Implementation

#endif // INKSCAPE_EXTENSION_IMPLEMENTATION_WASM_ABI_H

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
