// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Per-document gadget for tracking the fonts and styles in use in a document.
 */
/*
 * Authors:
 *   PBS <pbs3141@gmail.com>
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UTIL_DOCUMENT_FONTS_H
#define INKSCAPE_UTIL_DOCUMENT_FONTS_H

#include <map>
#include <sigc++/connection.h>
#include <sigc++/signal.h>

#include "libnrtype/font-lister.h"
#include "ui/item-factories.h"

namespace Inkscape {

class DocumentFonts
{
public:
    using InnerMap = std::map<std::string, int>;
    using OuterMap = std::map<std::string, InnerMap>;
    using ListStore = Gio::ListStore<WrapAsGObject<FontLister::FontListItem>>;

    sigc::connection connectFamiliesChanged(sigc::slot<void ()> &&slot) {
        return _families_changed.connect(std::move(slot));
    }
    sigc::connection connectStylesChanged(sigc::slot<void ()> &&slot) {
        return _styles_changed.connect(std::move(slot));
    }

    using Handle = std::pair<OuterMap::iterator, InnerMap::iterator>;
    Handle insert(std::string const &family, std::string const &style);
    void remove(Handle handle);

    OuterMap const &getMap() const { return _map; }
    Glib::RefPtr<ListStore> getFamilies();

private:
    OuterMap _map;

    std::weak_ptr<ListStore> _store; // only exists while observed

    sigc::signal<void ()> _families_changed;
    sigc::signal<void ()> _styles_changed;

    std::shared_ptr<ListStore> _createStore() const;
};

} // namespace Inkscape

#endif // INKSCAPE_UTIL_DOCUMENT_FONTS_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
