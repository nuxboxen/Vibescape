# SPDX-License-Identifier: GPL-2.0-or-later
file(GLOB _FILES "*.svg")
install(FILES ${_FILES} DESTINATION ${INKSCAPE_SHARE_INSTALL}/branding)

# Keep the Windows shell and GTK application icons in sync for Vibescape.
if(WIN32)
  install(FILES vibescape.ico DESTINATION ${INKSCAPE_SHARE_INSTALL}/branding)
  foreach(icon_size IN ITEMS 16 22 24 32 48 256)
    install(FILES "windows/${icon_size}x${icon_size}/vibescape.png"
      DESTINATION "${SHARE_INSTALL}/icons/hicolor/${icon_size}x${icon_size}/apps")
  endforeach()
endif()
