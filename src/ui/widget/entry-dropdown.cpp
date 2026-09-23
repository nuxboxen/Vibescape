// SPDX-License-Identifier: GPL-2.0-or-later
#include "entry-dropdown.h"

#include <gtkmm/binlayout.h>
#include <gtkmm/eventcontrollerkey.h>

#include "ui/containerize.h"
#include "ui/defocus-target.h"
#include "ui/item-factories.h"
#include "ui/tools/tool-base.h"

namespace Inkscape::UI::Widget {

static bool registered_type = false;

EntryDropDown::EntryDropDown()
    : Glib::ObjectBase{"EntryDropDown"}
    , CssNameClassInit{"dropdown"}
{
    _construct();
    registered_type = true;
}

EntryDropDown::EntryDropDown(BaseObjectType *cobject, Glib::RefPtr<Gtk::Builder> const &)
    : Glib::ObjectBase{"EntryDropDown"}
    , CssNameClassInit{"dropdown"}
    , Gtk::Widget{cobject}
{
    _construct();
    registered_type = true;
}

void EntryDropDown::register_type()
{
    // This workaround, both required and recommended by GTKmm, is offensively wasteful.
    if (!registered_type) {
        EntryDropDown{}; // construct and throw away object
    }
}

void EntryDropDown::_construct()
{
    containerize(*this);
    set_layout_manager(Gtk::BinLayout::create());

    _selection_model = Gtk::SingleSelection::create();
    _selection_model->set_can_unselect(true);

    // Note: It's important that *this != _box, otherwise _popover interferes with "linked".
    _box.add_css_class("linked");
    _box.set_parent(*this);
    _box.append(_entry);
    _box.append(_button);

    _button.set_image_from_icon_name("pan-down");

    _scroll.set_child(_view);
    _scroll.property_propagate_natural_height().set_value(true);
    _scroll.property_hscrollbar_policy().set_value(Gtk::PolicyType::NEVER);
    _scroll.set_max_content_height(400);

    _popover.add_css_class("menu");
    _popover.set_child(_scroll);
    _popover.set_has_arrow(false);
    _popover.set_halign(Gtk::Align::START);
    _popover.set_parent(*this);

    _view.set_model(_selection_model);
    _view.set_single_click_activate();

    _button.signal_clicked().connect([this] {
        _popover.property_width_request().set_value(_box.get_width());
        _popover.popup();
        _box.grab_focus();
        _resolveSelection();
        if (auto const item = std::get_if<ListItem>(&_selection)) {
            _view.scroll_to(item->pos, Gtk::ListScrollFlags::FOCUS | Gtk::ListScrollFlags::SELECT);
        } else {
            _selection_model->set_selected(GTK_INVALID_LIST_POSITION);
        }
    });

    _popover.signal_closed().connect([this] {
        _button.set_active(false);
        _defocus();
    });

    _view.signal_activate().connect([this] (unsigned pos) {
        _setSelection(pos);
        _popover.popdown();
    });

    _entry.signal_activate().connect([this] {
        if (_text != _entry.get_text()) {
            _selection = Tbd{};
            _text = _entry.get_text();
            _changed_signal.emit();
        } else {
            _defocus();
        }
    });

    _entry.property_has_focus().signal_changed().connect([this] {
        if (!_entry.property_has_focus()) {
            _entry.set_text(_text);
        }
    });

    auto key = Gtk::EventControllerKey::create();
    key->signal_key_pressed().connect([this, &key = *key] (auto &&...args) { return _onKeyPressed(key, args...); }, false);
    add_controller(key);
}

void EntryDropDown::setModel(Glib::RefPtr<Gio::ListModel> model)
{
    _model = std::move(model);
    _model_conn = _model->signal_items_changed().connect([this] (auto... args) { _itemsChanged(args...); });
    _dict = {};
    _selection = Tbd{};
    _selection_model->set_model(_model);
    _changed_signal.emit();
}

void EntryDropDown::setStringFuncAndFactory(StringFunc string_func)
{
    setFactory(create_label_factory_untyped(string_func));
    setStringFunc(std::move(string_func));
}

void EntryDropDown::setText(Glib::ustring text)
{
    if (text != _text) {
        _text = std::move(text);
        _updateEntry();
        _selection = Tbd{};
        _changed_signal.emit();
    }
}

void EntryDropDown::setSelectedPos(unsigned pos)
{
    assert(0 <= pos && pos < _model->get_n_items());
    _setSelection(pos);
}

std::optional<unsigned> EntryDropDown::getSelectedPos() const
{
    if (auto const item = std::get_if<ListItem>(&_selection)) {
        if (item->origin == Origin::Picked) {
            return item->pos;
        }
    }
    return {};
}

Glib::RefPtr<Glib::ObjectBase> EntryDropDown::getSelectedItem() const
{
    if (auto const pos = getSelectedPos()) {
        return _model->get_object(*pos);
    }
    return {};
}

std::optional<unsigned> EntryDropDown::lookupText(Glib::ustring const &text)
{
    if (!_dict) {
        _rebuildDict();
    }
    if (auto const it = _dict->find(_text.collate_key()); it != _dict->end()) {
        return it->second;
    }
    return {};
}

void EntryDropDown::_rebuildDict()
{
    _dict.emplace();

    auto const size = _model->get_n_items();
    for (int i = 0; i < size; i++) {
        // Give earlier entries priority when there are duplicate entries.
        _dict->try_emplace(_string_func(*_model->get_object(i)).collate_key(), i);
    }
}

void EntryDropDown::_itemsChanged(unsigned pos, unsigned removed, unsigned added)
{
    _dict = {};

    if (auto const item = std::get_if<ListItem>(&_selection)) {
        if (pos <= item->pos) {
            if (item->pos < pos + removed) {
                _selection = Tbd{};
                _changed_signal.emit();
            } else {
                item->pos += added - removed;
            }
        }
    }
}

void EntryDropDown::_resolveSelection()
{
    if (!std::holds_alternative<Tbd>(_selection)) {
        return;
    }

    if (!_dict) {
        _rebuildDict();
    }

    if (auto const it = _dict->find(_text.collate_key()); it != _dict->end()) {
        _selection = ListItem{ .pos = it->second, .origin = Origin::Matched };
    } else {
        _selection = TextOnly{};
    }
}

void EntryDropDown::_updateEntry()
{
    _entry.set_text(_text);
    _entry.select_region(_text.length(), _text.length()); // Fixme: Only if has focus.
}

void EntryDropDown::_setSelection(unsigned pos)
{
    _selection = ListItem{.pos = pos, .origin = Origin::Picked};
    _text = _string_func(*_model->get_object(pos));
    if (_entry.get_text() != _text) {
        _updateEntry();
    }
    _changed_signal.emit();
}

void EntryDropDown::_defocus()
{
    if (_defocus_target) {
        _entry.select_region(0, 0); // clear selection, which would otherwise persist
        _defocus_target->onDefocus(); // pass focus to canvas
    } else {
        _entry.grab_focus();
    }
}

bool EntryDropDown::_onKeyPressed(Gtk::EventControllerKey const &controller, unsigned keyval, unsigned keycode, Gdk::ModifierType state)
{
    auto shift = [this] (int diff) {
        _resolveSelection();
        if (auto const item = std::get_if<ListItem>(&_selection)) {
            auto const pos = static_cast<int>(item->pos) + diff; // don't use unsigned types for arithmetic
            if (0 <= pos && pos < _model->get_n_items()) {
                _setSelection(pos);
            }
        }
    };

    switch (Inkscape::UI::Tools::get_latin_keyval(controller, keyval, keycode, state)) {
        case GDK_KEY_Escape:
            if (_entry.get_text() != _text) {
                _updateEntry();
            }
            _defocus();
            return true;

        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            shift(-1);
            return true;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            shift(1);
            return true;

        default:
            break;
    }

    return false;
}

} // namespace Inkscape::UI::Widget
