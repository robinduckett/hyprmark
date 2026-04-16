#include "render/ThemeManager.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#ifndef HYPRMARK_FIXTURES_DIR
#error "HYPRMARK_FIXTURES_DIR must be defined at compile time"
#endif

namespace fs = std::filesystem;

class ThemeManagerTest : public ::testing::Test {
  protected:
    fs::path tmp;
    fs::path tmpUser;

    void SetUp() override {
        tmp = fs::temp_directory_path() / ("hyprmark-theme-test-" + std::to_string(::getpid()));
        tmpUser = tmp / "user";
        fs::create_directories(tmp);
        fs::create_directories(tmpUser);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }

    fs::path assetsThemesDir() {
        return fs::path(HYPRMARK_FIXTURES_DIR).parent_path().parent_path() / "assets" / "themes";
    }

    void writeTheme(const fs::path& dir, const std::string& name) {
        std::ofstream(dir / (name + ".css")) << "/* " << name << " */\n";
    }
};

TEST_F(ThemeManagerTest, FindsInTreeBuiltinThemes) {
    CThemeManager tm;
    tm.setBuiltinDir(assetsThemesDir());
    tm.setUserDir(tmpUser); // empty

    const auto hypr = tm.findTheme("hypr-dark");
    ASSERT_TRUE(hypr.has_value());
    EXPECT_FALSE(hypr->isUser);
    EXPECT_EQ(tm.defaultName(), "hypr-dark");
}

TEST_F(ThemeManagerTest, PicksUpUserThemes) {
    writeTheme(tmpUser, "my-custom");

    CThemeManager tm;
    tm.setBuiltinDir(assetsThemesDir());
    tm.setUserDir(tmpUser);

    const auto mine = tm.findTheme("my-custom");
    ASSERT_TRUE(mine.has_value());
    EXPECT_TRUE(mine->isUser);
}

TEST_F(ThemeManagerTest, UserThemeOverridesBuiltinSameName) {
    writeTheme(tmpUser, "hypr-dark");

    CThemeManager tm;
    tm.setBuiltinDir(assetsThemesDir());
    tm.setUserDir(tmpUser);

    const auto t = tm.findTheme("hypr-dark");
    ASSERT_TRUE(t.has_value());
    EXPECT_TRUE(t->isUser);
}

TEST_F(ThemeManagerTest, UnknownThemeReturnsNulloptAndEmptyUrl) {
    CThemeManager tm;
    tm.setBuiltinDir(assetsThemesDir());
    tm.setUserDir(tmpUser);

    EXPECT_FALSE(tm.findTheme("definitely-not-a-theme").has_value());
    EXPECT_TRUE(tm.themeUrl("definitely-not-a-theme").empty());
}

TEST_F(ThemeManagerTest, ThemeUrlIsFileScheme) {
    CThemeManager tm;
    tm.setBuiltinDir(assetsThemesDir());
    tm.setUserDir(tmpUser);

    const auto url = tm.themeUrl("hypr-dark");
    EXPECT_TRUE(url.rfind("file://", 0) == 0);
    EXPECT_NE(url.find("hypr-dark.css"), std::string::npos);
}

TEST_F(ThemeManagerTest, MissingDirsDontCrash) {
    CThemeManager tm;
    tm.setBuiltinDir("/tmp/hyprmark-totally-nonexistent-1");
    tm.setUserDir("/tmp/hyprmark-totally-nonexistent-2");
    EXPECT_TRUE(tm.listThemes().empty());
    // defaultName still returns a string
    EXPECT_FALSE(tm.defaultName().empty());
}
