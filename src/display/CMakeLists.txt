# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(inkscape_base PRIVATE
    nr-3dutils.cpp
    translucency-group.cpp

    control/canvas-temporary-item-list.cpp
    control/canvas-temporary-item.cpp
    control/ctrl-handle-manager.cpp
    control/ctrl-handle-rendering.cpp
    control/ctrl-handle-styling.cpp
    control/snap-indicator.cpp

    control/canvas-item.cpp
    control/canvas-item-bpath.cpp
    control/canvas-item-catchall.cpp
    control/canvas-item-context.cpp
    control/canvas-item-ctrl.cpp
    control/canvas-item-curve.cpp
    control/canvas-item-drawing.cpp
    control/canvas-item-grid.cpp
    control/canvas-item-group.cpp
    control/canvas-item-guideline.cpp
    control/canvas-item-quad.cpp
    control/canvas-item-rect.cpp
    control/canvas-item-text.cpp
    control/canvas-page.cpp

    # -------
    # Headers
    initlock.h
    nr-3dutils.h
    translucency-group.h

    control/canvas-temporary-item-list.h
    control/canvas-temporary-item.h
    control/ctrl-handle-manager.h
    control/ctrl-handle-rendering.h
    control/ctrl-handle-styling.h
    control/snap-indicator.h

    control/canvas-item.h
    control/canvas-item-bpath.h
    control/canvas-item-buffer.h
    control/canvas-item-catchall.h
    control/canvas-item-context.h
    control/canvas-item-ctrl.h
    control/canvas-item-curve.h
    control/canvas-item-drawing.h
    control/canvas-item-enums.h
    control/canvas-item-grid.h
    control/canvas-item-group.h
    control/canvas-item-guideline.h
    control/canvas-item-ptr.h
    control/canvas-item-quad.h
    control/canvas-item-rect.h
    control/canvas-item-text.h
    control/canvas-page.h
)
