// SPDX-License-Identifier: GPL-2.0-or-later
#include "item-factories.h"

#include <gtkmm/label.h>
#include <gtkmm/signallistitemfactory.h>

namespace Inkscape::UI {

Glib::RefPtr<Gtk::ListItemFactory> create_label_factory_untyped(std::function<Glib::ustring (Glib::ObjectBase const &)> get_text, bool use_markup)
{
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect([] (Glib::RefPtr<Gtk::ListItem> const &item) {
        auto label = Gtk::make_managed<Gtk::Label>();
        label->set_xalign(0);
        item->set_child(*label);
    });
    factory->signal_bind().connect([get_text = std::move(get_text), use_markup] (Glib::RefPtr<Gtk::ListItem> const &item) {
        auto &label = dynamic_cast<Gtk::Label &>(*item->get_child());
        auto const text = get_text(*item->get_item());
        if (use_markup) {
            label.set_markup(text);
        } else {
            label.set_text(text);
        }
    });
    return factory;
}

} // namespace Inkscape::UI
