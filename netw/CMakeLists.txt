get_filename_component(LIBRARY_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
string(REPLACE " " "_" LIBRARY_NAME "${LIBRARY_NAME}")

include(GenerateExportHeader)

set(Header_Files "netw.h") #  "${CMAKE_BINARY_DIR}/config/${PROJECT_NAME}Config.h"
source_group("Header Files" FILES "${Header_Files}")

set(_libs "")
set(Source_Files "netw.c")
if (APPLE)
  list(APPEND Source_Files "netw-macos.m")
  find_library(FOUNDATION_LIBRARY Foundation ONLY)
  enable_language(OBJC)
  list(APPEND _libs "${FOUNDATION_LIBRARY}")
elseif (WIN32)
  list(APPEND Source_Files "netw-win.c")
  list(APPEND _libs "Winhttp.lib")
else ()
  list(APPEND Source_Files "netw-libcurl.c")
  find_package(CURL CONFIG)
  if (CURL_FOUND)
    list(APPEND _libs "CURL::libcurl")
  else ()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL libcurl REQUIRED)
    list(APPEND _libs "curl")
  endif ()
endif ()
source_group("Source Files" FILES "${Source_Files}")

add_library("${LIBRARY_NAME}" "${Header_Files}" "${Source_Files}")

target_include_directories(
        "${LIBRARY_NAME}"
        INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
)

target_link_libraries("${LIBRARY_NAME}" PUBLIC "${PROJECT_LOWER_NAME}_compiler_flags")
target_link_libraries("${LIBRARY_NAME}" PRIVATE "${_libs}")

#set_target_properties(
#        "${LIBRARY_NAME}"
#        PROPERTIES
#        LINKER_LANGUAGE
#        C
#)

set(_export_file "${CMAKE_CURRENT_SOURCE_DIR}/${LIBRARY_NAME}_export.h")
generate_export_header("${LIBRARY_NAME}" EXPORT_FILE_NAME "${_export_file}")

# setup the version numbering
set_property(TARGET "${LIBRARY_NAME}" PROPERTY VERSION "1.0.0")
set_property(TARGET "${LIBRARY_NAME}" PROPERTY SOVERSION "1")

# install rules
set(installable_libs "${LIBRARY_NAME}" "${PROJECT_LOWER_NAME}_compiler_flags")
if (TARGET "${DEPENDANT_LIBRARY}")
  list(APPEND installable_libs "${DEPENDANT_LIBRARY}")
endif ()
install(TARGETS ${installable_libs}
        DESTINATION "bin"
        EXPORT "${LIBRARY_NAME}Targets")
#install(FILES "${_export_file}" "${Header_Files}" DESTINATION "include")
