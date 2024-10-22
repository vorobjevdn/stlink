configure_file(
    "${PROJECT_SOURCE_DIR}/inc/version.h.in"
    "${CMAKE_BINARY_DIR}/inc/version/stlink/version.h"
    )

file(GLOB STLINK_HEADERS
    "${CMAKE_SOURCE_DIR}/inc/stlink/*.h"
    "${CMAKE_SOURCE_DIR}/src/stlink-lib/include/stlink/*.h"
    "${CMAKE_BINARY_DIR}/inc/version/stlink/version.h")
install(FILES ${STLINK_HEADERS} DESTINATION ${STLINK_INCLUDE_PATH})
