if(WITH_INTERNAL_2GEOM)
    set(2GEOM_BUILD_SHARED ${BUILD_SHARED_LIBS} CACHE BOOL "")
    include(FetchContent)
    FetchContent_Declare(2Geom SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/2geom" OVERRIDE_FIND_PACKAGE)
    FetchContent_MakeAvailable(2Geom)
endif()

if(WITH_INTERNAL_UEMF)
    add_subdirectory(libuemf)
endif()

add_subdirectory(libcroco)

if(WITH_INTERNAL_DEPIXELIZE)
    add_subdirectory(libdepixelize)
endif()

if(WITH_INTERNAL_ADAPTAGRAMS)
    add_subdirectory(adaptagrams)
endif()

if(WITH_INTERNAL_AUTOTRACE)
    add_subdirectory(autotrace)
endif()
