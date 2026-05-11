// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * File operations (independent of GUI)
 *
 * Copyright (C) 2018, 2019 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include "io/file.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/refptr.h>
#include <giomm/file.h>

#include "document.h"
#include "document-undo.h"
#include "extension/system.h"     // Extension::open()
#include "extension/extension.h"
#include "extension/db.h"
#include "extension/output.h"
#include "extension/input.h"
#include "object/sp-root.h"
#include "xml/repr.h"

/**
 * Create a blank document, remove any template data.
 * Input: Empty string or template file name.
 */
std::unique_ptr<SPDocument> ink_file_new(std::string const &Template)
{
    auto doc = SPDocument::createNewDoc(Template.empty() ? nullptr : Template.c_str(), true);

    if (!doc) {
        std::cerr << "ink_file_new: Did not create new document!" << std::endl;
        return nullptr;
    }

    // Remove all the template info from xml tree
    Inkscape::XML::Node *myRoot = doc->getReprRoot();
    for (auto const name: {"inkscape:templateinfo",
                           "inkscape:_templateinfo"}) // backwards-compatibility
    {
        if (auto node = std::unique_ptr<Inkscape::XML::Node>{sp_repr_lookup_name(myRoot, name)}) {
            Inkscape::DocumentUndo::ScopedInsensitive no_undo(doc.get());
            sp_repr_unparent(node.get());
        }
    }

    return doc;
}

/**
 * Open a document from memory.
 */
std::unique_ptr<SPDocument> ink_file_open(std::span<char const> buffer)
{
    auto doc = SPDocument::createNewDocFromMem(buffer);

    if (!doc) {
        std::cerr << "ink_file_open: cannot open file in memory (pipe?)" << std::endl;
        return nullptr;
    }
    return doc;
}

/**
 * Open a document.
 */
std::pair<std::unique_ptr<SPDocument>, bool> ink_file_open(Glib::RefPtr<Gio::File> const &file)
{
    std::unique_ptr<SPDocument> doc;
    std::string path = file->get_path();

    // TODO: It's useless to catch these exceptions here (and below) unless we do something with them.
    //       If we can't properly handle them (e.g. by showing a user-visible message) don't catch them!
    try {
        doc = Inkscape::Extension::open(nullptr, path.c_str());
    } catch (Inkscape::Extension::Input::no_extension_found const &) {
    } catch (Inkscape::Extension::Input::open_failed const &) {
    } catch (Inkscape::Extension::Input::open_cancelled const &) {
        return {nullptr, true};
    }

    // Try to open explicitly as SVG.
    // TODO: Why is this necessary? Shouldn't this be handled by the first call already?
    if (!doc) {
        try {
            doc = Inkscape::Extension::open(Inkscape::Extension::db.get(SP_MODULE_KEY_INPUT_SVG), path.c_str());
        } catch (Inkscape::Extension::Input::no_extension_found const &) {
        } catch (Inkscape::Extension::Input::open_failed const &) {
        } catch (Inkscape::Extension::Input::open_cancelled const &) {
            return {nullptr, true};
        }
    }

    if (!doc) {
        std::cerr << "ink_file_open: '" << path << "' cannot be opened!" << std::endl;
        return {nullptr, false};
    }

    return {std::move(doc), false};
}

namespace Inkscape::IO {

/**
 * Create a temporary filename, which is closed and deleted when deconstructed.
 */
TempFilename::TempFilename(const std::string &pattern)
{
    try {
        _tempfd = Glib::file_open_tmp(_filename, pattern.c_str());
    } catch (...) {
        /// \todo Popup dialog here
    }
}
  
TempFilename::~TempFilename()
{
    close(_tempfd);
    unlink(_filename.c_str());
}

/**
 * Takes an absolute file path and returns a second file at the same
 * directory location, if and only if the filename exists and is a file.
 *
 * Returns the empty string if the new file is not found.
 */
std::string find_original_file(Glib::StdStringView const filepath, Glib::StdStringView const name)
{
    auto path = Glib::path_get_dirname(filepath);
    auto filename = Glib::build_filename(path, name);
    if (Glib::file_test(filename, Glib::FileTest::IS_REGULAR)) {
        return filename;
    }
    return ""; 
}

Glib::RefPtr<Gio::FileInfo> query_file_info_async(Glib::RefPtr<Gio::File> file, std::string const &attributes, size_t timeout_ms) {
    auto cancellable = Gio::Cancellable::create();
    Glib::RefPtr<Gio::FileInfo> file_info = nullptr;
    file->query_info_async([file, &file_info](Glib::RefPtr<Gio::AsyncResult> const &result) {
        try {
            file_info = file->query_info_finish(result);
        } catch (Glib::Error &ex) {
            if (ex.code() == Gio::Error::CANCELLED) {
                std::cerr << "IO::query_file_info_async: Async query cancelled for file \""
                          << file->get_uri() << "\": Timed out" << std::endl;
            } else {
                std::cerr << "IO::query_file_info_async: " << ex.what() << std::endl;
            }
        }
    }, cancellable, attributes);
    if (timeout_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
    while (Glib::MainContext::get_default()->iteration(false));
    cancellable->cancel();
    return file_info;
}

} // namespace Inkscape::IO

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
