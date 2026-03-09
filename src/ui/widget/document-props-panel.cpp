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

#include "document-props-panel.h"

#include <optional>
#include <2geom/rect.h>
#include <2geom/transforms.h>

#include "document.h"
#include "document-undo.h"
#include "object/sp-grid.h"
#include "object/sp-namedview.h"
#include "object/sp-root.h"
#include "page-manager.h"
#include "selection.h"
#include "svg/svg-length.h"
#include "util/units.h"
#include "ui/icon-names.h"

namespace Inkscape::UI::Widget {

namespace {

void set_namedview_bool(SPNamedView* nv, Inkscape::Util::Internal::ContextString operation, SPAttr key, bool on) {
    if (!nv || !nv->document) return;

    nv->change_bool_setting(key, on);
    nv->document->setModifiedSinceSave();
    DocumentUndo::done(nv->document, operation, "");
}

void set_document_dimensions(SPDocument* doc, double width, double height, Inkscape::Util::Unit const* unit) {
    if (!doc) return;

    auto new_w = Inkscape::Util::Quantity(width, unit);
    auto new_h = Inkscape::Util::Quantity(height, unit);
    auto rect = Geom::Rect(Geom::Point(0, 0),
                           Geom::Point(new_w.value("px"), new_h.value("px")));
    doc->fitToRect(rect, false);
    if (!doc->yaxisdown()) {
        auto vert_offset = Geom::Translate(Geom::Point(0, (doc->getHeight().value("px") - new_h.value("px"))));
        doc->getRoot()->translateChildItems(vert_offset);
    }
    DocumentUndo::done(doc, RC_("Undo", "Set page size"), "");
}

void set_document_viewbox_pos(SPDocument* doc, double x, double y) {
    if (!doc) return;

    auto box = doc->getViewBox();
    doc->setViewBox(Geom::Rect::from_xywh(x, y, box.width(), box.height()));
    DocumentUndo::done(doc, RC_("Undo", "Set viewbox position"), "");
}

void set_document_viewbox_size(SPDocument* doc, double width, double height) {
    if (!doc) return;

    auto box = doc->getViewBox();
    doc->setViewBox(Geom::Rect::from_xywh(box.min()[Geom::X], box.min()[Geom::Y], width, height));
    DocumentUndo::done(doc, RC_("Undo", "Set viewbox size"), "");
}

void set_document_scale_helper(SPDocument& doc, double scale) {
    if (scale <= 0) return;

    auto root = doc.getRoot();
    auto box  = doc.getViewBox();
    doc.setViewBox(Geom::Rect::from_xywh(
        box.min()[Geom::X], box.min()[Geom::Y],
        root->width.value / scale, root->height.value / scale));
}

void set_document_scale(SPDocument* doc, double scale) {
    if (!doc || scale <= 0) return;

    set_document_scale_helper(*doc, scale);
    DocumentUndo::done(doc, RC_("Undo", "Set page scale"), "");
}

void set_content_scale(SPDocument* doc, double scale) {
    if (!doc || scale <= 0) return;

    auto old_scale = doc->getDocumentScale(false);
    auto delta = old_scale * Geom::Scale(scale).inverse();
    doc->scaleContentBy(delta);
    doc->getPageManager().scalePages(delta);
    if (auto nv = doc->getNamedView()) {
        for (auto grid : nv->grids) {
            grid->scale(delta);
        }
    }
}

std::optional<Geom::Scale> get_document_scale_helper(SPDocument& doc) {
    auto root = doc.getRoot();
    if (root &&
        root->width._set  && root->width.unit  != SVGLength::PERCENT &&
        root->height._set && root->height.unit != SVGLength::PERCENT) {
        if (root->viewBox_set) {
            auto vw = root->viewBox.width();
            auto vh = root->viewBox.height();
            if (vw > 0 && vh > 0)
                return Geom::Scale(root->width.value / vw, root->height.value / vh);
        } else {
            auto w = root->width.computed;
            auto h = root->height.computed;
            if (w > 0 && h > 0)
                return Geom::Scale(root->width.value / w, root->height.value / h);
        }
    }
    return {};
}

} // anonymous namespace

DocumentPropertiesPanel::DocumentPropertiesPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL)
{
    _page = Gtk::manage(PageProperties::create());
    append(*_page);
    connect_signals();
}

void DocumentPropertiesPanel::set_document(SPDocument* document) {
    _document = document;
}

void DocumentPropertiesPanel::set_desktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void DocumentPropertiesPanel::connect_signals() {
    _page->signal_check_toggled().connect(
        sigc::mem_fun(*this, &DocumentPropertiesPanel::on_check_toggled));
    _page->signal_dimension_changed().connect(
        sigc::mem_fun(*this, &DocumentPropertiesPanel::on_dimension_changed));
    _page->signal_resize_to_fit().connect(
        sigc::mem_fun(*this, &DocumentPropertiesPanel::on_resize_to_fit));
}

void DocumentPropertiesPanel::on_check_toggled(bool checked, PageProperties::Check element) {
    if (_update.pending() || !_document) return;

    auto nv = _document->getNamedView();
    auto scoped(_update.block());
    switch (element) {
        case PageProperties::Check::YAxisPointsDown:
            set_namedview_bool(nv, RC_("Undo", "Toggle system coordinate Y axis orientation"),
                SPAttr::INKSCAPE_Y_AXIS_DOWN, checked); break;
        case PageProperties::Check::OriginCurrentPage:
            set_namedview_bool(nv, RC_("Undo", "Toggle system coordinate origin correction"),
                SPAttr::INKSCAPE_ORIGIN_CORRECTION, checked); break;
        default: break;
    }
}

void DocumentPropertiesPanel::on_dimension_changed(double x, double y, Util::Unit const* unit, PageProperties::Dimension element) {
    if (_update.pending() || !_document) return;

    auto scoped(_update.block());
    switch (element) {
        case PageProperties::Dimension::PageTemplate:
        case PageProperties::Dimension::PageSize:
            set_document_dimensions(_document, x, y, unit);
            update_viewbox_ui(_document);
            break;
        case PageProperties::Dimension::ViewboxSize:
            set_document_viewbox_size(_document, x, y);
            break;
        case PageProperties::Dimension::ViewboxPosition:
            set_document_viewbox_pos(_document, x, y);
            break;
        case PageProperties::Dimension::ScaleContent:
            set_content_scale(_document, x);
            //todo: doc-properties dialog calls both, is this right?
            set_document_scale(_document, x);
            break;
        case PageProperties::Dimension::Scale:
            set_document_scale(_document, x);
            break;
    }
    update_scale_ui(_document);
}

void DocumentPropertiesPanel::on_resize_to_fit() {
    if (_update.pending() || !_document || !_desktop) return;

    auto& page_manager = _document->getPageManager();
    page_manager.selectPage(0);
    page_manager.fitToSelection(_desktop->getSelection());
    DocumentUndo::done(_document, RC_("Undo", "Resize page to fit"), INKSCAPE_ICON("tool-pages"));
}

void DocumentPropertiesPanel::update_viewbox_ui(SPDocument* document) {
    if (!document) return;

    Geom::Rect viewBox = document->getViewBox();
    _page->set_dimension(PageProperties::Dimension::ViewboxPosition,
                         viewBox.min()[Geom::X], viewBox.min()[Geom::Y]);
    _page->set_dimension(PageProperties::Dimension::ViewboxSize,
                         viewBox.width(), viewBox.height());
}

void DocumentPropertiesPanel::update_scale_ui(SPDocument* document) {
    if (!document) return;

    if (auto scale = get_document_scale_helper(*document)) {
        auto sx = (*scale)[Geom::X];
        auto sy = (*scale)[Geom::Y];
        double eps = 0.0001;
        bool uniform = std::fabs(sx - sy) < eps;
        _page->set_dimension(PageProperties::Dimension::Scale, sx, sx);
        _page->set_check(PageProperties::Check::NonuniformScale, !uniform);
        _page->set_check(PageProperties::Check::DisabledScale, false);
    } else {
        _page->set_dimension(PageProperties::Dimension::Scale, 1, 1);
        _page->set_check(PageProperties::Check::NonuniformScale, false);
        _page->set_check(PageProperties::Check::DisabledScale, true);
    }
}

void DocumentPropertiesPanel::update(SPNamedView* nv, SPRoot* root) {
    if (_update.pending() || !root) return;

    auto scoped(_update.block());

    double doc_w = root->width.value;
    Glib::ustring doc_w_unit = Inkscape::Util::UnitTable::get().getUnit(root->width.unit)->abbr;
    bool percent = doc_w_unit == "%";
    if (doc_w_unit.empty()) {
        doc_w_unit = "px";
    } else if (doc_w_unit == "%" && root->viewBox_set) {
        doc_w_unit = "px";
        doc_w = root->viewBox.width();
    }

    double doc_h = root->height.value;
    Glib::ustring doc_h_unit = Inkscape::Util::UnitTable::get().getUnit(root->height.unit)->abbr;
    percent = percent || doc_h_unit == "%";
    if (doc_h_unit.empty()) {
        doc_h_unit = "px";
    } else if (doc_h_unit == "%" && root->viewBox_set) {
        doc_h_unit = "px";
        doc_h = root->viewBox.height();
    }

    _page->set_check(PageProperties::Check::UnsupportedSize, percent);
    _page->set_dimension(PageProperties::Dimension::PageSize, doc_w, doc_h);
    _page->set_unit(PageProperties::Units::Document, doc_w_unit);

    if (auto doc = root->document) {
        update_viewbox_ui(doc);
        update_scale_ui(doc);
    }

    // coordinate system settings
    if (nv) {
        _page->set_check(PageProperties::Check::YAxisPointsDown, nv->is_y_axis_down());
        _page->set_check(PageProperties::Check::OriginCurrentPage, nv->get_origin_follows_page());
    }
}

} // namespace Inkscape::UI::Widget
