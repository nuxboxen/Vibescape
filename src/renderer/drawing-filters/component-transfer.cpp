// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feComponentTransfer filter primitive renderer
 *
 * Authors:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *   Jasper van de Gronde <th.v.d.gronde@hccnet.nl>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "colors/manager.h"
#include "component-transfer.h"
#include "slot.h"

#include "renderer/context.h"
#include "renderer/pixel-filters/component-transfer.h"
#include "renderer/surface.h"

#include "colors/spaces/base.h"

namespace Inkscape::Renderer::DrawingFilter {

void ComponentTransfer::render(Slot &slot) const
{
    auto output = slot.get_copy(_input, _color_space);
    if (!output) {
        return;
    }

    std::vector<PixelFilter::TransferFunction> tfs;
    for (int i = 0; i <= output->components(); ++i) {
        switch (type[i]) {
            case ComponentTransferType::TABLE:
            case ComponentTransferType::DISCRETE:
                if (!tableValues[i].empty()) {
                    tfs.emplace_back(tableValues[i], type[i] == ComponentTransferType::DISCRETE);
                    break;
                }
                // pass through
            case ComponentTransferType::ERROR:
            case ComponentTransferType::IDENTITY:
                tfs.emplace_back();
                break;
            case ComponentTransferType::LINEAR:
                tfs.emplace_back(slope[i], intercept[i]);
                break;
            case ComponentTransferType::GAMMA:
                tfs.emplace_back(amplitude[i], exponent[i], offset[i]);
                break;
        }
    }
    output->run_pixel_filter(PixelFilter::ComponentTransfer(tfs));
    slot.set(_output, output);
}

bool ComponentTransfer::can_handle_affine(Geom::Affine const &) const
{
    return true;
}

double ComponentTransfer::complexity(Geom::Affine const &) const
{
    return 2.0;
}

} // namespace Inkscape::Renderer::Drawing

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
