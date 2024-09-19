# Findlibusb.cmake
# Find and install external libusb library

# Once done this will define
#
#  LIBUSB_FOUND         libusb present on system
#  LIBUSB_INCLUDE_DIR   the libusb include directory
#  LIBUSB_LIBRARY       the libraries needed to use libusb
#  LIBUSB_DEFINITIONS   compiler switches required for using libusb

include(FindPackageHandleStandardArgs)

if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")                       # FreeBSD; libusb is integrated into the system
    # libusb header file
    FIND_PATH(LIBUSB_INCLUDE_DIR
        NAMES libusb.h
        HINTS /usr/include
        )

    # libusb library
    set(LIBUSB_NAME usb)
    find_library(LIBUSB_LIBRARY
        NAMES ${LIBUSB_NAME}
        HINTS /usr /usr/local /opt
        )

    FIND_PACKAGE_HANDLE_STANDARD_ARGS(libusb DEFAULT_MSG LIBUSB_LIBRARY LIBUSB_INCLUDE_DIR)
    mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARY)
    if (NOT LIBUSB_FOUND)
        message(FATAL_ERROR "Expected libusb library not found on your system! Verify your system integrity.")
    endif()

elseif (CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")                   # OpenBSD; libusb is available from ports
    # libusb header file
    FIND_PATH(LIBUSB_INCLUDE_DIR
        NAMES libusb.h
        HINTS /usr/local/include
        PATH_SUFFIXES libusb-1.0
        )
    
    # libusb library
    set(LIBUSB_NAME usb-1.0)
    find_library(LIBUSB_LIBRARY
        NAMES ${LIBUSB_NAME}
        HINTS /usr/local
        )

    FIND_PACKAGE_HANDLE_STANDARD_ARGS(libusb DEFAULT_MSG LIBUSB_LIBRARY LIBUSB_INCLUDE_DIR)
    mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARY)
    if (NOT LIBUSB_FOUND)
        message(FATAL_ERROR "No libusb-1.0 library found on your system! Install libusb-1.0 from ports or packages.")
    endif()

elseif (WIN32 OR (MINGW AND EXISTS "/etc/debian_version"))      # Windows OR cross-build with MinGW-toolchain on Debian
    if (NOT LIBUSB_FOUND)   # Custom Approach
        FIND_PATH(LIBUSB_INCLUDE_DIR
                NAMES libusb.h
                HINTS "C:/Program Files/libusb-1.0" "C:/Program Files/usb-1.0" "C:/Program Files (x86)/libusb-1.0" "C:/Program Files (x86)/usb-1.0"
                PATH_SUFFIXES "include/libusb-1.0"
        )

        # libusb library
        set(LIBUSB_NAME usb-1.0)
        find_library(LIBUSB_LIBRARY
                NAMES ${LIBUSB_NAME} lib${LIBUSB_NAME}
                HINTS "C:/Program Files/libusb-1.0" "C:/Program Files/usb-1.0" "C:/Program Files (x86)/libusb-1.0" "C:/Program Files (x86)/usb-1.0"
        )

        set(LIBUSB_FIND_REQUIRED_TMP ${libusb_FIND_REQUIRED})
        set(libusb_FIND_REQUIRED OFF)
        FIND_PACKAGE_HANDLE_STANDARD_ARGS(libusb DEFAULT_MSG LIBUSB_LIBRARY LIBUSB_INCLUDE_DIR)
        set(libusb_FIND_REQUIRED ${LIBUSB_FIND_REQUIRED_TMP})
        mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARY)
    endif()
else ()                                                         # all other OS (unix-based)
    # libusb header file
    FIND_PATH(LIBUSB_INCLUDE_DIR
        NAMES libusb.h
        HINTS /usr/include
        PATH_SUFFIXES libusb-1.0
        )
    
    # libusb library
    set(LIBUSB_NAME usb-1.0)
    find_library(LIBUSB_LIBRARY
        NAMES ${LIBUSB_NAME}
        HINTS /usr /usr/local
        )
    set(LIBUSB_FIND_REQUIRED_TMP ${libusb_FIND_REQUIRED})
    set(libusb_FIND_REQUIRED OFF)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(libusb DEFAULT_MSG LIBUSB_INCLUDE_DIR LIBUSB_LIBRARY)
    set(libusb_FIND_REQUIRED ${LIBUSB_FIND_REQUIRED_TMP})
    mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARY)
endif()

if (NOT LIBUSB_FOUND)
    message(STATUS "### DOWNLOADING AND BUILDING LIBUSB FROM LIBUSB-CMAKE REPO ###")
    include(ExternalProject)

    set(LIBUSB_SRC     ${CMAKE_BINARY_DIR}/3rdParty/libusb-cmake/src)
    set(LIBUSB_BUILD   ${CMAKE_BINARY_DIR}/3rdParty/libusb-cmake/build)
    set(LIBUSB_INSTALL ${CMAKE_BINARY_DIR}/3rdParty/dist)

    if(MSVC)
        set(LIBUSB_FILES "${LIBUSB_INSTALL}/bin/usb-1.0.dll"
                "${LIBUSB_INSTALL}/lib/usb-1.0.lib"
                "${LIBUSB_INSTALL}/include/libusb-1.0/libusb.h")
    else()
        set(LIBUSB_FILES "${LIBUSB_INSTALL}/lib/libusb-1.0.so"
                "${LIBUSB_INSTALL}/include/libusb-1.0/libusb.h")
    endif()

    externalproject_add(
            libusb_ext
            GIT_REPOSITORY         https://github.com/libusb/libusb-cmake
            GIT_SHALLOW            ON
            GIT_TAG                main
            GIT_SUBMODULES_RECURSE ON
            GIT_PROGRESS           ON
            SOURCE_DIR             ${LIBUSB_SRC}
            BINARY_DIR             ${LIBUSB_BUILD}
            CMAKE_ARGS             -DLIBUSB_ENABLE_UDEV=OFF
                                   -DCMAKE_BUILD_TYPE=Release
                                   -DLIBUSB_BUILD_SHARED_LIBS=ON
                                   --install-prefix "${LIBUSB_INSTALL}"
            INSTALL_BYPRODUCTS     ${LIBUSB_FILES}
    )

    add_library(libusb SHARED IMPORTED)
    add_dependencies(libusb libusb_ext)

    set(LIBUSB_FOUND ON)
    set(LIBUSB_INCLUDE_DIR ${LIBUSB_INSTALL}/include)
    set(LIBUSB_LIBRARY libusb)
    mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARY)


    if(MSVC)
        set_target_properties(
                libusb PROPERTIES
                IMPORTED_LOCATION ${LIBUSB_INSTALL}/bin/libusb-1.0.dll
                IMPORTED_IMPLIB ${LIBUSB_INSTALL}/lib/usb-1.0.lib
        )

        target_compile_definitions(libusb INTERFACE _SSIZE_T_DEFINED ssize_t=long)
        set(LIBUSB_DEFINITIONS "_SSIZE_T_DEFINED" "ssize_t=int64_t")
        install(DIRECTORY ${LIBUSB_INSTALL}/bin DESTINATION ${CMAKE_INSTALL_PREFIX})
    else()
        set_target_properties(
                libusb PROPERTIES
                IMPORTED_LOCATION ${LIBUSB_INSTALL}/lib/libusb-1.0.so
        )
        set(LIBUSB_DEFINITIONS "")
    endif()

    install(DIRECTORY ${LIBUSB_INSTALL}/lib DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(DIRECTORY ${LIBUSB_INSTALL}/include DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
