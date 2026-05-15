// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Manages external resources such as image and css files.
 *
 * Copyright 2011  Jon A. Cruz  <jon@joncruz.org>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <span>
#include <string>
#include <giomm/file.h>
#include <gtkmm/recentinfo.h>

class SPDocument;

namespace Inkscape::IO {

std::string search_upwards_and_concat_paths(std::string_view base, std::string_view subpath);
bool fixBrokenLinks(SPDocument *doc, std::span<Glib::RefPtr<Gtk::RecentInfo>> recent_files);

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
