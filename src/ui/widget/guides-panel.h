// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Guides panel — show/lock guides and guide colours.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_GUIDES_PANEL_H
#define INKSCAPE_UI_WIDGET_GUIDES_PANEL_H

#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/grid.h>
#include <gtkmm/switch.h>

#include "ui/operation-blocker.h"

class SPNamedView;

namespace Inkscape::UI::Widget {

class ColorPicker;

/**
 * Passive panel for guide visibility, locking, and colour settings.
 *
 * update(SPNamedView*) reads guide state directly from the namedview.
 * Write callbacks also go straight to the namedview — no SPDesktop needed.
 */
class GuidesPanel : public Gtk::Box {
public:
    GuidesPanel();
    ~GuidesPanel() override = default;

    void update(SPNamedView* namedview);

private:
    Glib::RefPtr<Gtk::Builder> _builder;
    SPNamedView* _namedview = nullptr;
    OperationBlocker _update;

    Gtk::Switch& _show_guides;
    Gtk::CheckButton& _lock_guides;
    Gtk::Button& _create_guides_btn;
    Gtk::Button& _delete_guides_btn;
    ColorPicker& _guide_color;
    ColorPicker& _guide_highlight;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_GUIDES_PANEL_H
