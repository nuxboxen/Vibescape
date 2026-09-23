// SPDX-License-Identifier: GPL-2.0-or-later

#include "sp-text-item.h"

#include "document.h"
#include "style-text.h" // ink_font_description_from_style()

void SPTextItem::update(SPCtx *ctx, unsigned flags)
{
    SPItem::update(ctx, flags);

    if (document && (flags & SP_OBJECT_STYLE_MODIFIED_FLAG)) {
        auto &document_fonts = document->getDocumentFonts();

        auto descr = ink_font_description_from_style(style);
        auto const font_family = descr.get_family();
        if (!font_family.empty()) {
            descr.unset_fields(Pango::FontMask::FAMILY);

            // Add new before removing old to reduce churn.
            auto const handle = document_fonts.insert(font_family, descr.to_string());
            if (_handle) {
                document_fonts.remove(*_handle);
            }
            _handle = handle;
        } else {
            std::cerr << "getFamilyAndStyle: descr without font family! " << (getId() ? getId() : "null") << std::endl;

            if (_handle) {
                document_fonts.remove(*_handle);
                _handle = {};
            }
        }
    }
}

void SPTextItem::release()
{
    if (_handle) {
        document->getDocumentFonts().remove(*_handle);
        _handle = {};
    }

    SPItem::release();
}
