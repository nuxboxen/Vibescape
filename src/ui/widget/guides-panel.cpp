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

#include "guides-panel.h"

#include <glibmm/i18n.h>

#include "document.h"
#include "document-undo.h"
#include "object/sp-namedview.h"
#include "ui/builder-utils.h"
#include "ui/widget/color-picker.h"

using Inkscape::UI::create_builder;
using Inkscape::UI::get_widget;

namespace Inkscape::UI::Widget {

namespace {

void write_namedview_bool(SPNamedView* nv,
                          Inkscape::Util::Internal::ContextString operation,
                          SPAttr key, bool on)
{
    if (!nv || !nv->document) return;

    nv->change_bool_setting(key, on);
    nv->document->setModifiedSinceSave();
    DocumentUndo::done(nv->document, operation, "");
}

void write_namedview_color(SPNamedView* nv,
                           const char* undo_key,
                           Inkscape::Util::Internal::ContextString operation,
                           SPAttr color_key, SPAttr opacity_key,
                           Colors::Color const& color)
{
    if (!nv || !nv->document) return;

    nv->change_color(color_key, opacity_key, color);
    nv->document->setModifiedSinceSave();
    DocumentUndo::maybeDone(nv->document, undo_key, operation, "");
}

} // namespace

GuidesPanel::GuidesPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
    , _builder(create_builder("guides-panel.ui"))
    , _show_guides(get_widget<Gtk::Switch>(_builder, "show-guides"))
    , _lock_guides(get_widget<Gtk::CheckButton>(_builder, "lock-guides"))
    , _create_guides_btn(get_widget<Gtk::Button>(_builder, "create-guides-btn"))
    , _delete_guides_btn(get_widget<Gtk::Button>(_builder, "delete-guides-btn"))
    , _guide_color(get_derived_widget<ColorPicker>(_builder, "guide-color",
        _("Guide color"), true))
    , _guide_highlight(get_derived_widget<ColorPicker>(_builder, "guide-hi-color",
        _("Highlight color"), true))
{
    auto& grid = get_widget<Gtk::Grid>(_builder, "guides-grid");
    append(grid);

    // Set initial colors after widgets are created
    _guide_color.setColor(Colors::Color(0x0000ff99));
    _guide_highlight.setColor(Colors::Color(0xff000099));

    // --- write-back signal handlers ---
    _show_guides.property_active().signal_changed().connect([this]{
        if (_update.pending() || !_namedview) return;

        write_namedview_bool(_namedview,
            RC_("Undo", "Toggle show guides"), SPAttr::SHOWGUIDES,
            _show_guides.get_active());
    });

    _lock_guides.signal_toggled().connect([this]{
        if (_update.pending() || !_namedview) return;

        write_namedview_bool(_namedview,
            RC_("Undo", "Toggle lock guides"), SPAttr::INKSCAPE_LOCKGUIDES,
            _lock_guides.get_active());
    });

    _guide_color.connectChanged([this](Colors::Color const& color){
        if (_update.pending() || !_namedview) return;

        write_namedview_color(_namedview, "guide-color",
            RC_("Undo", "Set guide color"),
            SPAttr::GUIDECOLOR, SPAttr::GUIDEOPACITY, color);
    });

    _guide_highlight.connectChanged([this](Colors::Color const& color){
        if (_update.pending() || !_namedview) return;

        write_namedview_color(_namedview, "guide-hi-color",
            RC_("Undo", "Set guide highlight color"),
            SPAttr::GUIDEHICOLOR, SPAttr::GUIDEHIOPACITY, color);
    });
}

void GuidesPanel::update(SPNamedView* namedview) {
    _namedview = namedview;
    if (!namedview) return;

    auto scoped(_update.block());
    _show_guides.set_active(namedview->getShowGuides());
    _lock_guides.set_active(namedview->getLockGuides());
    _guide_color.setColor(namedview->getGuideColor());
    _guide_highlight.setColor(namedview->getGuideHiColor());
}

} // namespace Inkscape::UI::Widget
