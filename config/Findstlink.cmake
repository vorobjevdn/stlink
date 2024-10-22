# Findstlink.cmake
# Find and install external stlink library

# Once done this will define
#
#  STLINK_FOUND         stlink present on system
#  STLINK_INCLUDE_DIR   the stlink include directory
#  STLINK_LIBRARY       the libraries needed to use stlink
#  STLINK_DEFINITIONS   compiler switches required for using stlink

include(GNUInstallDirs)
option(STLINK_USE_STATIC "Use Static STLINK Library instead of shared" OFF)

# Retrieving the root directory and basic components.
# This script lives in %ROOT%/cmake/modules/Findstlink.cmake
set(SCRIPT_ROOT ${CMAKE_CURRENT_LIST_DIR})                       # SCRIPT_ROOT  = %ROOT%/cmake/modules
cmake_path(GET SCRIPT_ROOT PARENT_PATH MODULES_ROOT)             # MODULES_ROOT = %ROOT%/cmake
cmake_path(GET MODULES_ROOT PARENT_PATH STLINK_HOME)             # STLINK_HOME  = %ROOT%

# Setting the default directories
set(STLINK_BINDIR     ${STLINK_HOME}/${CMAKE_INSTALL_BINDIR})
set(STLINK_LIBDIR     ${STLINK_HOME}/${CMAKE_INSTALL_LIBDIR})
set(STLINK_INCLUDEDIR ${STLINK_HOME}/${CMAKE_INSTALL_INCLUDEDIR})

# Attempting to use already-installed libusb
find_package(libusb QUIET)

# Creating the imported library

# OS-Specific property-setting
if(MSVC)
    if (STLINK_USE_STATIC)
        add_library(stlink::stlink STATIC IMPORTED)
        target_compile_definitions(stlink::stlink PUBLIC STLINK_STATIC)
        set(STLINK_IMPORTED_LOCATION ${STLINK_LIBDIR}/stlink-static.lib)
        set(STLINK_IMPORTED_IMPLIB   ${STLINK_LIBDIR}/stlink-static.lib)
    else()
        add_library(stlink::stlink SHARED IMPORTED)
        set(STLINK_IMPORTED_LOCATION ${STLINK_LIBDIR}/stlink.dll)
        set(STLINK_IMPORTED_IMPLIB   ${STLINK_LIBDIR}/stlink-static.lib)
    endif()
    set_target_properties(
            stlink::stlink PROPERTIES
            IMPORTED_LOCATION                    ${STLINK_IMPORTED_LOCATION}
            IMPORTED_IMPLIB                      ${STLINK_IMPORTED_IMPLIB}
    )
    install(FILES ${STLINK_BINDIR}/stlink.dll  DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${STLINK_LIBDIR}/stlink-static.lib  DESTINATION ${CMAKE_INSTALL_LIBDIR})
    if(NOT LIBUSB_FOUND)
        set(LIBUSB_LIBRARY     "usb-1.0")
        add_library(usb-1.0 SHARED IMPORTED)
        set_target_properties(usb-1.0 PROPERTIES
                IMPORTED_LOCATION                    ${STLINK_BINDIR}/libusb-1.0.dll
                IMPORTED_IMPLIB                      ${STLINK_LIBDIR}/usb-1.0.lib
        )
        set(LIBUSB_INCLUDE_DIR "") #exists in st-link incdir
        set(LIBUSB_FOUND ON)
        set(LIBUSB_DEFINITIONS "_SSIZE_T_DEFINED" "ssize_t=long")
        install(FILES ${STLINK_BINDIR}/libusb-1.0.dll DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(FILES ${STLINK_LIBDIR}/usb-1.0.lib    DESTINATION ${CMAKE_INSTALL_LIBDIR})
        install(DIRECTORY ${STLINK_INCLUDEDIR}/libusb-1.0 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()
else()
    if (STLINK_USE_STATIC)
        add_library(stlink::stlink STATIC IMPORTED)
        target_compile_definitions(stlink::stlink PUBLIC STLINK_STATIC)
        set(STLINK_IMPORTED_LOCATION ${STLINK_LIBDIR}/libstlink.a)
        set(STLINK_IMPORTED_IMPLIB   ${STLINK_LIBDIR}/libstlink.a)
    else()
        add_library(stlink::stlink SHARED IMPORTED)
        set(STLINK_IMPORTED_LOCATION ${STLINK_LIBDIR}/libstlink.so)
        set(STLINK_IMPORTED_IMPLIB   ${STLINK_LIBDIR}/libstlink.so)
    endif()
    set_target_properties(
            stlink::stlink PROPERTIES
            IMPORTED_LOCATION                    ${STLINK_IMPORTED_LOCATION}
            IMPORTED_IMPLIB                      ${STLINK_IMPORTED_IMPLIB}
    )
    install(FILES ${STLINK_LIBDIR}/libstlink.so   DESTINATION ${CMAKE_INSTALL_LIBDIR})
    if(NOT LIBUSB_FOUND)
        set(LIBUSB_LIBRARY     "usb-1.0")
        add_library(usb-1.0 SHARED IMPORTED)
        set_target_properties(usb-1.0 PROPERTIES
                IMPORTED_LOCATION                    ${STLINK_LIBDIR}/libusb-1.0.so
        )
        set(LIBUSB_INCLUDE_DIR "") #exists in st-link incdir
        set(LIBUSB_FOUND ON)
        set(LIBUSB_DEFINITIONS "")
        install(FILES ${STLINK_LIBDIR}/libusb-1.0.so    DESTINATION ${CMAKE_INSTALL_LIBDIR})
        install(DIRECTORY ${STLINK_INCLUDEDIR}/libusb-1.0 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()
endif()

# Imported Target Configuration
target_link_libraries     (stlink::stlink               INTERFACE ${LIBUSB_LIBRARY})
target_include_directories(stlink::stlink SYSTEM BEFORE INTERFACE ${LIBUSB_INCLUDE_DIR})
target_compile_definitions(stlink::stlink               INTERFACE ${LIBUSB_DEFINITIONS})
install(DIRECTORY ${STLINK_INCLUDEDIR}/stlink DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Find Variables configuration
set(STLINK_FOUND        ON)
set(STLINK_INCLUDE_DIR  "${STLINK_INCLUDEDIR}")
if(NOT LIBUSB_INCLUDE_DIR STREQUAL "")
    list(APPEND STLINK_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR})
endif()
set(STLINK_LIBRARY      "stlink::stlink")
set(STLINK_DEFINITIONS  ${LIBUSB_DEFINITIONS})

