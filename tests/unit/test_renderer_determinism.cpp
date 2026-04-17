#include "render/MarkdownRenderer.hpp"

#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#ifndef HYPRMARK_FIXTURES_DIR
#  error "HYPRMARK_FIXTURES_DIR must be defined by CMake"
#endif

namespace {
    std::string readFixture(const char* name) {
        std::ifstream     in(std::string(HYPRMARK_FIXTURES_DIR) + "/" + name);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    CMarkdownRenderer makeRenderer() {
        CMarkdownRenderer r;
        r.setAssetBase(std::filesystem::path(HYPRMARK_FIXTURES_DIR).parent_path().parent_path() / "assets");
        return r;
    }
} // namespace

TEST(RendererDeterminism, IdenticalInputProducesIdenticalBody) {
    auto       r  = makeRenderer();
    const auto md = readFixture("reload_before.md");
    const auto a  = r.renderString(md, "/tmp/reload_before.md");
    const auto b  = r.renderString(md, "/tmp/reload_before.md");
    EXPECT_EQ(a.html, b.html);
    EXPECT_EQ(a.fullPage, b.fullPage);
}

TEST(RendererDeterminism, LocalEditPreservesStablePrefix) {
    auto       r      = makeRenderer();
    const auto before = r.renderString(readFixture("reload_before.md"));
    const auto after  = r.renderString(readFixture("reload_after.md"));
    ASSERT_NE(before.html, after.html);
    const auto h1End = before.html.find("</h1>");
    ASSERT_NE(h1End, std::string::npos);
    EXPECT_EQ(before.html.substr(0, h1End + 5), after.html.substr(0, h1End + 5));
}

TEST(RendererDeterminism, MissingFileYieldsNonEmptyErrorBody) {
    auto       r = makeRenderer();
    const auto s = r.renderFile("/definitely/not/here.md");
    EXPECT_FALSE(s.html.empty());
}
