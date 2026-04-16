#include "config/ConfigManager.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef HYPRMARK_FIXTURES_DIR
#error "HYPRMARK_FIXTURES_DIR must be defined at compile time"
#endif

namespace {
    std::string fixture(const std::string& rel) {
        return std::string(HYPRMARK_FIXTURES_DIR) + "/" + rel;
    }
}

TEST(ConfigManager, DefaultsWhenFileMissing) {
    CConfigManager cm("/tmp/definitely-does-not-exist-hyprmark-test.conf");
    cm.init();

    EXPECT_EQ(cm.defaultTheme(), "hypr-dark");
    EXPECT_TRUE(cm.liveReload());
    EXPECT_FALSE(cm.showToc());
    EXPECT_FLOAT_EQ(cm.defaultZoom(), 1.0f);
    EXPECT_TRUE(cm.enableMath());
    EXPECT_TRUE(cm.enableMermaid());
    EXPECT_FALSE(cm.allowRemoteImages());

    EXPECT_EQ(cm.keybind("open"), "Ctrl+O");
    EXPECT_EQ(cm.keybind("cycle_theme"), "Ctrl+T");
}

TEST(ConfigManager, DefaultsWhenConfigPathEmpty) {
    // empty path => findDefaultConfigPath may still return something in HOME,
    // but in any case the manager must not crash.
    CConfigManager cm("");
    cm.init();
    EXPECT_FALSE(cm.defaultTheme().empty());
}

TEST(ConfigManager, MinimalOverrides) {
    const auto path = fixture("configs/minimal.conf");
    ASSERT_TRUE(std::filesystem::exists(path)) << "fixture missing: " << path;

    CConfigManager cm(path);
    cm.init();

    EXPECT_EQ(cm.defaultTheme(), "nord");
    EXPECT_FLOAT_EQ(cm.defaultZoom(), 1.5f);
    EXPECT_FALSE(cm.enableMath());
    EXPECT_TRUE(cm.enableMermaid());            // untouched default
    EXPECT_TRUE(cm.allowRemoteImages());

    // keybinds unchanged when not present in the file
    EXPECT_EQ(cm.keybind("open"), "Ctrl+O");
}

TEST(ConfigManager, KeybindOverrides) {
    const auto path = fixture("configs/keybinds.conf");
    ASSERT_TRUE(std::filesystem::exists(path));

    CConfigManager cm(path);
    cm.init();

    EXPECT_EQ(cm.keybind("open"), "Ctrl+Shift+O");
    EXPECT_EQ(cm.keybind("cycle_theme"), "Ctrl+Shift+T");
    // untouched default
    EXPECT_EQ(cm.keybind("find"), "Ctrl+F");
}

TEST(ConfigManager, EmptyFileUsesAllDefaults) {
    const auto path = fixture("configs/empty.conf");
    ASSERT_TRUE(std::filesystem::exists(path));

    CConfigManager cm(path);
    cm.init();

    EXPECT_EQ(cm.defaultTheme(), "hypr-dark");
    EXPECT_TRUE(cm.liveReload());
    EXPECT_FLOAT_EQ(cm.defaultZoom(), 1.0f);
}

TEST(ConfigManager, UnknownKeyDoesNotCrash) {
    // Write a tmp file with an unknown key, ensure we don't crash on parse.
    // Unknown keys surface as parse errors (Debug::log WARN), not exceptions.
    const auto path = std::filesystem::temp_directory_path() / "hyprmark-unknown-test.conf";
    std::filesystem::remove(path);
    {
        std::ofstream f(path);
        f << "general {\n    nonexistent_key = 42\n}\n";
    }

    CConfigManager cm(path.string());
    cm.init();
    // Should still return defaults.
    EXPECT_EQ(cm.defaultTheme(), "hypr-dark");

    std::filesystem::remove(path);
}
