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

#include "wasm-abi.h"

#include <cstring>

#include "document.h"
#include "gc-anchored.h"
#include "object/sp-item.h"
#include "util/cast.h"
#include "xml/node.h"

namespace Inkscape::Extension::Implementation {

WasmInvocation::WasmInvocation(wasm_store_t *store, SPDocument *document)
    : _store(store)
    , _document(document)
{
    // Index 0 is never handed out, so a handle of 0 always means null.
    _handles.push_back({WasmHandleKind::None, nullptr});
}

WasmInvocation::~WasmInvocation()
{
    // Drop the one reference each created node was born with. A node the guest parented is
    // kept alive by its parent from here on; one it abandoned becomes collectable, which is
    // the right outcome for a node nothing in the document points at.
    for (auto *node : _owned) {
        Inkscape::GC::release(node);
    }
}

void WasmInvocation::own(Inkscape::XML::Node *node)
{
    if (node) {
        _owned.push_back(node);
    }
}

int32_t WasmInvocation::makeHandle(WasmHandleKind kind, void *ptr)
{
    if (!ptr) {
        return 0;
    }
    auto const key = std::make_pair(kind, ptr);
    if (auto const known = _by_object.find(key); known != _by_object.end()) {
        return known->second;
    }

    _handles.push_back({kind, ptr});
    auto const handle = static_cast<int32_t>(_handles.size() - 1);
    _by_object.emplace(key, handle);
    return handle;
}

void *WasmInvocation::lookup(int32_t handle, WasmHandleKind kind) const
{
    if (handle <= 0 || static_cast<size_t>(handle) >= _handles.size()) {
        return nullptr;
    }
    auto const &entry = _handles[static_cast<size_t>(handle)];
    return entry.kind == kind ? entry.ptr : nullptr;
}

Inkscape::XML::Document *WasmInvocation::getDocument(int32_t handle) const
{
    return static_cast<Inkscape::XML::Document *>(lookup(handle, WasmHandleKind::Document));
}

Inkscape::XML::Node *WasmInvocation::getNode(int32_t handle) const
{
    return static_cast<Inkscape::XML::Node *>(lookup(handle, WasmHandleKind::Node));
}

SPItem *WasmInvocation::itemFor(Inkscape::XML::Node *node) const
{
    if (!node || !_document) {
        return nullptr;
    }
    // The object tree is rebuilt from the repr by observers, which run later; a plugin that
    // creates a node and measures it in the same breath would otherwise read nothing.
    _document->ensureUpToDate();
    return cast<SPItem>(_document->getObjectByRepr(node));
}

int32_t WasmInvocation::makeNodeSnapshot(std::vector<Inkscape::XML::Node *> nodes)
{
    _node_snapshots.push_back(std::move(nodes));
    return makeHandle(WasmHandleKind::NodeSnapshot, &_node_snapshots.back());
}

std::vector<Inkscape::XML::Node *> const *WasmInvocation::getNodeSnapshot(int32_t handle) const
{
    return static_cast<std::vector<Inkscape::XML::Node *> const *>(lookup(handle, WasmHandleKind::NodeSnapshot));
}

Inkscape::XML::Node *WasmInvocation::getNodeList(int32_t handle) const
{
    return static_cast<Inkscape::XML::Node *>(lookup(handle, WasmHandleKind::NodeList));
}

int32_t WasmInvocation::makeStringList(std::vector<std::string> strings)
{
    _string_lists.push_back(std::move(strings));
    return makeHandle(WasmHandleKind::StringList, &_string_lists.back());
}

std::vector<std::string> const *WasmInvocation::getStringList(int32_t handle) const
{
    return static_cast<std::vector<std::string> const *>(lookup(handle, WasmHandleKind::StringList));
}

bool WasmInvocation::spanIsValid(int32_t offset, int32_t length) const
{
    if (!_memory || offset < 0 || length < 0) {
        return false;
    }
    // Re-read the size every time: the guest can grow its memory, which moves the base.
    size_t const size = wasm_memory_data_size(_memory);
    return static_cast<size_t>(offset) <= size && static_cast<size_t>(length) <= size - static_cast<size_t>(offset);
}

void WasmInvocation::setSession(SPDesktop *desktop, Inkscape::Selection *selection, Effect *effect)
{
    _desktop = desktop;
    _selection = selection;
    _effect = effect;
}

char *WasmInvocation::memoryBytes() const
{
    return _memory ? wasm_memory_data(_memory) : nullptr;
}

bool WasmInvocation::readString(int32_t offset, int32_t length, std::string &out) const
{
    if (!spanIsValid(offset, length)) {
        return false;
    }
    out.assign(wasm_memory_data(_memory) + offset, static_cast<size_t>(length));
    return true;
}

int32_t WasmInvocation::writeString(int32_t offset, int32_t capacity, char const *value) const
{
    if (!value) {
        return STRING_ABSENT;
    }

    auto const length = static_cast<int32_t>(strlen(value));
    if (length > capacity || !spanIsValid(offset, length)) {
        return -length - 1; // "needs `length` bytes"; nothing written
    }

    memcpy(wasm_memory_data(_memory) + offset, value, static_cast<size_t>(length));
    return length;
}

bool WasmInvocation::writeDoubles(int32_t offset, double const *values, int count) const
{
    auto const bytes = static_cast<int32_t>(count * sizeof(double));
    if (!spanIsValid(offset, bytes)) {
        return false;
    }
    // memcpy rather than a cast: the guest chooses the offset and linear memory is untyped,
    // so the destination need not be suitably aligned for a double.
    memcpy(wasm_memory_data(_memory) + offset, values, static_cast<size_t>(bytes));
    return true;
}

wasm_trap_t *WasmInvocation::trap(char const *message) const
{
    wasm_message_t text;
    wasm_name_new_from_string_nt(&text, message);
    wasm_trap_t *result = wasm_trap_new(_store, &text);
    wasm_byte_vec_delete(&text);
    return result;
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
