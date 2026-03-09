// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Metadata panel — Dublin Core RDF entities.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_METADATA_PANEL_H
#define INKSCAPE_UI_WIDGET_METADATA_PANEL_H

#include <vector>
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>

#include "ui/operation-blocker.h"

class SPDocument;
struct rdf_work_entity_t;

namespace Inkscape::UI::Widget {

/**
 * Passive panel showing Dublin Core RDF entities for the document.
 */
class MetadataPanel : public Gtk::Box {
public:
    MetadataPanel();
    ~MetadataPanel() override = default;

    void set_document(SPDocument* document);
    void update(SPDocument* document);

private:
    struct Row {
        rdf_work_entity_t* entity = nullptr;
        Gtk::Label* label = nullptr;
        Gtk::Widget* editor = nullptr;
        Gtk::Entry* entry = nullptr;
        Gtk::ScrolledWindow* scroller = nullptr;
        Gtk::TextView* textview = nullptr;
    };

    void on_entry_changed(rdf_work_entity_t* entity, Gtk::Entry* entry);
    void on_text_changed(rdf_work_entity_t* entity, Gtk::TextView* textview);
    void save_to_preferences(const Row& row);
    void load_from_preferences(const Row& row);
    void update_row(Row& row, SPDocument* document);

    void save_default();
    void load_default();

    SPDocument*      _document = nullptr;
    OperationBlocker _update;
    Gtk::Grid        _grid;

    std::vector<Row> _rows;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_METADATA_PANEL_H
