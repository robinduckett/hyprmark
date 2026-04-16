#include "config/ConfigManager.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {
    std::string slurp(const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    class TmpConfig {
      public:
        TmpConfig() {
            path = fs::temp_directory_path() / ("hyprmark-writer-" + std::to_string(::getpid()) + "-" + std::to_string(counter++) + ".conf");
        }
        ~TmpConfig() { std::error_code ec; fs::remove(path, ec); }
        void seed(const std::string& content) {
            std::ofstream(path) << content;
        }
        fs::path path;
      private:
        inline static int counter = 0;
    };
}

TEST(ConfigWriter, ReplacesExistingKeyPreservingWhitespace) {
    TmpConfig tmp;
    tmp.seed(
        "general {\n"
        "    default_theme = nord\n"
        "    allow_remote_images = false\n"
        "}\n");

    CConfigManager cm(tmp.path.string());
    cm.init();
    ASSERT_TRUE(cm.writeKey("general", "allow_remote_images", "true"));

    const auto out = slurp(tmp.path);
    EXPECT_NE(out.find("allow_remote_images = true"), std::string::npos);
    EXPECT_EQ(out.find("allow_remote_images = false"), std::string::npos);
    // default_theme untouched
    EXPECT_NE(out.find("default_theme = nord"), std::string::npos);
}

TEST(ConfigWriter, InsertsKeyInExistingSection) {
    TmpConfig tmp;
    tmp.seed(
        "general {\n"
        "    default_theme = nord\n"
        "}\n");

    CConfigManager cm(tmp.path.string());
    cm.init();
    ASSERT_TRUE(cm.writeKey("general", "allow_remote_images", "true"));

    const auto out = slurp(tmp.path);
    EXPECT_NE(out.find("allow_remote_images = true"), std::string::npos);
    // original key still there
    EXPECT_NE(out.find("default_theme = nord"), std::string::npos);
}

TEST(ConfigWriter, AppendsNewSectionIfAbsent) {
    TmpConfig tmp;
    tmp.seed(
        "# top-of-file comment\n"
        "general {\n"
        "    default_theme = nord\n"
        "}\n");

    CConfigManager cm(tmp.path.string());
    cm.init();
    ASSERT_TRUE(cm.writeKey("advanced", "foo", "bar"));

    const auto out = slurp(tmp.path);
    EXPECT_NE(out.find("advanced {"), std::string::npos);
    EXPECT_NE(out.find("foo = bar"), std::string::npos);
    // existing content preserved
    EXPECT_NE(out.find("# top-of-file comment"), std::string::npos);
    EXPECT_NE(out.find("default_theme = nord"), std::string::npos);
}

TEST(ConfigWriter, PreservesCommentsAroundKey) {
    TmpConfig tmp;
    tmp.seed(
        "general {\n"
        "    # this toggles the thing\n"
        "    allow_remote_images = false  # nope\n"
        "    # bottom\n"
        "}\n");

    CConfigManager cm(tmp.path.string());
    cm.init();
    ASSERT_TRUE(cm.writeKey("general", "allow_remote_images", "true"));

    const auto out = slurp(tmp.path);
    EXPECT_NE(out.find("# this toggles the thing"), std::string::npos);
    EXPECT_NE(out.find("# bottom"), std::string::npos);
    EXPECT_NE(out.find("allow_remote_images = true"), std::string::npos);
}

TEST(ConfigWriter, CreatesFileWhenMissing) {
    // Point CConfigManager at a fresh path; writeKey should create it.
    const auto path = fs::temp_directory_path() / ("hyprmark-new-" + std::to_string(::getpid()) + ".conf");
    std::error_code ec; fs::remove(path, ec);

    CConfigManager cm(path.string());
    cm.init();
    ASSERT_TRUE(cm.writeKey("general", "allow_remote_images", "true"));
    EXPECT_TRUE(fs::exists(path));

    const auto out = slurp(path);
    EXPECT_NE(out.find("general {"), std::string::npos);
    EXPECT_NE(out.find("allow_remote_images = true"), std::string::npos);

    fs::remove(path, ec);
}
