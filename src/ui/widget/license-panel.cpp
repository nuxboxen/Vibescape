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

#include "license-panel.h"

#include <glibmm/i18n.h>
#include <gtkmm/label.h>

#include "ui/widget/licensor.h"

namespace Inkscape::UI::Widget {

LicensePanel::LicensePanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 4)
{
    set_margin(8);

    auto const title = Gtk::make_managed<Gtk::Label>();
    title->set_markup(_("<b>License</b>"));
    title->set_halign(Gtk::Align::START);
    append(*title);

    _licensor = Gtk::make_managed<Licensor>();
    _licensor->init();
    _licensor->set_hexpand();
    _licensor->set_valign(Gtk::Align::CENTER);
    append(*_licensor);
}

void LicensePanel::set_document(SPDocument* document)
{
    _licensor->set_document(document);
}

void LicensePanel::update(SPDocument* document)
{
    if (document) {
        _licensor->update(document);
    }
}

} // namespace Inkscape::UI::Widget
