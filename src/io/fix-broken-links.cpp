// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * tracks external resources such as image and css files.
 *
 * Copyright 2011  Jon A. Cruz  <jon@joncruz.org>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "fix-broken-links.h"

#include <giomm/file.h>
#include <glibmm/convert.h>
#include <glibmm/fileutils.h>
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <glibmm/uriutils.h>
#include <gtkmm/recentmanager.h>

#include "document-undo.h"
#include "document.h"
#include "io/path.h"
#include "io/recent-files.h"
#include "object/sp-object.h"
#include "ui/icon-names.h"
#include "xml/href-attribute-helper.h"
#include "xml/node.h"

namespace Inkscape::IO {

bool fixBrokenLinks(SPDocument *doc);
    

/**
 * Walk all links in a document and create a listing of unique broken links.
 *
 * @return a list of all broken links.
 */
static std::vector<Glib::ustring> findBrokenLinks(SPDocument *doc);

/**
 * Resolve broken links as a whole and return a map for those that can be found.
 *
 * Note: this will allow for future enhancements including relinking to new locations
 * with the most broken files found, etc.
 *
 * @return a map of found links.
 */
static std::map<Glib::ustring, Glib::ustring> locateLinks(Glib::ustring const & docbase, std::vector<Glib::ustring> const & brokenLinks, std::span<Glib::RefPtr<Gtk::RecentInfo>> recent_files);


/**
 * Try to parse href into a local filename using standard methods.
 *
 * @return true if successful.
 */
static bool extractFilepath(Glib::ustring const &href, std::string &filename);

/**
 * Try to parse href into a local filename using some non-standard methods.
 * This means the href is likely invalid and should be rewritten.
 *
 * @return true if successful.
 */
static bool reconstructFilepath(Glib::ustring const &href, std::string &filename);




static bool extractFilepath(Glib::ustring const &href, std::string &filename)
{                    
    bool isFile = false;

    filename.clear();

    auto scheme = Glib::uri_parse_scheme(href.raw());
    if ( !scheme.empty() ) {
        // TODO debug g_message("Scheme is now [%s]", scheme.c_str());
        if ( scheme == "file" ) {
            // TODO debug g_message("--- is a file URI                 [%s]", href.c_str());

            // throws Glib::ConvertError:
            try {
                filename = Glib::filename_from_uri(href);
                isFile = true;
            } catch(Glib::ConvertError e) {
                g_warning("%s", e.what());
            }
        }
    } else {
        // No scheme. Assuming it is a file path (absolute or relative).
        // throws Glib::ConvertError:
        filename = Glib::filename_from_utf8(href);
        isFile = true;
    }

    return isFile;
}

static bool reconstructFilepath(Glib::ustring const &href, std::string &filename)
{                    
    bool isFile = false;

    filename.clear();

    auto scheme = Glib::uri_parse_scheme(href.raw());
    if ( !scheme.empty() ) {
        if ( scheme == "file" ) {
            // try to build a relative filename for URIs like "file:image.png"
            // they're not standard conformant but not uncommon
            Glib::ustring href_new = Glib::ustring(href, 5);
            filename = Glib::filename_from_utf8(href_new);
            isFile = true;
        }
    }
    return isFile;
}


static std::vector<Glib::ustring> findBrokenLinks( SPDocument *doc )
{
    std::vector<Glib::ustring> result;
    std::set<Glib::ustring> uniques;

    if ( doc ) {
        std::vector<SPObject *> images = doc->getResourceList("image");
        for (auto image : images) {
            Inkscape::XML::Node *ir = image->getRepr();

            gchar const *href = Inkscape::getHrefAttribute(*ir).second;
            if ( href &&  ( uniques.find(href) == uniques.end() ) ) {
                std::string filename;
                if (extractFilepath(href, filename)) {
                    if (Glib::path_is_absolute(filename)) {
                        if (!Glib::file_test(filename, Glib::FileTest::EXISTS)) {
                            result.emplace_back(href);
                            uniques.insert(href);
                        }
                    } else {
                        std::string combined = Glib::build_filename(doc->getDocumentBase(), filename);
                        if ( !Glib::file_test(combined, Glib::FileTest::EXISTS) ) {
                            result.emplace_back(href);
                            uniques.insert(href);
                        }
                    }
                } else if (reconstructFilepath(href, filename)) {
                    result.emplace_back(href);
                    uniques.insert(href);
                }
            }
        }        
    }

    return result;
}

/* Given a base path and an assumed subpath, returns the longest
 * combination of their parts that exists on the file system.
 *
 * Example:
 *
 * Assume /a/d exists on the file system
 *
 * Function arguments:
 *   base = /a/b
 *   subpath = c/d
 *
 * Try /a/b/c/d => doesn't exist
 * Try /a/b/d   => doesn't exist
 * Try /a/c/d   => doesn't exist
 * Try /a/d     => exists, return this path
 *
 * If none of the combinations exist, return an empty string
 */
std::string search_upwards_and_concat_paths(std::string_view base, std::string_view subpath)
{
    Inkscape::IO::PathParts parts = Inkscape::IO::split_path(subpath);

    if (parts.empty()) {
        return std::string{};
    }

    Inkscape::IO::PathParts base_parts = Inkscape::IO::split_path(base);

    for (auto i = base_parts.size(); i > 0; i--) {
        base_parts.data.resize(base_parts.size() + parts.size());
        for (auto j = 0; j < parts.size(); j++) {
            std::copy(parts.data.begin() + j, parts.data.end(), base_parts.data.begin() + i);
            std::string filepath = base_parts.join();
            if (Gio::File::create_for_path(filepath)->query_exists()) {
                return filepath;
            };
            base_parts.data.pop_back();
        }
        base_parts.data.pop_back();
    }

    return std::string{};
}

static std::map<Glib::ustring, Glib::ustring> locateLinks(Glib::ustring const & docbase, std::vector<Glib::ustring> const & brokenLinks, std::span<Glib::RefPtr<Gtk::RecentInfo>> const recent_files)
{
    std::map<Glib::ustring, Glib::ustring> result;


    // Note: we use a vector because we want them to stay in order:
    std::vector<std::string> priorLocations;

    for (auto recentItem : recent_files) {
        Glib::ustring uri = recentItem->get_uri();
        auto scheme = Glib::uri_parse_scheme(uri.raw());
        if ( scheme == "file" ) {
            try {
                std::string path = Glib::filename_from_uri(uri);
                path = Glib::path_get_dirname(path);
                if ( std::find(priorLocations.begin(), priorLocations.end(), path) == priorLocations.end() ) {
                    // TODO debug g_message("               ==>[%s]", path.c_str());
                    priorLocations.push_back(path);
                }
            } catch (Glib::ConvertError e) {
                g_warning("%s", e.what());
            }
        }
    }

    // At the moment we expect this list to contain file:// references, or simple relative or absolute paths.
    for (const auto & brokenLink : brokenLinks) {
        // TODO debug g_message("========{%s}", it->c_str());

        std::string filename;
        if (extractFilepath(brokenLink, filename) || reconstructFilepath(brokenLink, filename)) {
            auto const docbase_native = Glib::filename_from_utf8(docbase);

            // We were able to get some path. Check it
            std::string origPath = filename;

            if (!Glib::path_is_absolute(filename)) {
                filename = Glib::build_filename(docbase_native, filename);
            }

            bool exists = Glib::file_test(filename, Glib::FileTest::EXISTS);

            // search in parent folders
            if (!exists) {
                filename = search_upwards_and_concat_paths(docbase_native, origPath);
                exists = !filename.empty();
            }

            // Check if the MRU bases point us to it.
            if ( !exists ) {
                if ( !Glib::path_is_absolute(origPath) ) {
                    for ( std::vector<std::string>::iterator it = priorLocations.begin(); !exists && (it != priorLocations.end()); ++it ) {
                        filename = search_upwards_and_concat_paths(*it, origPath);
                        exists = !filename.empty();
                    }
                }
            }

            if ( exists ) {
                if (Glib::path_is_absolute(filename)) {
                    filename = Inkscape::IO::optimize_path(docbase_native, filename).first;
                }

                bool isAbsolute = Glib::path_is_absolute(filename);
                Glib::ustring replacement =
                    isAbsolute ? Glib::filename_to_uri(filename) : Glib::filename_to_utf8(filename);
                result[brokenLink] = replacement;
            }
        }
    }

    return result;
}

bool fixBrokenLinks(SPDocument *doc, std::span<Glib::RefPtr<Gtk::RecentInfo>> recent_files)
{
    bool changed = false;
    if ( doc ) {
        // TODO debug g_message("FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP");
        // TODO debug g_message("      base is [%s]", doc->getDocumentBase());

        std::vector<Glib::ustring> brokenHrefs = findBrokenLinks(doc);
        if ( !brokenHrefs.empty() ) {
            // TODO debug g_message("    FOUND SOME LINKS %d", static_cast<int>(brokenHrefs.size()));
            for ( std::vector<Glib::ustring>::iterator it = brokenHrefs.begin(); it != brokenHrefs.end(); ++it ) {
                // TODO debug g_message("        [%s]", it->c_str());
            }
        }

        Glib::ustring base;
        if (doc->getDocumentBase()) {
            base = doc->getDocumentBase();
        }

        std::map<Glib::ustring, Glib::ustring> mapping = locateLinks(base, brokenHrefs, recent_files);
        for ( std::map<Glib::ustring, Glib::ustring>::iterator it = mapping.begin(); it != mapping.end(); ++it )
        {
            // TODO debug g_message("     [%s] ==> {%s}", it->first.c_str(), it->second.c_str());
        }

        DocumentUndo::ScopedInsensitive _no_undo(doc);
        
        std::vector<SPObject *> images = doc->getResourceList("image");
        for (auto image : images) {
            Inkscape::XML::Node *ir = image->getRepr();

            auto [href_key, href] = Inkscape::getHrefAttribute(*ir);
            if ( href ) {
                // TODO debug g_message("                  consider [%s]", href);
                
                if ( mapping.find(href) != mapping.end() ) {
                    // TODO debug g_message("                     Found a replacement");

                    ir->setAttributeOrRemoveIfEmpty(href_key, mapping[href]);
                    if ( ir->attribute( "sodipodi:absref" ) ) {
                        ir->removeAttribute("sodipodi:absref"); // Remove this attribute
                    }

                    SPObject *updated = doc->getObjectByRepr(ir);
                    if (updated) {
                        // force immediate update of dependent attributes
                        updated->updateRepr();
                    }

                    changed = true;
                }
            }
        }
        if ( changed ) {
            DocumentUndo::done( doc, RC_("Undo", "Fixup broken links"), INKSCAPE_ICON("dialog-xml-editor"));
        }
    }

    return changed;
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
