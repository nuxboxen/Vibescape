// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Widget for specifying a document's license; part of document
 * preferences dialog.
 */
/*
 * Authors:
 *   bulia byak <buliabyak@users.sf.net>
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon Phillips <jon@rejon.org>
 *   Ralf Stephan <ralf@ark.in-berlin.de> (Gtkmm)
 *   Abhishek Sharma
 *   Mike Kowalski
 *
 * Copyright (C) 2000 - 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "licensor.h"

#include <gtkmm/entry.h>
#include <gtkmm/dropdown.h>

#include "document.h"
#include "document-undo.h"
#include "rdf.h"
#include "ui/builder-utils.h"
#include "ui/operation-blocker.h"
#include "ui/widget/drop-down-list.h"

using Inkscape::UI::create_builder;
using Inkscape::UI::get_widget;

namespace Inkscape::UI::Widget {

const struct rdf_license_t _proprietary_license =
  {_("Proprietary"), "", nullptr};

const struct rdf_license_t _other_license =
  {Q_("MetadataLicence|Other"), "", nullptr};

Licensor::Licensor()
: Gtk::Box{Gtk::Orientation::VERTICAL, 4}
{
    auto builder = create_builder("licensor.ui");
    auto& grid = get_widget<Gtk::Grid>(builder, "licensor_grid");
    _license_dropdown = &get_widget<Gtk::DropDown>(builder, "license_dropdown");
    _uri_label = &get_widget<Gtk::Label>(builder, "uri_label");
    _uri_entry = &get_widget<Gtk::Entry>(builder, "uri_entry");

    append(grid);
}

Licensor::~Licensor() = default;

void Licensor::init() {
    auto scoped(_update.block());

    _licenses.clear();
    _licenses.push_back(&_proprietary_license);

    for (auto license = rdf_licenses; license && license->name; ++license) {
        _licenses.push_back(license);
    }

    _licenses.push_back(&_other_license);

    // Create string list for dropdown
    auto string_list = Gtk::StringList::create({});
    for (auto license : _licenses) {
        string_list->append(_(license->name));
    }
    _license_dropdown->set_model(string_list);
    _license_dropdown->set_selected(0);

    _uri_entry->set_hexpand();

    _license_dropdown->property_selected().signal_changed().connect([this] {
        on_license_changed();
    });
    _uri_entry->signal_changed().connect(sigc::mem_fun(*this, &Licensor::on_uri_changed));
}

void Licensor::set_document(SPDocument* document) {
    _document = document;
}

const rdf_license_t* Licensor::get_selected_license() const {
    if (!_license_dropdown) return nullptr;
    auto selected = _license_dropdown->get_selected();
    if (selected >= _licenses.size()) return nullptr;
    return _licenses[selected];
}

void Licensor::set_uri_text(const char* text) {
    auto scoped(_update.block());
    _uri_entry->set_text(text ? text : "");
}

void Licensor::on_license_changed() {
    if (_update.pending() || !_document) return;

    const auto license = get_selected_license();
    if (!license) return;

    auto scoped(_update.block());
    rdf_set_license(_document, license->details ? license : nullptr);
    if (_document->isSensitive()) {
        DocumentUndo::done(_document, RC_("Undo", "Document license updated"), "");
    }

    set_uri_text(license->uri);
    rdf_work_entity_t *entity = rdf_find_entity("license_uri");
    if (!entity) return;
    rdf_set_work_entity(_document, entity, _uri_entry->get_text().c_str());
}

void Licensor::on_uri_changed() {
    if (_update.pending() || !_document) return;

    rdf_work_entity_t *entity = rdf_find_entity("license_uri");
    if (!entity) return;

    auto scoped(_update.block());
    if (rdf_set_work_entity(_document, entity, _uri_entry->get_text().c_str()) && _document->isSensitive()) {
        DocumentUndo::done(_document, RC_("Undo", "Document metadata updated"), "");
    }
}

void Licensor::update(SPDocument* doc) {
    if (!doc || !_license_dropdown || _licenses.empty()) return;

    constexpr bool read_only = false;
    const auto license = rdf_get_license(doc, read_only);

    auto it = std::find(_licenses.begin(), _licenses.end(), license);
    if (it == _licenses.end()) {
        it = _licenses.begin();
    }

    auto scoped(_update.block());
    _license_dropdown->set_selected(std::distance(_licenses.begin(), it));

    auto entity = rdf_find_entity("license_uri");
    const auto text = entity ? rdf_get_work_entity(doc, entity) : nullptr;
    set_uri_text(text);
}

} // namespace Inkscape::UI::Widget

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
