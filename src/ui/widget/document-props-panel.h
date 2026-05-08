// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Document properties panel — page size, viewbox, coordinate system, and render settings.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_DOCUMENT_PROPS_PANEL_H
#define INKSCAPE_UI_WIDGET_DOCUMENT_PROPS_PANEL_H

#include <gtkmm/box.h>

#include "desktop.h"
#include "ui/operation-blocker.h"
#include "ui/widget/page-properties.h"

class SPDesktop;
class SPDocument;
class SPNamedView;
class SPRoot;

namespace Inkscape {
namespace Colors { class Color; }
namespace Util { class Unit; }
class PageManager;
} // namespace Inkscape

namespace Inkscape::UI::Widget {

/**
 * Document properties panel widget.
 *
 * update() reads directly from SPNamedView, SPRoot and SPPageManager; it does
 * not require SPDesktop.
 *
 * set_document() must be called whenever the active document changes so that
 * write-back callbacks (page resize, colour changes, etc.) can reach the
 * document for undo.
 */
class DocumentPropertiesPanel : public Gtk::Box {
public:
    DocumentPropertiesPanel();
    ~DocumentPropertiesPanel() override = default;

    void set_document(SPDocument* document);
    void set_desktop(SPDesktop* desktop);

    void update(SPNamedView* namedview, SPRoot* root);

    Gtk::Grid* get_widget() const { return _page->left_grid(); }

private:
    void connect_signals();

    void on_check_toggled(bool checked, PageProperties::Check element);
    void on_dimension_changed(double x, double y, Util::Unit const* unit,
                              PageProperties::Dimension element);
    void on_resize_to_fit();

    void update_viewbox_ui(SPDocument* document);
    void update_scale_ui(SPDocument* document);

    PageProperties* _page = nullptr;
    SPDocument* _document = nullptr;
    SPDesktop* _desktop = nullptr;
    OperationBlocker _update;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_DOCUMENT_PROPS_PANEL_H
