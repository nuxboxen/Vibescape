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

#ifndef INKSCAPE_EXTENSION_IMPLEMENTATION_WASM_BACKEND_H
#define INKSCAPE_EXTENSION_IMPLEMENTATION_WASM_BACKEND_H

#include <string>
#include <vector>

#include "implementation.h"

class SPDesktop;
class SPDocument;

struct wasm_engine_t;
struct wasm_module_t;
struct wasm_store_t;

namespace Inkscape::Extension::Implementation {

/**
 * Runs an extension supplied as a WebAssembly module.
 *
 * Where Script serialises the document, spawns an interpreter and parses the result back,
 * this runs the extension in-process against the live document. The module is called through
 * the published plugin API, whose host half lives in wasm-abi.h; nothing engine-specific
 * appears in either, only the community C API from <wasm.h>, so any engine implementing that
 * header can be substituted at build time.
 *
 * The module named by the .inx is loaded and compiled once, at load(); each invocation gets a
 * fresh store, instance and handle table, so nothing survives from one run to the next.
 */
class WasmBackend : public Implementation
{
public:
    WasmBackend();
    ~WasmBackend() override;

    bool load(Inkscape::Extension::Extension *module) override;
    void unload(Inkscape::Extension::Extension *module) override;
    bool check(Inkscape::Extension::Extension *module) override;

    void effect(Inkscape::Extension::Effect *module, ExecutionEnv *executionEnv, SPDesktop *desktop,
                ImplementationDocumentCache *docCache) override;
    void effect(Inkscape::Extension::Effect *module, ExecutionEnv *executionEnv, SPDocument *document) override;

    /**
     * Ask the running module to stop.
     *
     * Cooperative, because it has to be: a WebAssembly call cannot be interrupted from
     * outside, and abandoning one mid-flight would leave the handle table and the document
     * transaction in an unknown state. This raises a flag that the guest observes by calling
     * inkIsCancelled(), the same arrangement Photoshop's TestAbort has used for decades.
     *
     * A module that never asks cannot be stopped. That is a property of running a plugin
     * in-process rather than of this backend -- a C plugin looping forever behind dlopen is
     * no more killable -- and it is why Script, which spawns a process it can signal, is the
     * one thing this design gives up.
     */
    bool cancelProcessing() override;

private:
    /**
     * Instantiate the module and run its entry point against @a document.
     *
     * Both effect() overloads land here: the desktop one because everything it adds is
     * presentation, the document one because that is all a headless run has. A fresh
     * instance and handle table are built per call and torn down on return, so nothing a
     * module did last time can be reached this time. The store outlives them, because the
     * compiled module belongs to it.
     */
    void _run(Inkscape::Extension::Effect *module, ExecutionEnv *executionEnv, SPDocument *document,
              SPDesktop *desktop);

    /** Undo the invocation and report @a message to the user, if there is one to report to. */
    void _reportFailure(ExecutionEnv *executionEnv, SPDesktop *desktop, Glib::ustring const &message);

    /** Bytes of the .wasm file named by the .inx, read once at load(). */
    std::vector<char> _module_bytes;

    /**
     * Name of the module's exported entry point, taken from the .inx.
     *
     * The .inx names it because <wasm.h> offers no lookup-export-by-name, and because a
     * guest language is free to decorate the symbol however it likes -- a Java plugin
     * exports a canonical name such as "org.inkscape.Plugin.effect(I)I". Naming it here
     * keeps the host free of any assumption about the language that produced the module.
     */
    std::string _entry;

    /** Raised by cancelProcessing(), observed by the guest through inkIsCancelled(). */
    bool _cancelled = false;

    wasm_engine_t *_engine = nullptr;
    wasm_module_t *_compiled = nullptr;
    wasm_store_t *_store = nullptr;
};

} // namespace Inkscape::Extension::Implementation

#endif // INKSCAPE_EXTENSION_IMPLEMENTATION_WASM_BACKEND_H

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
