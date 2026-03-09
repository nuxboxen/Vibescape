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

#include "display-props-panel.h"

#include <2geom/transforms.h>

#include "document.h"
#include "document-undo.h"
#include "object/sp-namedview.h"
#include "page-manager.h"
#include "util/units.h"
#include "colors/color.h"

namespace Inkscape::UI::Widget {

namespace {

void set_namedview_bool(SPNamedView* nv, Inkscape::Util::Internal::ContextString operation, SPAttr key, bool on) {
    if (!nv || !nv->document) return;

    nv->change_bool_setting(key, on);
    nv->document->setModifiedSinceSave();
    DocumentUndo::done(nv->document, operation, "");
}

void set_namedview_color(SPNamedView* nv, const char* key,
                                Inkscape::Util::Internal::ContextString operation,
                                SPAttr color_key, SPAttr opacity_key,
                                Colors::Color const& color) {
    if (!nv || !nv->document) return;

    nv->change_color(color_key, opacity_key, color);
    nv->document->setModifiedSinceSave();
    DocumentUndo::maybeDone(nv->document, key, operation, "");
}

void display_unit_change(SPDocument* doc, SPNamedView* nv, Inkscape::Util::Unit const* unit) {
    if (!nv || !unit) return;

    nv->setAttribute("inkscape:document-units", unit->abbr);
    DocumentUndo::done(doc, RC_("Undo", "Set display unit"), "");
}

} // anonymous namespace

// ---------------------------------------------------------------------------

DisplayPropertiesPanel::DisplayPropertiesPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL)
{
    _page = Gtk::manage(PageProperties::create());
    append(*_page);
    connect_signals();
}

void DisplayPropertiesPanel::set_document(SPDocument* document) {
    _document = document;
}

void DisplayPropertiesPanel::connect_signals() {
    _page->signal_color_changed().connect(
        sigc::mem_fun(*this, &DisplayPropertiesPanel::on_color_changed));
    _page->signal_check_toggled().connect(
        sigc::mem_fun(*this, &DisplayPropertiesPanel::on_check_toggled));
    _page->signal_unit_changed().connect(
        sigc::mem_fun(*this, &DisplayPropertiesPanel::on_unit_changed));
}

void DisplayPropertiesPanel::on_color_changed(Colors::Color const& color, PageProperties::Color element) {
    if (_update.pending() || !_document) return;

    auto nv = _document->getNamedView();
    if (!nv) return;

    auto scoped(_update.block());
    switch (element) {
        case PageProperties::Color::Desk:
            set_namedview_color(nv, "document-color-desk",
                RC_("Undo", "Desk color"),
                SPAttr::INKSCAPE_DESK_COLOR, SPAttr::INKSCAPE_DESK_OPACITY, color);
            break;
        case PageProperties::Color::Background:
            set_namedview_color(nv, "document-color-background",
                RC_("Undo", "Background color"),
                SPAttr::PAGECOLOR, SPAttr::INKSCAPE_PAGEOPACITY, color);
            break;
        case PageProperties::Color::Border:
            set_namedview_color(nv, "document-color-border",
                RC_("Undo", "Border color"),
                SPAttr::BORDERCOLOR, SPAttr::BORDEROPACITY, color);
            break;
    }
}

void DisplayPropertiesPanel::on_check_toggled(bool checked, PageProperties::Check element) {
    if (_update.pending() || !_document) return;

    auto nv = _document->getNamedView();
    if (!nv) return;

    auto scoped(_update.block());
    switch (element) {
        case PageProperties::Check::Checkerboard:
            set_namedview_bool(nv, RC_("Undo", "Toggle checkerboard"),
                SPAttr::INKSCAPE_DESK_CHECKERBOARD, checked); break;
        case PageProperties::Check::Border:
            set_namedview_bool(nv, RC_("Undo", "Toggle page border"),
                SPAttr::SHOWBORDER, checked); break;
        case PageProperties::Check::BorderOnTop:
            set_namedview_bool(nv, RC_("Undo", "Toggle border on top"),
                SPAttr::BORDERLAYER, checked); break;
        case PageProperties::Check::Shadow:
            set_namedview_bool(nv, RC_("Undo", "Toggle page shadow"),
                SPAttr::SHOWPAGESHADOW, checked); break;
        case PageProperties::Check::AntiAlias:
            set_namedview_bool(nv, RC_("Undo", "Toggle anti-aliasing"),
                SPAttr::INKSCAPE_ANTIALIAS_RENDERING, checked); break;
        case PageProperties::Check::ClipToPage:
            set_namedview_bool(nv, RC_("Undo", "Toggle clip to page mode"),
                SPAttr::INKSCAPE_CLIP_TO_PAGE_RENDERING, checked); break;
        case PageProperties::Check::PageLabelStyle:
            set_namedview_bool(nv, RC_("Undo", "Toggle page label style"),
                SPAttr::PAGELABELSTYLE, checked); break;
        default: break;
    }
}

void DisplayPropertiesPanel::on_unit_changed(Util::Unit const* unit, PageProperties::Units element) {
    if (_update.pending() || !_document) return;

    if (element == PageProperties::Units::Display) {
        display_unit_change(_document, _document->getNamedView(), unit);
    }
}

void DisplayPropertiesPanel::update_display_unit_ui(SPNamedView* nv) {
    if (!nv) return;

    if (nv->display_units) {
        _page->set_unit(PageProperties::Units::Display, nv->display_units->abbr);
    }
}

void DisplayPropertiesPanel::update(SPNamedView* nv) {
    if (_update.pending() || !nv || !_document) return;

    auto scoped(_update.block());

    update_display_unit_ui(nv);
    _page->set_check(PageProperties::Check::Checkerboard, nv->desk_checkerboard);
    _page->set_color(PageProperties::Color::Desk, nv->getDeskColor());
    _page->set_check(PageProperties::Check::AntiAlias, nv->antialias_rendering);
    _page->set_check(PageProperties::Check::ClipToPage, nv->clip_to_page);

    // Handle page manager related settings
    auto& page_manager = _document->getPageManager();
    _page->set_color(PageProperties::Color::Background, page_manager.getBackgroundColor());
    _page->set_check(PageProperties::Check::Border, page_manager.border_show);
    _page->set_check(PageProperties::Check::BorderOnTop, page_manager.border_on_top);
    _page->set_color(PageProperties::Color::Border, page_manager.getBorderColor());
    _page->set_check(PageProperties::Check::Shadow, page_manager.shadow_show);
    _page->set_check(PageProperties::Check::PageLabelStyle, page_manager.label_style != "default");
}

} // namespace Inkscape::UI::Widget
