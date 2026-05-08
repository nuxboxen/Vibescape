// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Display properties panel — display units, rendering settings, and visual preferences.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_DISPLAY_PROPS_PANEL_H
#define INKSCAPE_UI_WIDGET_DISPLAY_PROPS_PANEL_H

#include <gtkmm/box.h>

#include "ui/operation-blocker.h"
#include "ui/widget/page-properties.h"

class SPDocument;
class SPNamedView;

namespace Inkscape {
namespace Colors { class Color; }
namespace Util { class Unit; }
} // namespace Inkscape

namespace Inkscape::UI::Widget {

/**
 * Display properties panel widget.
 *
 * update() reads directly from SPNamedView; it does
 * not require SPDesktop.
 *
 * set_document() must be called whenever the active document changes so that
 * write-back callbacks can reach the document for undo.
 */
class DisplayPropertiesPanel : public Gtk::Box {
public:
    DisplayPropertiesPanel();
    ~DisplayPropertiesPanel() override = default;

    void set_document(SPDocument* document);

    void update(SPNamedView* namedview);

    Gtk::Grid* get_widget() const { return _page->right_grid(); }

private:
    void connect_signals();

    void on_color_changed(Colors::Color const& color, PageProperties::Color element);
    void on_check_toggled(bool checked, PageProperties::Check element);
    void on_unit_changed(Util::Unit const* unit, PageProperties::Units element);

    void update_display_unit_ui(SPNamedView* nv);

    PageProperties* _page = nullptr;
    SPDocument* _document = nullptr;
    OperationBlocker _update;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_DISPLAY_PROPS_PANEL_H
