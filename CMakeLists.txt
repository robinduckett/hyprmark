cmake_minimum_required(VERSION 3.27)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VERSION)

project(
  hyprmark
  DESCRIPTION "A markdown viewer for Hyprland"
  VERSION ${VERSION}
  LANGUAGES CXX)

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Configuring hyprmark in Debug with CMake")
  add_compile_definitions(HYPRLAND_DEBUG)
else()
  add_compile_options(-O3)
  message(STATUS "Configuring hyprmark in Release with CMake")
endif()

include_directories(.)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value
                    -Wno-missing-field-initializers -Wno-narrowing)

add_compile_definitions(HYPRMARK_VERSION="${VERSION}")

if(DEFINED HYPRMARK_COMMIT)
  add_compile_definitions(HYPRMARK_COMMIT="${HYPRMARK_COMMIT}")
else()
  execute_process(
    OUTPUT_VARIABLE GIT_SHORT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND git rev-parse --short HEAD
    ERROR_QUIET
  )
  if(GIT_SHORT_HASH STREQUAL "")
    set(GIT_SHORT_HASH "unknown")
  endif()
  add_compile_definitions(HYPRMARK_COMMIT="${GIT_SHORT_HASH}")
endif()

if(DEFINED HYPRMARK_VERSION_COMMIT)
  add_compile_definitions(HYPRMARK_VERSION_COMMIT="${HYPRMARK_VERSION_COMMIT}")
else()
  execute_process(
    OUTPUT_VARIABLE GIT_TAG_SHORT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND git rev-parse --short v${VERSION}
    ERROR_QUIET
  )
  if(GIT_TAG_SHORT_HASH STREQUAL "")
    set(GIT_TAG_SHORT_HASH "unreleased")
  endif()
  add_compile_definitions(HYPRMARK_VERSION_COMMIT="${GIT_TAG_SHORT_HASH}")
endif()

message(STATUS "VERSION COMMIT: ${HYPRMARK_VERSION_COMMIT}")
message(STATUS "COMMIT: ${HYPRMARK_COMMIT}")

# position-independent build: strip absolute paths from __FILE__
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

# Coverage (opt-in, applied globally; only matters when test binaries are linked)
option(HYPRMARK_COVERAGE "Enable --coverage on hyprmark_core and test binaries" OFF)
if(HYPRMARK_COVERAGE)
  if(NOT (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG))
    message(WARNING "HYPRMARK_COVERAGE=ON but build type is not Debug — numbers will be optimised-out")
  endif()
  add_compile_options(--coverage -O0 -g)
  add_link_options(--coverage)
  message(STATUS "Coverage instrumentation: ON")
endif()

# dependencies
message(STATUS "Checking deps...")

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineCore WebEngineWidgets WebChannel)
qt_standard_project_setup(REQUIRES 6.6)

pkg_check_modules(
  deps
  REQUIRED
  IMPORTED_TARGET
  hyprutils>=0.8.0
  hyprlang>=0.6.0
  md4c
  md4c-html)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

# Build the app code as a small library so the test binary can link against
# the same objects (matches the hyprutils pattern for coverage + test reuse).
list(REMOVE_ITEM SRCFILES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(hyprmark_core STATIC ${SRCFILES})
target_link_libraries(hyprmark_core PUBLIC
  PkgConfig::deps
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebEngineCore Qt6::WebEngineWidgets Qt6::WebChannel)
target_include_directories(hyprmark_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

qt_add_executable(hyprmark src/main.cpp)
target_link_libraries(hyprmark PRIVATE
  rt Threads::Threads
  hyprmark_core)

# Installation
install(TARGETS hyprmark)
install(FILES ${CMAKE_SOURCE_DIR}/assets/example.conf
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/hypr
        RENAME hyprmark.conf)
install(FILES
          ${CMAKE_SOURCE_DIR}/assets/template.html
          ${CMAKE_SOURCE_DIR}/assets/hyprmark.css
          ${CMAKE_SOURCE_DIR}/assets/hyprmark.js
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/hyprmark)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/themes
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/hyprmark
        FILES_MATCHING PATTERN "*.css")
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/vendor
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/hyprmark
        OPTIONAL)
install(FILES ${CMAKE_SOURCE_DIR}/assets/hyprmark.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/applications)
install(FILES ${CMAKE_SOURCE_DIR}/assets/icon.svg
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/scalable/apps
        RENAME hyprmark.svg)

# Tests (Debug builds only, matching hyprutils convention)
if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG OR BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
