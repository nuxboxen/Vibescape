# Stage a runnable Windows development build only when its inputs change.
# The existing full install rules remain the authority for runtime contents.
file(GLOB_RECURSE VIBESCAPE_STAGE_RESOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false
    "${CMAKE_SOURCE_DIR}/share/*"
    "${CMAKE_SOURCE_DIR}/po/*.po"
)
list(FILTER VIBESCAPE_STAGE_RESOURCES EXCLUDE REGEX "/(\\.git|__pycache__)(/|$)|\\.py[co]$")

# Every successful configure invalidates the staged runtime, including packaging-rule changes.
file(WRITE "${CMAKE_BINARY_DIR}/vibescape-configured.stamp" "")
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/vibescape-installed.stamp"
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/vibescape-installed.stamp"
    DEPENDS
        inkscape inkview inkscape_com inkview_com
        "${CMAKE_BINARY_DIR}/vibescape-configured.stamp"
        "${CMAKE_SOURCE_DIR}/README.md"
        "${CMAKE_SOURCE_DIR}/NEWS.md"
        ${VIBESCAPE_STAGE_RESOURCES}
    COMMENT "Updating the runnable Vibescape folder"
    VERBATIM
    USES_TERMINAL
)
add_custom_target(vibescape-stage
    DEPENDS "${CMAKE_BINARY_DIR}/vibescape-installed.stamp"
)
