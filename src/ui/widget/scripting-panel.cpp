// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Scripting panel — external and embedded SVG scripts.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "scripting-panel.h"

#include <gtkmm/grid.h>
#include <ranges>
#include <glibmm/i18n.h>

#include "document.h"
#include "document-undo.h"
#include "object/sp-script.h"
#include "xml/document.h"
#include "xml/node.h"
#include "ui/builder-utils.h"

using Inkscape::UI::create_builder;
using Inkscape::UI::get_widget;

namespace Inkscape::UI::Widget {

ScriptingPanel::ScriptingPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , _builder(create_builder("scripting-panel.ui"))
    , _tab_strip(get_widget<Inkscape::UI::Widget::TabStrip>(_builder, "tab_strip"))
    , _stack(get_widget<Gtk::Stack>(_builder, "stack"))
    , _external_page(get_widget<Gtk::Box>(_builder, "external_page"))
    , _embedded_page(get_widget<Gtk::Box>(_builder, "embedded_page"))
    , _external_label(get_widget<Gtk::Label>(_builder, "external_label"))
    , _external_scroller(get_widget<Gtk::ScrolledWindow>(_builder, "external_scroller"))
    , _external_view(get_widget<Gtk::TreeView>(_builder, "external_view"))
    , _external_entry(get_widget<Gtk::Entry>(_builder, "external_entry"))
    , _external_add_btn(get_widget<Gtk::Button>(_builder, "external_add_btn"))
    , _external_remove_btn(get_widget<Gtk::Button>(_builder, "external_remove_btn"))
    , _embedded_label(get_widget<Gtk::Label>(_builder, "embedded_label"))
    , _embedded_scroller(get_widget<Gtk::ScrolledWindow>(_builder, "embedded_scroller"))
    , _embedded_view(get_widget<Gtk::TreeView>(_builder, "embedded_view"))
    , _content_view(get_widget<Gtk::TextView>(_builder, "content_view"))
    , _content_scroller(get_widget<Gtk::ScrolledWindow>(_builder, "content_scroller"))
    , _embedded_new_btn(get_widget<Gtk::Button>(_builder, "embedded_new_btn"))
    , _embedded_remove_btn(get_widget<Gtk::Button>(_builder, "embedded_remove_btn"))
{
    // Add tabs to the TabStrip
    auto external_tab = _tab_strip.add_tab(_("External scripts"), "script-external", 0);
    auto embedded_tab = _tab_strip.add_tab(_("Embedded scripts"), "script-internal", 1);

    // Set up tab switching
    _tab_strip.signal_select_tab().connect([this](Gtk::Widget& tab) {
        _tab_strip.select_tab(tab);
        int page = _tab_strip.get_tab_position(tab);
        _stack.set_visible_child(page == 0 ? _external_page : _embedded_page);
    });

    // Select the first tab by default
    _tab_strip.select_tab_at(0);
    // Setup external scripts tree view
    _ext_store = Gtk::ListStore::create(_ext_cols);
    _external_view.set_model(_ext_store);
    _external_view.append_column(_("Filename"), _ext_cols.filename);
    _external_view.set_headers_visible(true);

    // Setup embedded scripts tree view
    _emb_store = Gtk::ListStore::create(_emb_cols);
    _embedded_view.set_model(_emb_store);
    _embedded_view.append_column(_("Script ID"), _emb_cols.id);
    _embedded_view.set_headers_visible(true);

    // Set TextView as child of content scroller
    _content_scroller.set_child(_content_view);

    append(get_widget<Gtk::Grid>(_builder, "main-grid"));

    // --- Signal connections ---
    _external_add_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &ScriptingPanel::add_external_script));
    _external_remove_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &ScriptingPanel::remove_external_script));
    _embedded_new_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &ScriptingPanel::add_embedded_script));
    _embedded_remove_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &ScriptingPanel::remove_embedded_script));
    _external_view.get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &ScriptingPanel::on_external_script_selected));
    _embedded_view.get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &ScriptingPanel::on_embedded_script_selected));
    _embedded_view.signal_cursor_changed().connect(
        sigc::mem_fun(*this, &ScriptingPanel::on_embedded_cursor_changed));
    _content_view.get_buffer()->signal_changed().connect(
        sigc::mem_fun(*this, &ScriptingPanel::edit_embedded_script));

    _scripts_observer.signal_changed().connect([this](auto, auto) {
        populate_lists(_document);
    });

    on_external_script_selected();
    on_embedded_script_selected();
}

ScriptingPanel::~ScriptingPanel() {
    // clear document, so callbacks that fire during destruction exit early
    _document = nullptr;
}

void ScriptingPanel::update(SPDocument* document) {
    _document = document;
    populate_lists(document);
}

void ScriptingPanel::populate_lists(SPDocument* document) {
    _ext_store->clear();
    _emb_store->clear();
    if (!document) return;

    auto current = document->getResourceList("script");
    for (auto obj : current) {
        auto script = cast<SPScript>(obj);
        if (!script) continue;
        if (script->xlinkhref) {
            auto row = *(_ext_store->append());
            row[_ext_cols.filename] = script->xlinkhref;
        } else {
            auto row = *(_emb_store->append());
            row[_emb_cols.id] = obj->getId() ? obj->getId() : "";
        }
    }
}

void ScriptingPanel::add_external_script() {
    if (!_document) return;

    auto text = _external_entry.get_text();
    if (text.empty()) return;

    auto repr = _document->getReprDoc()->createElement("svg:script");
    repr->setAttribute("xlink:href", text.c_str());
    _document->getReprRoot()->appendChild(repr);
    Inkscape::GC::release(repr);
    DocumentUndo::done(_document, RC_("Undo", "Add external script"), "");
    populate_lists(_document);
}

void ScriptingPanel::remove_external_script() {
    if (!_document) return;

    Glib::ustring filename;
    if (auto sel = _external_view.get_selection()) {
        auto it = sel->get_selected();
        if (it) filename = (*it)[_ext_cols.filename];
    }
    if (filename.empty()) return;

    for (auto obj : _document->getResourceList("script")) {
        auto script = cast<SPScript>(obj);
        if (script && script->xlinkhref && filename == script->xlinkhref) {
            obj->deleteObject();
            break;
        }
    }
    DocumentUndo::done(_document, RC_("Undo", "Remove external script"), "");
    populate_lists(_document);
}

void ScriptingPanel::add_embedded_script() {
    if (!_document) return;

    auto repr = _document->getReprDoc()->createElement("svg:script");
    _document->getReprRoot()->appendChild(repr);
    Inkscape::GC::release(repr);
    DocumentUndo::done(_document, RC_("Undo", "Add embedded script"), "");
    populate_lists(_document);
}

void ScriptingPanel::remove_embedded_script() {
    if (!_document) return;

    Glib::ustring id;
    if (auto sel = _embedded_view.get_selection()) {
        auto it = sel->get_selected();
        if (it) id = (*it)[_emb_cols.id];
    }
    if (id.empty()) return;

    for (auto obj : _document->getResourceList("script")) {
        if (id == obj->getId()) {
            obj->deleteObject();
            break;
        }
    }
    DocumentUndo::done(_document, RC_("Undo", "Remove embedded script"), "");
    populate_lists(_document);
}

void ScriptingPanel::on_external_script_selected() {
    bool has = _external_view.get_selection() &&
               _external_view.get_selection()->count_selected_rows() > 0;
    _external_remove_btn.set_sensitive(has);
}

void ScriptingPanel::on_embedded_script_selected() {
    bool has = _embedded_view.get_selection() &&
               _embedded_view.get_selection()->count_selected_rows() > 0;
    _embedded_remove_btn.set_sensitive(has);
}

void ScriptingPanel::on_embedded_cursor_changed() {
    if (!_document) return;

    Glib::ustring id;
    if (auto sel = _embedded_view.get_selection()) {
        auto it = sel->get_selected();
        if (it) id = (*it)[_emb_cols.id];
    }
    if (id.empty()) { _content_view.get_buffer()->set_text(""); return; }

    bool found = false;
    for (auto obj : _document->getResourceList("script")) {
        if (id == obj->getId()) {
            auto child = obj->firstChild();
            if (child && child->getRepr()) {
                if (auto const* content = child->getRepr()->content()) {
                    _content_view.get_buffer()->set_text(content);
                    found = true;
                }
            }
            break;
        }
    }
    if (!found) _content_view.get_buffer()->set_text("");
}

void ScriptingPanel::edit_embedded_script() {
    if (!_document) return;

    Glib::ustring id;
    if (auto sel = _embedded_view.get_selection()) {
        auto it = sel->get_selected();
        if (it) id = (*it)[_emb_cols.id];
    }
    if (id.empty()) return;

    for (auto obj : _document->getResourceList("script")) {
        if (id == obj->getId()) {
            if (auto repr = obj->getRepr()) {
                auto tmp = obj->children | std::views::transform([](SPObject& o) { return &o; });
                std::vector<SPObject*> vec(tmp.begin(), tmp.end());
                for (auto child : vec) child->deleteObject();
                obj->appendChildRepr(_document->getReprDoc()->createTextNode(
                    _content_view.get_buffer()->get_text().c_str()));
                DocumentUndo::done(_document, RC_("Undo", "Edit embedded script"), "");
            }
            break;
        }
    }
}

} // namespace Inkscape::UI::Widget
