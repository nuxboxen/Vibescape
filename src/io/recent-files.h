// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Access Inkscape's recent files
 *
 * Copyright 2025 Martin Owens <doctormo@geek-2.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm/refptr.h>
#include <optional>
#include <vector>
#include <span>
#include <string>

#include <giomm/file.h>
#include <giomm/menu.h>
#include <gtkmm/recentmanager.h>
#include <gtkmm/recentinfo.h>

namespace Inkscape::IO {

extern Glib::RefPtr<Gio::Menu> recent_files_menu;
extern std::vector<Glib::RefPtr<Gtk::RecentInfo>> recent_files_list;

bool reset_recent_files_list();
std::vector<Glib::RefPtr<Gtk::RecentInfo>> get_recent_files_list(size_t max_files = 0, bool is_autosave = false, bool is_startup = false);
bool build_recent_files_menu(Glib::RefPtr<Gio::Menu> const recent_menu, std::span<Glib::RefPtr<Gtk::RecentInfo>> const recent_files);
bool add_or_update_recent_file(std::string const &uri, std::optional<std::string> const &name = {}, std::vector<Glib::ustring> const &group = {}, std::optional<std::string> const &original = {});
bool remove_recent_file(std::string const &uri);
std::optional<std::pair<std::string, Glib::RefPtr<Gtk::RecentInfo>>> get_recent_file_group_and_original_info(std::string const &uri);
Glib::RefPtr<Gtk::RecentInfo> get_recent_file(std::string const &uri);
std::vector<std::string> get_recent_file_paths(std::span<Glib::RefPtr<Gtk::RecentInfo>> const recent_files);
std::vector<std::string> shorten_recent_file_paths(std::span<std::string> const paths);

}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
