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

#include "pages-panel.h"

#include <glibmm/i18n.h>

#include "document.h"
#include "document-undo.h"
#include "page-manager.h"
#include "ui/builder-utils.h"

using Inkscape::UI::create_builder;
using Inkscape::UI::get_widget;

namespace Inkscape::UI::Widget {

namespace {

void write_document_bool(SPDocument* doc,
                          Inkscape::Util::Internal::ContextString operation,
                          const char* key, bool on)
{
    if (!doc) return;

    // TODO: Implement actual document property writing
    // This would need to be implemented based on the specific document properties
    doc->setModifiedSinceSave();
    DocumentUndo::done(doc, operation, "");
}

} // namespace

PagesPropertiesPanel::PagesPropertiesPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 4)
    , _builder(create_builder("pages-panel.ui"))
    , _add_page_btn(get_widget<Gtk::Button>(_builder, "add_page_btn"))
    , _delete_page_btn(get_widget<Gtk::Button>(_builder, "delete_page_btn"))
    , _move_page_up_btn(get_widget<Gtk::Button>(_builder, "move_page_up_btn"))
    , _move_page_down_btn(get_widget<Gtk::Button>(_builder, "move_page_down_btn"))
    , _show_page_border_btn(get_widget<Gtk::CheckButton>(_builder, "show_page_border_btn"))
    , _show_page_shadow_btn(get_widget<Gtk::CheckButton>(_builder, "show_page_shadow_btn"))
    // , _enable_page_labels_btn(get_widget<Gtk::CheckButton>(_builder, "enable_page_labels_btn"))
{
    auto& grid = get_widget<Gtk::Grid>(_builder, "pages_grid");
    append(grid);

    // --- write-back signal handlers ---
    _add_page_btn.signal_clicked().connect([this]{
        if (_update.pending() || !_document) return;
        auto& page_manager = _document->getPageManager();
        // TODO: Implement add page functionality
        write_document_bool(_document, RC_("Undo", "Add page"), "add-page", true);
    });

    _delete_page_btn.signal_clicked().connect([this]{
        if (_update.pending() || !_document) return;
        auto& page_manager = _document->getPageManager();
        // TODO: Implement delete page functionality
        write_document_bool(_document, RC_("Undo", "Delete page"), "delete-page", true);
    });

    _move_page_up_btn.signal_clicked().connect([this]{
        if (_update.pending() || !_document) return;
        auto& page_manager = _document->getPageManager();
        // TODO: Implement move page up functionality
        write_document_bool(_document, RC_("Undo", "Move page up"), "move-page-up", true);
    });

    _move_page_down_btn.signal_clicked().connect([this]{
        if (_update.pending() || !_document) return;
        auto& page_manager = _document->getPageManager();
        // TODO: Implement move page down functionality
        write_document_bool(_document, RC_("Undo", "Move page down"), "move-page-down", true);
    });

    _show_page_border_btn.signal_toggled().connect([this]{
        if (_update.pending() || !_document) return;
        auto& page_manager = _document->getPageManager();
        // TODO: Implement show page border functionality
        write_document_bool(_document, RC_("Undo", "Toggle page border"), "show-page-border",
                           _show_page_border_btn.get_active());
    });

    _show_page_shadow_btn.signal_toggled().connect([this]{
        if (_update.pending() || !_document) return;
        auto& page_manager = _document->getPageManager();
        // TODO: Implement show page shadow functionality
        write_document_bool(_document, RC_("Undo", "Toggle page shadow"), "show-page-shadow",
                           _show_page_shadow_btn.get_active());
    });

    // _enable_page_labels_btn.signal_toggled().connect([this]{
    //     if (_update.pending() || !_document) return;
    //     auto& page_manager = _document->getPageManager();
    //     // TODO: Implement enable page labels functionality
    //     write_document_bool(_document, RC_("Undo", "Toggle page labels"), "enable-page-labels",
    //                        _enable_page_labels_btn.get_active());
    // });
}

void PagesPropertiesPanel::update(SPDocument* document) {
    _document = document;
    if (!document) return;

    auto scoped(_update.block());
    auto& page_manager = document->getPageManager();

    // TODO: Update UI based on current document state
    // For now, set some default values
    _show_page_border_btn.set_active(true);
    _show_page_shadow_btn.set_active(true);
    // _enable_page_labels_btn.set_active(false);

    // Enable/disable buttons based on page count
    bool has_pages = page_manager.getPageCount() > 0;
    bool multiple_pages = page_manager.getPageCount() > 1;

    _delete_page_btn.set_sensitive(has_pages);
    _move_page_up_btn.set_sensitive(multiple_pages);
    _move_page_down_btn.set_sensitive(multiple_pages);
}

} // namespace Inkscape::UI::Widget
