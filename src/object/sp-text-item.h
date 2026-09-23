// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_OBJECT_TEXT_ITEM_H
#define INKSCAPE_OBJECT_TEXT_ITEM_H

#include <optional>

#include "sp-item.h"
#include "util/document-fonts.h"

class SPTextItem : public SPItem
{
public:
    int tag() const override { return tag_of<decltype(*this)>; }

    void update(SPCtx *ctx, unsigned flags) override;
    void release() override;

private:
    std::optional<Inkscape::DocumentFonts::Handle> _handle;
};

#endif // INKSCAPE_OBJECT_TEXT_ITEM_H
