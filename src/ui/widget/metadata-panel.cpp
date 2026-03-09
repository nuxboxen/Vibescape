// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Metadata panel — Dublin Core RDF entities.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "metadata-panel.h"

#include <cstring>
#include <glibmm/i18n.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/object.h>
#include <gtkmm/scrolledwindow.h>

#include "document.h"
#include "document-undo.h"
#include "preferences.h"
#include "rdf.h"
#include "streq.h"
#include "object/sp-root.h"

namespace Inkscape::UI::Widget {

MetadataPanel::MetadataPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
{
    _grid.set_row_spacing(4);
    _grid.set_column_spacing(4);
    append(_grid);

    // Populate entity rows — entries are built once; update() refreshes values.
    int row = 0;
    for (auto entity = rdf_work_entities; entity && entity->name; ++entity) {
        if (entity->editable != RDF_EDIT_GENERIC) continue;

        Row entry_row;
        entry_row.entity = entity;

        auto label = Gtk::make_managed<Gtk::Label>(Glib::ustring(_(entity->title)), Gtk::Align::END);
        label->set_halign(Gtk::Align::START);
        label->set_valign(Gtk::Align::CENTER);
        _grid.attach(*label, 0, row, 1, 1);
        entry_row.label = label;

        if (entity->format == RDF_FORMAT_MULTILINE) {
            auto scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
            scroller->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
            scroller->set_has_frame(true);
            scroller->set_hexpand();

            auto textview = Gtk::make_managed<Gtk::TextView>();
            textview->set_size_request(-1, 35);
            textview->set_wrap_mode(Gtk::WrapMode::WORD);
            textview->set_accepts_tab(false);
            textview->set_tooltip_text(_(entity->tip));
            scroller->set_child(*textview);

            scroller->set_valign(Gtk::Align::CENTER);
            // let "description" field grow taller than other fields; it's the one that typically needs more space
            if (streq(entity->name, "description")) {
                scroller->set_valign(Gtk::Align::FILL);
                scroller->set_vexpand();
            }

            _grid.attach(*scroller, 1, row, 2, 1);

            entry_row.editor = scroller;
            entry_row.scroller = scroller;
            entry_row.textview = textview;

            textview->get_buffer()->signal_changed().connect([this, entity, textview] {
                on_text_changed(entity, textview);
            });
        } else {
            auto entry = Gtk::make_managed<Gtk::Entry>();
            entry->set_tooltip_text(_(entity->tip));
            entry->set_hexpand();
            entry->set_valign(Gtk::Align::CENTER);
            _grid.attach(*entry, 1, row, 2, 1);

            entry_row.editor = entry;
            entry_row.entry = entry;

            entry->signal_changed().connect([this, entity, entry] {
                on_entry_changed(entity, entry);
            });
        }

        _rows.push_back(entry_row);
        ++row;
    }

    // load/save default metadata
    auto label = Gtk::make_managed<Gtk::Label>(_("Defaults"));
    label->set_halign(Gtk::Align::START);
    _grid.attach(*label, 0, row);
    auto const btn_save = Gtk::make_managed<Gtk::Button>(_("_Save"), true);
    btn_save->set_tooltip_text(_("Save this metadata as the default metadata"));
    btn_save->set_hexpand();
    auto const btn_load = Gtk::make_managed<Gtk::Button>(_("Load"), true);
    btn_load->set_tooltip_text(_("Use the previously saved default metadata here"));
    btn_load->set_hexpand();

    _grid.attach(*btn_load, 1, row);
    _grid.attach(*btn_save, 2, row);

    btn_save->signal_clicked().connect([this]{ save_default(); });
    btn_load->signal_clicked().connect([this]{ load_default(); });
}

void MetadataPanel::set_document(SPDocument* document) {
    _document = document;
}

void MetadataPanel::update(SPDocument* document) {
    _document = document;
    if (!document) return;

    for (auto& row : _rows) {
        update_row(row, document);
    }
}

void MetadataPanel::save_default() {
    if (!_document) return;

    for (auto const& row : _rows) {
        save_to_preferences(row);
    }
}

void MetadataPanel::load_default() {
    for (auto const& row : _rows) {
        load_from_preferences(row);
    }
}

void MetadataPanel::on_entry_changed(rdf_work_entity_t* entity, Gtk::Entry* entry) {
    if (_update.pending() || !_document) return;

    auto scoped(_update.block());

    auto text = entry->get_text();
    if (rdf_set_work_entity(_document, entity, text.c_str()) && _document->isSensitive()) {
        DocumentUndo::done(_document, RC_("Undo", "Document metadata updated"), "");
    }
}

void MetadataPanel::on_text_changed(rdf_work_entity_t* entity, Gtk::TextView* textview) {
    if (_update.pending() || !_document) return;

    auto scoped(_update.block());

    auto text = textview->get_buffer()->get_text();
    if (rdf_set_work_entity(_document, entity, text.c_str())) {
        DocumentUndo::done(_document, RC_("Undo", "Document metadata updated"), "");
    }
}

void MetadataPanel::save_to_preferences(const Row& row) {
    if (!_document || !row.entity) return;

    auto prefs = Inkscape::Preferences::get();
    auto const* text = rdf_get_work_entity(_document, row.entity);
    prefs->setString(PREFS_METADATA + Glib::ustring(row.entity->name), Glib::ustring(text ? text : ""));
}

void MetadataPanel::load_from_preferences(const Row& row) {
    if (!row.entity) return;

    auto scoped(_update.block());

    auto prefs = Inkscape::Preferences::get();
    auto text = prefs->getString(PREFS_METADATA + Glib::ustring(row.entity->name));
    if (text.empty()) return;

    if (row.entry) {
        row.entry->set_text(text);
        return;
    }

    if (row.textview) {
        row.textview->get_buffer()->set_text(text);
    }
}

void MetadataPanel::update_row(Row& row, SPDocument* document) {
    if (!row.entity || !document) return;

    // "title" can also come from the document <title> element
    auto const* text = rdf_get_work_entity(document, row.entity);
    if (!text && std::strcmp(row.entity->name, "title") == 0 && document->getRoot()) {
        text = document->getRoot()->title();
    }

    if (row.entry) {
        row.entry->set_text(text ? text : "");
    }
    else if (row.textview) {
        row.textview->get_buffer()->set_text(text ? text : "");
    }
}

} // namespace Inkscape::UI::Widget
