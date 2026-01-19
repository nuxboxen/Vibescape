# SPDX-License-Identifier: GPL-2.0-or-later

set(renderer_SRC
    surface-image.cpp
    surface-image.h
)

include("${CMAKE_CURRENT_LIST_DIR}/CMakeUnit.txt")

target_sources(inkscape_base PRIVATE ${renderer_unit_SRC} ${renderer_SRC})

add_subdirectory(drawing)
add_subdirectory(drawing-filters)
