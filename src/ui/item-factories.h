// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_ITEM_FACTORIES_H
#define INKSCAPE_UI_ITEM_FACTORIES_H

#include <concepts>
#include <glibmm/object.h>
#include <glibmm/ustring.h>
namespace Gtk { class ListItemFactory; }

namespace Inkscape {

/**
 * Helper for putting a C++ type inside a @a Gio::ListModel.
 */
template <typename T>
class WrapAsGObject : public Glib::Object
{
public:
    template <typename... Args>
    static Glib::RefPtr<WrapAsGObject> create(Args&&... args)
    {
        return Glib::make_refptr_for_instance(new WrapAsGObject(std::forward<Args>(args)...));
    }

    T data;

protected:
    template <typename... Args>
    WrapAsGObject(Args&&... args)
        : data(std::forward<Args>(args)...)
    {}
};

} // namespace Inkscape

namespace Inkscape::UI {

/**
 * Adapts a function that accepts @a T const & to accept @a Glib::ObjectBase const &,
 * assuming its argument has underlying type @a WrapAsGObject<T> and unwrapping it.
 */
template <typename T, typename F>
requires
    requires (T const &t, F const &f) {
        { f(t) };
    }
auto unwrap_arg_adaptor(F &&f)
{
    return [f = std::forward<F>(f)] (Glib::ObjectBase const &obj) -> decltype(auto) {
        return f(dynamic_cast<WrapAsGObject<T> const &>(obj).data);
    };
}

/**
 * Returns a @a Gtk::ListItemFactory that shows objects of any type as a @a Gtk::Label.
 * @param get_text The function to get the text for the label, given an object of type Glib::ObjectBase.
 */
Glib::RefPtr<Gtk::ListItemFactory> create_label_factory_untyped(std::function<Glib::ustring (Glib::ObjectBase const &)> get_text, bool use_markup = false);

/**
 * Returns a @a Gtk::ListItemFactory that shows objects of type T as a @a Gtk::Label.
 * @tparam T The C++ type of the objects in the listmodel, assumed to be wrapped using WrapAsGObject.
 * @param get_text The function to get the text for the label, given an object of type T.
 */
template <typename T, typename F>
requires
    requires (T const &t, F const &f) {
        { f(t) } -> std::convertible_to<Glib::ustring>;
    }
Glib::RefPtr<Gtk::ListItemFactory> create_label_factory(F &&get_text, bool use_markup = false)
{
    return create_label_factory_untyped(unwrap_arg_adaptor<T>(std::forward<F>(get_text)), use_markup);
}

} // namespace Inkscape::UI

#endif // INKSCAPE_UI_ITEM_FACTORIES_H
