// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Document properties dialog, Gtkmm-style.
 */
/* Authors:
 *   bulia byak <buliabyak@users.sf.net>
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon Phillips <jon@rejon.org>
 *   Ralf Stephan <ralf@ark.in-berlin.de> (Gtkmm)
 *   Diederik van Lierop <mail@diedenrezi.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006-2008 Johan Engelen  <johan@shouraizou.nl>
 * Copyright (C) 2000 - 2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "document-properties.h"

#include <giomm/themedicon.h>
#include <glibmm/main.h>
#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/object.h>
#include <gtkmm/spinbutton.h>

#include "inkscape-window.h"
#include "object/sp-guide.h"
#include "object/sp-root.h"
#include "page-manager.h"
#include "preferences.h"
#include "rdf.h"
#include "selection.h"
#include "ui/dialog/choose-file-utils.h"
#include "ui/icon-loader.h"
#include "ui/icon-names.h"
#include "ui/pack.h"
#include "ui/popup-menu.h"
#include "ui/util.h"
#include "ui/widget/alignment-selector.h"
#include "ui/widget/color-system-panel.h"
#include "ui/widget/grid-widget.h"
#include "ui/widget/guides-panel.h"
#include "ui/widget/metadata-panel.h"
#include "ui/widget/scripting-panel.h"
#include "ui/widget/notebook-page.h"
#include "ui/widget/page-properties.h"
#include "ui/widget/generic/popover-menu.h"
#include "util/expression-evaluator.h"

namespace Inkscape::UI {

namespace Widget {
using GridWidget = Inkscape::UI::Widget::GridWidget;
} // namespace Widget

namespace Dialog {

DocumentProperties::DocumentProperties()
    : DialogBase("/dialogs/documentoptions", "DocumentProperties")
    , _page_page(Gtk::make_managed<UI::Widget::NotebookPage>(1, 1, true))
    , _guides_panel(Gtk::make_managed<UI::Widget::GuidesPanel>())
    , _cms_panel(Gtk::make_managed<UI::Widget::ColorSystemPanel>())
    , _scripting_panel(Gtk::make_managed<UI::Widget::ScriptingPanel>())
    , _metadata_panel(Gtk::make_managed<UI::Widget::MetadataPanel>())
    , _page_metadata2(Gtk::make_managed<UI::Widget::NotebookPage>(1, 1, true))
    //---------------------------------------------------------------
    , _grids_label_def("", Gtk::Align::START)
    , _grids_vbox(Gtk::Orientation::VERTICAL)
    , _grids_hbox_crea(Gtk::Orientation::HORIZONTAL)
    // Attach nodeobservers to this document
    , _namedview_connection(this)
    , _root_connection(this)
{
    append(_popoverbin);
    _popoverbin.set_expand();
    _popoverbin.setChild(&_notebook);

    _notebook.append_page(*_page_page,        _("Display"));
    _notebook.append_page(*_guides_panel,     _("Guides"));
    _notebook.append_page(_grids_vbox,        _("Grids"));
    _notebook.append_page(*_cms_panel,        _("Color"));
    _notebook.append_page(*_scripting_panel,  _("Scripting"));
    _notebook.append_page(*_metadata_panel,   _("Metadata"));
    _notebook.append_page(*_page_metadata2,   _("License"));
    _notebook.signal_switch_page().connect([this](Gtk::Widget const *, unsigned const page){
        // we cannot use widget argument, as this notification fires during destruction with all pages passed one by one
        // page no 3 - cms
        if (page == 3) {
            // lazy-load color profiles; it can get prohibitively expensive when hundreds are installed
            _cms_panel->populate_available_profiles(false);
        }
    });

    _wr.setUpdating (true);
    build_page();
    _guides_panel->set_margin(8);
    _cms_panel->set_margin(8);
    _scripting_panel->set_margin(8);
    _metadata_panel->set_margin(8);
    build_gridspage();
    build_metadata();
    _wr.setUpdating (false);
}

DocumentProperties::~DocumentProperties() = default;

//========================================================================

void set_namedview_bool(SPDesktop* desktop, Inkscape::Util::Internal::ContextString operation, SPAttr key, bool on) {
    if (!desktop || !desktop->getDocument()) return;

    desktop->getNamedView()->change_bool_setting(key, on);

    desktop->getDocument()->setModifiedSinceSave();
    DocumentUndo::done(desktop->getDocument(), operation, "");
}

void set_color(SPDesktop* desktop, const char* key, Inkscape::Util::Internal::ContextString operation, SPAttr color_key, SPAttr opacity_key, Colors::Color const &color) {
    if (!desktop || !desktop->getDocument()) return;

    desktop->getNamedView()->change_color(color_key, opacity_key, color);
    desktop->getDocument()->setModifiedSinceSave();
    DocumentUndo::maybeDone(desktop->getDocument(), key, operation, "");
}

void set_document_dimensions(SPDesktop* desktop, double width, double height, const Inkscape::Util::Unit* unit) {
    if (!desktop) return;

    auto new_width_q = Inkscape::Util::Quantity(width, unit);
    auto new_height_q = Inkscape::Util::Quantity(height, unit);
    SPDocument* doc = desktop->getDocument();
    Inkscape::Util::Quantity const old_height_q = doc->getHeight();
    auto rect = Geom::Rect(Geom::Point(0, 0), Geom::Point(new_width_q.value("px"), new_height_q.value("px")));
    doc->fitToRect(rect, false);

    // The origin for the user is in the lower left corner; this point should remain stationary when
    // changing the page size. The SVG's origin however is in the upper left corner, so we must compensate for this
    if (!doc->yaxisdown()) {
        auto const vert_offset = Geom::Translate(Geom::Point(0, (old_height_q.value("px") - new_height_q.value("px"))));
        doc->getRoot()->translateChildItems(vert_offset);
    } else {
        // when this yaxisdown is true, we need to translate just the guides
        // the guides simply need their new converted positions
        // in reference to: https://gitlab.com/inkscape/inkscape/-/issues/1230
        for (auto guide : doc->getNamedView()->guides) {
            guide->moveto(guide->getPoint() * Geom::Translate(0, 0), true);
        }
    }

    // units: this is most likely not needed, units are part of document size attributes
    // if (unit) {
        // set_namedview_value(desktop, "", SPAttr::UNITS)
        // write_str_to_xml(desktop, _("Set document unit"), "unit", unit->abbr.c_str());
    // }
    doc->setWidthAndHeight(new_width_q, new_height_q, true);

    DocumentUndo::done(doc, RC_("Undo", "Set page size"), "");
}

void DocumentProperties::set_viewbox_pos(SPDesktop* desktop, double x, double y) {
    if (!desktop) return;

    auto document = desktop->getDocument();
    if (!document) return;

    auto box = document->getViewBox();
    document->setViewBox(Geom::Rect::from_xywh(x, y, box.width(), box.height()));
    DocumentUndo::done(document, RC_("Undo", "Set viewbox position"), "");
    update_scale_ui(desktop);
}

void DocumentProperties::set_viewbox_size(SPDesktop* desktop, double width, double height) {
    if (!desktop) return;

    auto document = desktop->getDocument();
    if (!document) return;

    auto box = document->getViewBox();
    document->setViewBox(Geom::Rect::from_xywh(box.min()[Geom::X], box.min()[Geom::Y], width, height));
    DocumentUndo::done(document, RC_("Undo", "Set viewbox size"), "");
    update_scale_ui(desktop);
}

// helper function to set document scale; uses magnitude of document width/height only, not computed (pixel) values
void set_document_scale_helper(SPDocument& document, double scale) {
    if (scale <= 0) return;

    auto root = document.getRoot();
    auto box = document.getViewBox();
    document.setViewBox(Geom::Rect::from_xywh(
        box.min()[Geom::X], box.min()[Geom::Y],
        root->width.value / scale, root->height.value / scale)
    );
}

void DocumentProperties::set_content_scale(SPDesktop *desktop, double scale)
{
    if (!desktop) return;

    auto document = desktop->getDocument();
    if (!document) return;

    if (scale > 0) {
        auto old_scale = document->getDocumentScale(false);
        auto delta = old_scale * Geom::Scale(scale).inverse();

        // Shapes in the document
        document->scaleContentBy(delta);

        // Pages, margins and bleeds
        document->getPageManager().scalePages(delta);

        // Grids
        if (auto nv = document->getNamedView()) {
            for (auto grid : nv->grids) {
                grid->scale(delta);
            }
        }
    }
}

void DocumentProperties::set_document_scale(SPDesktop* desktop, double scale) {
    if (!desktop) return;

    auto document = desktop->getDocument();
    if (!document) return;

    if (scale > 0) {
        set_document_scale_helper(*document, scale);
        update_viewbox_ui(desktop);
        update_scale_ui(desktop);
        DocumentUndo::done(document, RC_("Undo", "Set page scale"), "");
    }
}

// document scale as a ratio of document size and viewbox size
// as described in Wiki: https://wiki.inkscape.org/wiki/index.php/Units_In_Inkscape
// for example: <svg width="100mm" height="100mm" viewBox="0 0 100 100"> will report 1:1 scale
std::optional<Geom::Scale> get_document_scale_helper(SPDocument& doc) {
    auto root = doc.getRoot();
    if (root &&
        root->width._set  && root->width.unit  != SVGLength::PERCENT &&
        root->height._set && root->height.unit != SVGLength::PERCENT) {
        if (root->viewBox_set) {
            // viewbox and document size present
            auto vw = root->viewBox.width();
            auto vh = root->viewBox.height();
            if (vw > 0 && vh > 0) {
                return Geom::Scale(root->width.value / vw, root->height.value / vh);
            }
        } else {
            // no viewbox, use SVG size in pixels
            auto w = root->width.computed;
            auto h = root->height.computed;
            if (w > 0 && h > 0) {
                return Geom::Scale(root->width.value / w, root->height.value / h);
            }
        }
    }

    // there is no scale concept applicable in the current state
    return std::optional<Geom::Scale>();
}

void DocumentProperties::update_scale_ui(SPDesktop* desktop) {
    if (!desktop) return;

    auto document = desktop->getDocument();
    if (!document) return;

    using UI::Widget::PageProperties;
    if (auto scale = get_document_scale_helper(*document)) {
        auto sx = (*scale)[Geom::X];
        auto sy = (*scale)[Geom::Y];
        double eps = 0.0001; // TODO: tweak this value
        bool uniform = fabs(sx - sy) < eps;
        _page->set_dimension(PageProperties::Dimension::Scale, sx, sx); // only report one, only one "scale" is used
        _page->set_check(PageProperties::Check::NonuniformScale, !uniform);
        _page->set_check(PageProperties::Check::DisabledScale, false);
    } else {
        // no scale
        _page->set_dimension(PageProperties::Dimension::Scale, 1, 1);
        _page->set_check(PageProperties::Check::NonuniformScale, false);
        _page->set_check(PageProperties::Check::DisabledScale, true);
    }
}

void DocumentProperties::update_viewbox_ui(SPDesktop* desktop) {
    if (!desktop) return;

    auto document = desktop->getDocument();
    if (!document) return;

    using UI::Widget::PageProperties;
    Geom::Rect viewBox = document->getViewBox();
    _page->set_dimension(PageProperties::Dimension::ViewboxPosition, viewBox.min()[Geom::X], viewBox.min()[Geom::Y]);
    _page->set_dimension(PageProperties::Dimension::ViewboxSize, viewBox.width(), viewBox.height());
}

void DocumentProperties::build_page()
{
    using UI::Widget::PageProperties;
    _page = Gtk::manage(PageProperties::create());
    _page_page->table().attach(*_page->left_grid(), 0, 0);
    auto display = Gtk::make_managed<Gtk::Label>(_("Display"));
    display->add_css_class("dialog-heading");
    display->set_halign(Gtk::Align::START);
    display->set_margin_top(8);
    _page_page->table().attach(*display, 0, 1);
    _page_page->table().attach(*_page->right_grid(), 0, 2);

    _page->signal_color_changed().connect([this](Colors::Color const &color, PageProperties::Color const element){
        if (_wr.isUpdating() || !_wr.desktop()) return;

        _wr.setUpdating(true);
        switch (element) {
            case PageProperties::Color::Desk:
                set_color(_wr.desktop(), "document-color-desk", RC_("Undo", "Desk color"), SPAttr::INKSCAPE_DESK_COLOR, SPAttr::INKSCAPE_DESK_OPACITY, color);
                break;
            case PageProperties::Color::Background:
                set_color(_wr.desktop(), "document-color-background", RC_("Undo", "Background color"), SPAttr::PAGECOLOR, SPAttr::INKSCAPE_PAGEOPACITY, color);
                break;
            case PageProperties::Color::Border:
                set_color(_wr.desktop(), "document-color-border", RC_("Undo", "Border color"), SPAttr::BORDERCOLOR, SPAttr::BORDEROPACITY, color);
                break;
        }
        _wr.setUpdating(false);
    });

    _page->signal_dimension_changed().connect([this](double const x, double const y,
                                                     auto const unit,
                                                     PageProperties::Dimension const element)
    {
        if (_wr.isUpdating() || !_wr.desktop()) return;

        _wr.setUpdating(true);
        switch (element) {
            case PageProperties::Dimension::PageTemplate:
            case PageProperties::Dimension::PageSize:
                set_document_dimensions(_wr.desktop(), x, y, unit);
                update_viewbox(_wr.desktop());
                break;

            case PageProperties::Dimension::ViewboxSize:
                set_viewbox_size(_wr.desktop(), x, y);
                break;

            case PageProperties::Dimension::ViewboxPosition:
                set_viewbox_pos(_wr.desktop(), x, y);
                break;

            case PageProperties::Dimension::ScaleContent:
                set_content_scale(_wr.desktop(), x);
            case PageProperties::Dimension::Scale:
                set_document_scale(_wr.desktop(), x); // only uniform scale; there's no 'y' in the dialog
                break;
        }
        _wr.setUpdating(false);
    });

    _page->signal_check_toggled().connect([this](bool const checked, PageProperties::Check const element){
        if (_wr.isUpdating() || !_wr.desktop()) return;

        _wr.setUpdating(true);
        switch (element) {
            case PageProperties::Check::Checkerboard:
                set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle checkerboard"), SPAttr::INKSCAPE_DESK_CHECKERBOARD, checked);
                break;
            case PageProperties::Check::Border:
                set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle page border"), SPAttr::SHOWBORDER, checked);
                break;
            case PageProperties::Check::BorderOnTop:
                set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle border on top"), SPAttr::BORDERLAYER, checked);
                break;
            case PageProperties::Check::Shadow:
                set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle page shadow"), SPAttr::SHOWPAGESHADOW, checked);
                break;
            case PageProperties::Check::AntiAlias:
                set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle anti-aliasing"), SPAttr::INKSCAPE_ANTIALIAS_RENDERING, checked);
                break;
            case PageProperties::Check::ClipToPage:
                set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle clip to page mode"), SPAttr::INKSCAPE_CLIP_TO_PAGE_RENDERING, checked);
                break;
            case PageProperties::Check::PageLabelStyle:
                set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle page label style"), SPAttr::PAGELABELSTYLE, checked);
                break;
        case PageProperties::Check::YAxisPointsDown:
            set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle system coordinate Y axis orientation"), SPAttr::INKSCAPE_Y_AXIS_DOWN, checked);
            break;
        case PageProperties::Check::OriginCurrentPage:
            set_namedview_bool(_wr.desktop(), RC_("Undo", "Toggle system coordinate origin correction"), SPAttr::INKSCAPE_ORIGIN_CORRECTION, checked);
            break;
        }
        _wr.setUpdating(false);
    });

    _page->signal_unit_changed().connect([this](Inkscape::Util::Unit const * const unit, PageProperties::Units const element){
        if (_wr.isUpdating() || !_wr.desktop()) return;

        if (element == PageProperties::Units::Display) {
            // display only units
            display_unit_change(unit);
        }
        else if (element == PageProperties::Units::Document) {
            // not used, fired with page size
        }
    });

    _page->signal_resize_to_fit().connect([this]{
        if (_wr.isUpdating() || !_wr.desktop()) return;

        if (auto document = getDocument()) {
            auto &page_manager = document->getPageManager();
            page_manager.selectPage(0);
            // fit page to selection or content, if there's no selection
            page_manager.fitToSelection(_wr.desktop()->getSelection());
            DocumentUndo::done(document, RC_("Undo", "Resize page to fit"), INKSCAPE_ICON("tool-pages"));
            update_widgets();
        }
    });
}

void DocumentProperties::build_metadata()
{
    int row = 0;
    auto const llabel = Gtk::make_managed<Gtk::Label>();
    llabel->set_markup (_("<b>License</b>"));
    llabel->set_halign(Gtk::Align::START);
    llabel->set_valign(Gtk::Align::CENTER);
    _page_metadata2->table().attach(*llabel, 0, row, 2, 1);

    /* add license selector pull-down and URI */
    ++row;
    _licensor.init();

    _licensor.set_hexpand();
    _licensor.set_valign(Gtk::Align::CENTER);
    _page_metadata2->table().attach(_licensor, 0, row, 2, 1);
    _page_metadata2->table().set_valign(Gtk::Align::START);
}

/**
* Called for _updating_ the dialog. DO NOT call this a lot. It's expensive!
* Will need to probably create a GridManager with signals to each Grid attribute
*/
void DocumentProperties::rebuild_gridspage()
{
    _grids_list.remove_all();
    for (auto w : _grids_unified_size->get_widgets()) {
        _grids_unified_size->remove_widget(*w);
    }

    for (auto grid : getDesktop()->getNamedView()->grids) {
        add_grid_widget(grid);
    }

    update_grid_placeholder();
}

void DocumentProperties::update_grid_placeholder() {
    _no_grids.set_visible(_grids_list.get_first_child() == nullptr);
}

void DocumentProperties::add_grid_widget(SPGrid *grid)
{
    auto const widget = Gtk::make_managed<Inkscape::UI::Widget::GridWidget>(grid, grid ? grid->getRepr() : nullptr);
    _grids_list.append(*widget);
    _grids_unified_size->add_widget(*widget);
    // get rid of row highlight - they are not selectable (we just need to change the last one, but there's no API for that)
    int index = 0;
    for (auto row = _grids_list.get_row_at_index(index); row; row = _grids_list.get_row_at_index(++index)) {
        row->property_activatable() = false;
    }

    update_grid_placeholder();
}

void DocumentProperties::remove_grid_widget(XML::Node &node)
{
    // The SPObject is already gone, so we're working from the xml node directly.
    int index = 0;
    for (auto row = _grids_list.get_row_at_index(index); row; row = _grids_list.get_row_at_index(++index)) {
        if (auto widget = dynamic_cast<Inkscape::UI::Widget::GridWidget*>(row->get_child())) {
            if (&node == widget->get_tag()) {
                _grids_unified_size->remove_widget(*widget);
                _grids_list.remove(*row);
                break;
            }
        }
    }

    update_grid_placeholder();
}

/**
 * Build grid page of dialog.
 */
void DocumentProperties::build_gridspage()
{
    /// \todo FIXME: gray out snapping when grid is off.
    /// Dissenting view: you want snapping without grid.

    _grids_hbox_crea.set_spacing(5);
    _grids_hbox_crea.set_margin(8);
    _grids_hbox_crea.set_halign(Gtk::Align::CENTER);

    {
        auto btn = Gtk::make_managed<Gtk::Button>();
        btn->set_size_request(120); // make it easier to hit
        auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
        hbox->set_halign(Gtk::Align::CENTER);
        hbox->set_valign(Gtk::Align::CENTER);

        auto icon_image = Gtk::make_managed<Gtk::Image>();
        icon_image->set_from_icon_name("plus");
        icon_image->set_icon_size(Gtk::IconSize::NORMAL);
        hbox->append(*icon_image);

        auto btn_label = Gtk::make_managed<Gtk::Label>(_("New Grid"));
        btn_label->set_valign(Gtk::Align::CENTER);
        hbox->append(*btn_label);

        btn->set_child(*hbox);

        UI::pack_start(_grids_hbox_crea, *btn, false, true);
        btn->signal_clicked().connect([this]{ onNewGrid(GridType::RECTANGULAR); });
    }

    UI::pack_start(_grids_vbox, _grids_hbox_crea, false, false);
    _no_grids.set_text(_("There are no grids defined."));
    _no_grids.set_halign(Gtk::Align::CENTER);
    _no_grids.set_hexpand();
    _no_grids.set_margin_top(40);
    _no_grids.add_css_class("informational-text");
    UI::pack_start(_grids_vbox, _no_grids, false, false);
    UI::pack_start(_grids_vbox, _grids_wnd, true, true);
    _grids_wnd.set_child(_grids_list);
    _grids_list.set_show_separators();
    _grids_list.set_selection_mode(Gtk::SelectionMode::NONE);
    _grids_wnd.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    _grids_wnd.set_has_frame(false);
}

void DocumentProperties::update_viewbox(SPDesktop* desktop) {
    if (!desktop) return;

    auto* document = desktop->getDocument();
    if (!document) return;

    using UI::Widget::PageProperties;
    SPRoot* root = document->getRoot();
    if (root->viewBox_set) {
        auto& vb = root->viewBox;
        _page->set_dimension(PageProperties::Dimension::ViewboxPosition, vb.min()[Geom::X], vb.min()[Geom::Y]);
        _page->set_dimension(PageProperties::Dimension::ViewboxSize, vb.width(), vb.height());
    }

    update_scale_ui(desktop);
}

/**
 * Update dialog widgets from desktop. Also call updateWidget routines of the grids.
 */
void DocumentProperties::update_widgets()
{
    auto desktop = getDesktop();
    auto document = getDocument();
    if (_wr.isUpdating() || !document) return;

    auto nv = desktop->getNamedView();
    auto &page_manager = document->getPageManager();

    _wr.setUpdating(true);

    SPRoot *root = document->getRoot();

    double doc_w = root->width.value;
    Glib::ustring doc_w_unit = Util::UnitTable::get().getUnit(root->width.unit)->abbr;
    bool percent = doc_w_unit == "%";
    if (doc_w_unit == "") {
        doc_w_unit = "px";
    } else if (doc_w_unit == "%" && root->viewBox_set) {
        doc_w_unit = "px";
        doc_w = root->viewBox.width();
    }
    double doc_h = root->height.value;
    Glib::ustring doc_h_unit = Util::UnitTable::get().getUnit(root->height.unit)->abbr;
    percent = percent || doc_h_unit == "%";
    if (doc_h_unit == "") {
        doc_h_unit = "px";
    } else if (doc_h_unit == "%" && root->viewBox_set) {
        doc_h_unit = "px";
        doc_h = root->viewBox.height();
    }
    using UI::Widget::PageProperties;
    // dialog's behavior is not entirely correct when document sizes are expressed in '%', so put up a disclaimer
    _page->set_check(PageProperties::Check::UnsupportedSize, percent);

    _page->set_dimension(PageProperties::Dimension::PageSize, doc_w, doc_h);
    _page->set_unit(PageProperties::Units::Document, doc_w_unit);

    update_viewbox_ui(desktop);
    update_scale_ui(desktop);

    if (nv->display_units) {
        _page->set_unit(PageProperties::Units::Display, nv->display_units->abbr);
    }
    _page->set_check(PageProperties::Check::Checkerboard, nv->desk_checkerboard);
    _page->set_color(PageProperties::Color::Desk, nv->getDeskColor());
    _page->set_color(PageProperties::Color::Background, page_manager.getBackgroundColor());
    _page->set_check(PageProperties::Check::Border, page_manager.border_show);
    _page->set_check(PageProperties::Check::BorderOnTop, page_manager.border_on_top);
    _page->set_color(PageProperties::Color::Border, page_manager.getBorderColor());
    _page->set_check(PageProperties::Check::Shadow, page_manager.shadow_show);
    _page->set_check(PageProperties::Check::PageLabelStyle, page_manager.label_style != "default");
    _page->set_check(PageProperties::Check::AntiAlias, nv->antialias_rendering);
    _page->set_check(PageProperties::Check::ClipToPage, nv->clip_to_page);
    _page->set_check(PageProperties::Check::YAxisPointsDown, nv->is_y_axis_down());
    _page->set_check(PageProperties::Check::OriginCurrentPage, nv->get_origin_follows_page());

    //-----------------------------------------------------------guide page

    _guides_panel->update(nv);

    //-----------------------------------------------------------meta pages
    _metadata_panel->update(document);
    _licensor.update(document);

    _wr.setUpdating (false);
}

//--------------------------------------------------------------------

void DocumentProperties::on_response (int id)
{
    if (id == Gtk::ResponseType::CLOSE)
        set_visible(false);
}

void DocumentProperties::WatchConnection::connect(Inkscape::XML::Node *node)
{
    disconnect();
    if (!node) return;

    _node = node;
    _node->addObserver(*this);
}

void DocumentProperties::WatchConnection::disconnect() {
    if (_node) {
        _node->removeObserver(*this);
        _node = nullptr;
    }
}

void DocumentProperties::WatchConnection::notifyChildAdded(XML::Node&, XML::Node &child, XML::Node*)
{
    if (auto grid = cast<SPGrid>(_dialog->getDocument()->getObjectByRepr(&child))) {
        _dialog->add_grid_widget(grid);
    }
}

void DocumentProperties::WatchConnection::notifyChildRemoved(XML::Node&, XML::Node &child, XML::Node*)
{
    _dialog->remove_grid_widget(child);
}

void DocumentProperties::WatchConnection::notifyAttributeChanged(XML::Node&, GQuark, Util::ptr_shared, Util::ptr_shared)
{
    _dialog->update_widgets();
}

void DocumentProperties::documentReplaced()
{
    _root_connection.disconnect();
    _namedview_connection.disconnect();
    _scripting_panel->set_desktop(getDesktop());

    if (auto desktop = getDesktop()) {
        _wr.setDesktop(desktop);
        _namedview_connection.connect(desktop->getNamedView()->getRepr());
        auto document = desktop->getDocument();
        if (document) {
            _root_connection.connect(document->getRoot()->getRepr());
        }
        _cms_panel->set_document(document);
        _cms_panel->update(document);
        _scripting_panel->update(document);
        _metadata_panel->set_document(document);
        _licensor.set_document(document);
        update_widgets();
        rebuild_gridspage();
    }
}

void DocumentProperties::update()
{
    update_widgets();
}

/*########################################################################
# BUTTON CLICK HANDLERS    (callbacks)
########################################################################*/

void DocumentProperties::onNewGrid(GridType grid_type)
{
    auto desktop = getDesktop();
    auto document = getDocument();
    if (!desktop || !document) return;

    auto repr = desktop->getNamedView()->getRepr();
    SPGrid::create_new(document, repr, grid_type);
    // flip global switch, so snapping to grid works
    desktop->getNamedView()->newGridCreated();

    DocumentUndo::done(document, RC_("Undo", "Create new grid"), INKSCAPE_ICON("document-properties"));

    // scroll to the last (newly added) grid, so we can see it; postponed till idle time, since scrolling
    // range is not yet updated, despite new grid UI being in place already
    _on_idle_scroll = Glib::signal_idle().connect([this](){
        if (auto adj = _grids_wnd.get_vadjustment()) {
            adj->set_value(adj->get_upper());
        }
        return false;
    });
}

/* This should not effect anything in the SVG tree (other than "inkscape:document-units").
   This should only effect values displayed in the GUI. */
void DocumentProperties::display_unit_change(const Inkscape::Util::Unit* doc_unit)
{
    SPDocument *document = getDocument();
    // Don't execute when change is being undone
    if (!document || !DocumentUndo::getUndoSensitive(document)) {
        return;
    }
    // Don't execute when initializing widgets
    if (_wr.isUpdating()) {
        return;
    }

    auto action = document->getActionGroup()->lookup_action("set-display-unit");
    action->activate(doc_unit->abbr);
}

} // namespace Dialog

} // namespace Inkscape::UI

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
