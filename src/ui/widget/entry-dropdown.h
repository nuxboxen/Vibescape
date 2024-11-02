// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_WIDGET_ENTRY_DROPDOWN_H
#define INKSCAPE_UI_WIDGET_ENTRY_DROPDOWN_H

#include <variant>
#include <sigc++/signal.h>
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/listview.h>
#include <gtkmm/popover.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/singleselection.h>
#include <gtkmm/togglebutton.h>
#include "ui/widget/generic/css-name-class-init.h"

namespace Gio { class ListModel; }
namespace Gtk {
class Builder;
class ListItemFactory;
class EventControllerKey;
} // namespace Gtk

namespace Inkscape::UI { class DefocusTarget; }

namespace Inkscape::UI::Widget {

class EntryDropDown
    : public CssNameClassInit
    , public Gtk::Widget
{
public:
    using StringFunc = std::function<Glib::ustring (Glib::ObjectBase const &)>;

    EntryDropDown();
    EntryDropDown(BaseObjectType *cobject, Glib::RefPtr<Gtk::Builder> const &);
    static void register_type();

    void setModel(Glib::RefPtr<Gio::ListModel> model);
    void setFactory(Glib::RefPtr<Gtk::ListItemFactory> const &factory) { _view.set_factory(factory); }
    void setStringFunc(StringFunc string_func) { _string_func = std::move(string_func); }

    // Convenience function: Set both the string function and a label factory using it.
    void setStringFuncAndFactory(StringFunc string_func);

    void setWidthChars(int n_chars) { _entry.set_width_chars(n_chars); }
    void setMaxWidthChars(int n_chars) { _entry.set_max_width_chars(n_chars); }
    void setDefocusTarget(DefocusTarget *defocus_target) { _defocus_target = defocus_target; }

    void setText(Glib::ustring text);
    Glib::ustring const &getText() const { return _text; }
    void setSelectedPos(unsigned pos);
    std::optional<unsigned> getSelectedPos() const;
    Glib::RefPtr<Glib::ObjectBase> getSelectedItem() const;

    sigc::connection connectChanged(sigc::slot<void ()> &&slot) { return _changed_signal.connect(std::move(slot)); }

private:
    void _construct();

    Gtk::Box _box;
    Gtk::Entry _entry;
    Gtk::ToggleButton _button;
    Gtk::Popover _popover;
    Gtk::ScrolledWindow _scroll;
    Gtk::ListView _view;

    Glib::RefPtr<Gio::ListModel> _model;
    Glib::RefPtr<Gtk::SingleSelection> _selection_model;

    StringFunc _string_func;

    Inkscape::UI::DefocusTarget *_defocus_target = nullptr;

    sigc::signal<void ()> _changed_signal;

    enum class Origin { Picked, Matched };
    struct Tbd {};
    struct TextOnly {};
    struct ListItem { unsigned pos; Origin origin; };
    Glib::ustring _text;
    std::variant<Tbd, TextOnly, ListItem> _selection;
    std::optional<std::unordered_map<std::string, unsigned>> _dict;
    sigc::scoped_connection _model_conn;
    void _rebuildDict();
    void _itemsChanged(unsigned pos, unsigned removed, unsigned added);
    void _resolveSelection();
    void _updateEntry();
    void _setSelection(unsigned pos);

    void _defocus();
    bool _onKeyPressed(Gtk::EventControllerKey const &controller, unsigned keyval, unsigned keycode, Gdk::ModifierType state);
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_ENTRY_DROPDOWN_H
