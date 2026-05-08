// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Widget for specifying a document's license; part of document
 * preferences dialog.
 */
/*
 * Authors:
 *   bulia byak <buliabyak@users.sf.net>
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon Phillips <jon@rejon.org>
 *   Ralf Stephan <ralf@ark.in-berlin.de> (Gtkmm)
 *   Abhishek Sharma
 *   Mike Kowalski
 *
 * Copyright (C) 2000 - 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_LICENSOR_H
#define INKSCAPE_UI_WIDGET_LICENSOR_H

#include <gtkmm/box.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>

#include "ui/operation-blocker.h"

class SPDocument;
struct rdf_license_t;

namespace Inkscape::UI::Widget {

class DropDownList;

/**
 * Widget for specifying a document's license; part of document
 * preferences dialog.
 */
class Licensor final : public Gtk::Box {
public:
    Licensor();
    ~Licensor() final;

    void init();
    void set_document(SPDocument* document);
    void update(SPDocument* doc);

private:
    void on_license_changed();
    void on_uri_changed();
    const rdf_license_t* get_selected_license() const;
    void set_uri_text(const char* text);

    Gtk::Label* _uri_label = nullptr;
    Gtk::Entry* _uri_entry = nullptr;
    Gtk::DropDown* _license_dropdown = nullptr;
    std::vector<const rdf_license_t*> _licenses;
    OperationBlocker _update;
    SPDocument* _document = nullptr;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_LICENSOR_H

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
