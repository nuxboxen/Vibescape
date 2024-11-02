// SPDX-License-Identifier: GPL-2.0-or-later

#include "document-fonts.h"

namespace Inkscape {

DocumentFonts::Handle DocumentFonts::insert(std::string const &family, std::string const &style)
{
    auto const [outer, outer_inserted] = _map.try_emplace(family);
    auto const [inner, inner_inserted] = outer->second.try_emplace(style);

    inner->second++;

    if (outer_inserted) {
        _families_changed.emit();

        if (auto const store = _store.lock()) {
            store->append(WrapAsGObject<FontLister::FontListItem>::create(FontLister::FontListItem{
                .family = family,
                .on_system = false,
            }));
        }
    }
    if (inner_inserted) {
        _styles_changed.emit();
    }

    return {outer, inner};
}

void DocumentFonts::remove(Handle handle)
{
    auto const [outer, inner] = handle;

    inner->second--;

    if (inner->second == 0) {
        outer->second.erase(inner);

        if (outer->second.empty()) {
            if (auto const store = _store.lock()) {
                auto const size = store->get_n_items();
                for (int i = 0; i < size; i++) {
                    if (store->get_item(i)->data.family.raw() == outer->first) {
                        store->remove(i);
                        break;
                    }
                }
            }

            _map.erase(outer);
            _families_changed.emit();
        }

        _styles_changed.emit();
    }
}

Glib::RefPtr<DocumentFonts::ListStore> DocumentFonts::getFamilies()
{
    auto store = _store.lock();
    if (!store) {
        _store = store = _createStore();
    }
    return store;
}

std::shared_ptr<DocumentFonts::ListStore> DocumentFonts::_createStore() const
{
    auto store = ListStore::create();

    for (auto const &[family, _] : _map) {
        store->append(WrapAsGObject<FontLister::FontListItem>::create(FontLister::FontListItem{
            .family = family,
            .on_system = false,
        }));
        // Todo: Link back styles to here to allow lazy-loading.
    }

    return store;
}

} // namespace Inkscape

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
