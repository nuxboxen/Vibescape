// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Pages panel — page management and properties.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_PAGES_PANEL_H
#define INKSCAPE_UI_WIDGET_PAGES_PANEL_H

#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/grid.h>

#include "ui/operation-blocker.h"

class SPDocument;

namespace Inkscape::UI::Widget {

/**
 * Passive panel for page management and properties.
 *
 * update(SPDocument*) reads page state directly from the document.
 * Write callbacks also go straight to the document — no SPDesktop needed.
 */
class PagesPropertiesPanel : public Gtk::Box {
public:
    PagesPropertiesPanel();
    ~PagesPropertiesPanel() override = default;

    void update(SPDocument* document);

private:
    Glib::RefPtr<Gtk::Builder> _builder;
    SPDocument* _document = nullptr;
    OperationBlocker _update;

    Gtk::Button& _add_page_btn;
    Gtk::Button& _delete_page_btn;
    Gtk::Button& _move_page_up_btn;
    Gtk::Button& _move_page_down_btn;
    Gtk::CheckButton& _show_page_border_btn;
    Gtk::CheckButton& _show_page_shadow_btn;
    // Gtk::CheckButton& _enable_page_labels_btn;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_PAGES_PANEL_H
