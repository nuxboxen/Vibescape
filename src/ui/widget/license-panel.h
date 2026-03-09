// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * License panel — Creative Commons / license selector.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_LICENSE_PANEL_H
#define INKSCAPE_UI_WIDGET_LICENSE_PANEL_H

#include <gtkmm/box.h>

class SPDocument;

namespace Inkscape::UI::Widget {

class Licensor;

/**
 * Passive panel wrapping the Licensor widget.
 *
 * set_document(SPDocument*) must be called on document changes.
 * update(SPDocument*) refreshes the UI from the document's RDF data.
 */
class LicensePanel : public Gtk::Box {
public:
    LicensePanel();
    ~LicensePanel() override = default;

    void set_document(SPDocument* document);
    void update(SPDocument* document);

private:
    Licensor* _licensor = nullptr;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_LICENSE_PANEL_H
