// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_FONT_SELECTOR_INTERFACE_H
#define SEEN_FONT_SELECTOR_INTERFACE_H

#include <glibmm/ustring.h>
#include <sigc++/signal.h>

namespace Gtk { class Widget; }
namespace Inkscape { class LocalFontLister; }
namespace Inkscape::UI { class DefocusTarget; }

class FontSelectorInterface
{
public:
    virtual ~FontSelectorInterface() {};

    // get font selected in this FontList, if any
    virtual Glib::ustring get_fontspec() const = 0;
    virtual double get_fontsize() const = 0;

    // show requested font in a FontList
    virtual void set_current_font(const Glib::ustring& family, const Glib::ustring& face) = 0;
    // 
    virtual void set_current_size(double size) = 0;

    virtual sigc::signal<void ()>& signal_fontspec_changed() = 0;
    virtual sigc::signal<void ()>& signal_fontsize_changed() = 0;
    virtual sigc::signal<void ()>& signal_set_default() = 0;
    virtual sigc::signal<void (const Glib::ustring&)>& signal_insert_text() = 0;

    // get UI element
    virtual Gtk::Widget* box() = 0;

    // legacy font selector
    virtual void set_model(Inkscape::LocalFontLister &localfontlister) {};
    virtual void unset_model() {};

    // defocus handling
    virtual void setDefocusTarget(Inkscape::UI::DefocusTarget *) {};
};

#endif // SEEN_FONT_SELECTOR_INTERFACE_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
