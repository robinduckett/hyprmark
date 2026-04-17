find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# Unit tests — exercise pure library code (ConfigManager, MarkdownRenderer,
# ThemeManager, future IPC protocol). Link hyprmark_core so they get the real
# code, not reimplementations.
file(GLOB UNIT_TESTS CONFIGURE_DEPENDS "unit/*.cpp")

if(UNIT_TESTS)
  add_executable(hyprmark_unit_tests ${UNIT_TESTS})
  target_link_libraries(hyprmark_unit_tests PRIVATE
    hyprmark_core
    GTest::gtest_main
    Qt6::Test)
  target_compile_definitions(hyprmark_unit_tests PRIVATE
    HYPRMARK_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
  gtest_discover_tests(hyprmark_unit_tests)
endif()
