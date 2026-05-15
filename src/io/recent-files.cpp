// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Create a list of recentyly used files.
 *
 * Copyright 2025 Martin Owens <doctormo@geek-2.com>
 * Copyright 2024, 2025 Tavmjong Bah <tavmjong@free.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "recent-files.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <ranges>

#include <boost/algorithm/string/replace.hpp>
#include <giomm/cancellable.h>
#include <giomm/file.h>
#include <giomm/fileinfo.h>
#include <giomm/menu.h>
#include <glibmm/convert.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/recentinfo.h>

#include "io/path.h"
#include "preferences.h"

namespace Inkscape::IO {

static Glib::ustring const recent_app_name = "org.inkscape.Inkscape";

Glib::RefPtr<Gio::Menu> recent_files_menu;
std::vector<Glib::RefPtr<Gtk::RecentInfo>> recent_files_list;

bool check_recent_info(Glib::RefPtr<Gtk::RecentInfo> const &recent_info, bool is_autosave)
{
    return recent_info->get_mime_type() == "image/svg+xml" and is_autosave == recent_info->has_group("Auto") and
     (recent_info->has_application(g_get_prgname()) or recent_info->has_application(recent_app_name) or
      recent_info->has_application("inkscape") or recent_info->has_application("inkscape.exe"));
}

/**
 * Generate a vector of recently used Inkscape files.
 *
 * @arg max_files - Limits the output to this number of files, zero means no-maximum.
 * @arg is_autosave - Limit the list to just auto save files.
 * @arg for_startup - Indicates that the function is run to populate the startup menu.
 *
 * @returns a vector of pointers to recent info structs.
 */
std::vector<Glib::RefPtr<Gtk::RecentInfo>> get_recent_files_list(size_t max_files, bool is_autosave, bool for_startup)
{
    auto recent_manager = Gtk::RecentManager::get_default();

    if (!recent_manager) {
        std::cerr << "IO::get_recent_files_list: Failed to get default RecentManager" << std::endl;
        return {};
    }

    // All recent files, not necessarily inkscape only (std::vector)
    auto recent_files = recent_manager->get_items();
    std::vector<Glib::RefPtr<Gtk::RecentInfo>> selected_files;

    auto cancellable = Gio::Cancellable::create();

    auto it = recent_files | std::ranges::views::filter([is_autosave](auto info){
        return check_recent_info(info, is_autosave);
    });

    // create an async exists query for each file in the filtered iterator
    for (auto recent_info : it) {
        auto const file = Gio::File::create_for_uri(recent_info->get_uri());
        file->query_info_async([file, recent_info, &selected_files](Glib::RefPtr<Gio::AsyncResult> const &result) {
            try {
                Glib::RefPtr<Gio::FileInfo> info = file->query_info_finish(result);
                if (info and info->get_file_type() == Gio::FileType::REGULAR) {
                    selected_files.push_back(recent_info);
                }
            } catch (Glib::Error &ex) {
                if (ex.code() == Gio::Error::CANCELLED) {
                    std::cerr << "IO::get_recent_files_list: Async query cancelled for file \""
                              << file->get_uri() << "\": Timed out" << std::endl;
                } else {
                    std::cerr << "IO::get_recent_files_list: " << ex.what() << std::endl;
                }
            }
        }, cancellable, "standard::type");
    }

    // Wait for async queries
    auto prefs = Preferences::get();
    size_t timeout_ms = for_startup ? prefs->getUInt("/options/recentfiles/query_timeout_ms_startup")
                                    : prefs->getUInt("/options/recentfiles/query_timeout_ms_background");

    if (timeout_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }

    while (Glib::MainContext::get_default()->iteration(false));
    cancellable->cancel();

    // Sort by "last modified" time, which puts the most recently opened files first.
    std::sort(std::begin(selected_files), std::end(selected_files), [](auto const &a, auto const &b) -> bool {
        // a should precede b if a->get_modified() is later than b->get_modified()
        return a->get_modified().compare(b->get_modified()) > 0;
    });

    // Truncate to user-specified max_files.
    if (max_files && selected_files.size() > max_files) {
        selected_files.resize(max_files);
    }

    return selected_files;
}

bool build_recent_files_menu(Glib::RefPtr<Gio::Menu> const recent_menu,
                             std::span<Glib::RefPtr<Gtk::RecentInfo>> const recent_files)
{
    if (!recent_menu) {
        g_warning("IO::build_recent_files menu: No recent recent_menu in menus.ui found.");
        return false;
    }

    recent_menu->remove_all();

    if (recent_files.empty()) { // Create a placeholder with a non-existent action
        auto nothing2c = Gio::MenuItem::create(_("No items found"), "app.nop");
        recent_menu->append_item(nothing2c);
        return false;
    }

    auto max_files = Inkscape::Preferences::get()->getUInt("/options/maxrecentdocuments/value");
    auto first_n = max_files < recent_files.size() ? recent_files.first(max_files) : recent_files;

    auto recent_paths = get_recent_file_paths(first_n);
    auto shortened_paths = shorten_recent_file_paths(recent_paths);

    for (auto i = 0; i < first_n.size(); i++) {
        // Escape underscores to prevent them from being interpreted as accelerator mnemonics
        boost::algorithm::replace_all(shortened_paths[i], "_", "__");
        auto item = Gio::MenuItem::create(std::move(shortened_paths[i]), "");
        auto target = Glib::Variant<Glib::ustring>::create(recent_paths[i]);
        // note: setting action and target separately rather than using convenience menu method append
        // since some filename characters can result in invalid "direct action" string
        item->set_action_and_target(Glib::ustring("app.file-open-window"), target);
        recent_menu->append_item(item);
    }

    return true;
}

/**
 * Add a recent file to the Gtk RecentFiles manager for an SVG file.
 *
 * @arg filename - An absolute local filename of the document in question
 * @arg name     - The name of the document
 * @arg groups   - Optional groups, used for AutoSave and Crash
 * @arg original - The filename to the original document, where available. If used this save is marked as private.
 */
bool add_or_update_recent_file(std::string const &uri, std::optional<std::string> const &name,
                               std::vector<Glib::ustring> const &groups, std::optional<std::string> const &original_uri)
{
    if (auto recentmanager = Gtk::RecentManager::get_default()) {
        auto has_original = original_uri.has_value();
        auto item_name = name.has_value() ? name.value() : Glib::path_get_basename(uri);
        auto description = has_original ? original_uri.value() : std::string{};
        bool success = recentmanager->add_item(uri, {
            item_name,       // Name
            description,     // Description used for original file uri
            "image/svg+xml", // Mime type
            recent_app_name, // App name
            {},              // Execute
            groups,          // Groups
            has_original,    // Private if points to another document
        });
        if (!success) {
            std::cerr << "IO::add_recent_file: Failed to add uri \"" << uri << "\"" << std::endl;
        }
        return success;
    } else {
        std::cerr << "IO::add_recent_file: Failed to get default RecentManager: uri \"" << uri << "\"" << std::endl;
    }
    return false;
}

/**
 * Remove a recent file entry, call when deleting files.
 */
bool remove_recent_file(std::string const &uri)
{
    if (auto recentmanager = Gtk::RecentManager::get_default()) {
        try {
            bool success = recentmanager->remove_item(uri);
            if (!success) {
                std::cerr << "remove_recent_file: Failed to remove uri \"" << uri << "\"" << std::endl;
            }
            return success;
        } catch (Glib::Error const &ex) { // lookup failed
            std::cerr << ex.what() << std::endl;
        }
    } else {
        std::cerr << "IO::remove_recent_file: Failed to get default RecentManager: uri \"" << uri << "\"" << std::endl;
    }
    return false;
}

/**
 * Remove inkscape recent items, but preserve items opened by other programs
 * auto any auto-saves which are considered not user accessable.
 */
bool reset_recent_files_list()
{
    if (auto recentmanager = Gtk::RecentManager::get_default()) {
        for (auto info : recentmanager->get_items()) {
            bool is_ink, is_other = false;
            for (auto &app : info->get_applications()) {
                if ( app == g_get_prgname()
                  || app == recent_app_name
                  || app == "inkscape"
                  || app == "inkscape.exe") {
                    is_ink = true;
                } else {
                    is_other = true;
                }
            }
            if (is_ink && !is_other && !info->has_group("Auto")) {
                recentmanager->remove_item(info->get_uri());
            }
        }
        // clear the recent files list and menu
        recent_files_list.clear();
        recent_files_menu->remove_all();
        auto nothing2c = Gio::MenuItem::create(_("No items found"), "app.nop");
        recent_files_menu->append_item(nothing2c);
    } else {
        std::cerr << "IO::reset_recent_files_list: Failed to get default RecentManager" << std::endl;
    }
    return false;
}

/**
 * Get the file recent info for the given path, if there is one.
 */
Glib::RefPtr<Gtk::RecentInfo> get_recent_file(std::string const &uri)
{
    if (auto recentmanager = Gtk::RecentManager::get_default()) {
        try {
            return recentmanager->lookup_item(uri);
        } catch (Glib::Error const &ex) { // lookup failed
            std::cerr << "IO::get_recent_file: " << ex.what() << std::endl;
        }
    } else {
        std::cerr << "IO::get_recent_file: Failed to get default RecentManager" << std::endl;
    }
    return {};
}

/**
 * Get the original filename for the given file, and remove the recent files entry if it's a crash.
 *
 * @arg filename - The auto save or crash file we are opening.
 *
 * @returns - False optional if this isn't an auto save or crash, an empty string if is is
 *            but doesn't have an original filename because it was unsaved. Otherwise the
 *            original filename is provided.
 */
std::optional<std::pair<std::string, Glib::RefPtr<Gtk::RecentInfo>>> get_recent_file_group_and_original_info(std::string const &uri)
{
    if (auto info = get_recent_file(uri)) {
        if (info->has_group("Auto")) {
            // Original filename stored in description, see add_recent_file above.
            return std::make_pair("Auto", get_recent_file(info->get_description()));
        }
        if (info->has_group("Crash")) {
            return std::make_pair("Crash", get_recent_file(info->get_description()));
        }
    } else {
        std::cerr << "IO::get_recent_file_group_and_original_info: Unable to retrieve info for uri \"" << uri << "\"" << std::endl;
    }
    return std::nullopt;
}

std::vector<std::string> get_recent_file_paths(std::span<Glib::RefPtr<Gtk::RecentInfo>> const recent_files)
{
    std::vector<std::string> paths;
    paths.reserve(recent_files.size());

    for (auto recent_file : recent_files) {
        paths.emplace_back(recent_file->get_uri_display());
    }

    return paths;
}

std::vector<std::string> shorten_recent_file_paths(std::span<std::string> const paths)
{
    auto const prefs = Preferences::get();
    auto const sep = prefs->getString("/options/recentfiles/shortened_path_separator", "  🞂  ");
    std::vector<std::string_view> views;
    views.reserve(paths.size());

    for (auto const &path : paths) {
        views.emplace_back(path);
    }

    return Inkscape::IO::shorten_paths(views, sep.raw());
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
