# SPDX-License-Identifier: GPL-2.0-or-later

set(drawing_SRC
    svg-renderer.cpp
    svg-renderer.h
)

include("${CMAKE_CURRENT_LIST_DIR}/CMakeUnit.txt")

target_sources(inkscape_base PRIVATE ${drawing_unit_SRC} ${drawing_SRC})
