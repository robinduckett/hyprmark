#include "render/MarkdownRenderer.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef HYPRMARK_FIXTURES_DIR
#error "HYPRMARK_FIXTURES_DIR must be defined at compile time"
#endif

namespace {
    std::string fixture(const std::string& rel) {
        return std::string(HYPRMARK_FIXTURES_DIR) + "/" + rel;
    }
}

class MarkdownRendererTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // point the asset base at our in-tree assets/ dir so the template
        // loader works in tests without needing an install.
        renderer.setAssetBase(std::filesystem::path(HYPRMARK_FIXTURES_DIR).parent_path().parent_path() / "assets");
    }
    CMarkdownRenderer renderer;
};

TEST_F(MarkdownRendererTest, BasicMarkdown) {
    const auto r = renderer.renderString("# Hello\n\nWorld.\n");
    EXPECT_NE(r.html.find("<h1"),  std::string::npos);
    EXPECT_NE(r.html.find("Hello"), std::string::npos);
    EXPECT_NE(r.html.find("<p>"),   std::string::npos);
    ASSERT_FALSE(r.headings.empty());
    EXPECT_EQ(r.headings.front().level, 1);
    EXPECT_EQ(r.headings.front().text,  "Hello");
    EXPECT_FALSE(r.headings.front().id.empty());
}

TEST_F(MarkdownRendererTest, HeadingIdsInjected) {
    const auto r = renderer.renderString("# First\n## Second\n");
    EXPECT_NE(r.html.find("<h1 id=\""), std::string::npos);
    EXPECT_NE(r.html.find("<h2 id=\""), std::string::npos);
}

TEST_F(MarkdownRendererTest, HeadingSluggerDedup) {
    const auto r = renderer.renderString("## Collision\n## Collision\n## Collision\n");
    ASSERT_EQ(r.headings.size(), 3u);
    EXPECT_EQ(r.headings[0].id, "collision");
    EXPECT_EQ(r.headings[1].id, "collision-2");
    EXPECT_EQ(r.headings[2].id, "collision-3");
}

TEST_F(MarkdownRendererTest, HeadingSluggerAscii) {
    const auto r = renderer.renderString("## Hello World!!!\n");
    ASSERT_EQ(r.headings.size(), 1u);
    EXPECT_EQ(r.headings.front().id, "hello-world");
}

TEST_F(MarkdownRendererTest, GfmTables) {
    const auto r = renderer.renderString("| a | b |\n|---|---|\n| 1 | 2 |\n");
    EXPECT_NE(r.html.find("<table"), std::string::npos);
    EXPECT_NE(r.html.find("<th"),    std::string::npos);
    EXPECT_NE(r.html.find("<td"),    std::string::npos);
}

TEST_F(MarkdownRendererTest, GfmStrikethrough) {
    const auto r = renderer.renderString("~~gone~~\n");
    EXPECT_NE(r.html.find("<del>"), std::string::npos);
}

TEST_F(MarkdownRendererTest, GfmTaskList) {
    const auto r = renderer.renderString("- [x] done\n- [ ] todo\n");
    // md4c emits `<input type="checkbox"` for task items.
    EXPECT_NE(r.html.find("type=\"checkbox\""), std::string::npos);
}

TEST_F(MarkdownRendererTest, CodeFenceWithLanguage) {
    const auto r = renderer.renderString("```python\nprint(1)\n```\n");
    EXPECT_NE(r.html.find("language-python"), std::string::npos);
    EXPECT_NE(r.html.find("print(1)"),        std::string::npos);
}

TEST_F(MarkdownRendererTest, MermaidFencePreservedAsLanguageClass) {
    // Our C++ side leaves mermaid fences alone; JS rewrites them. Verify that.
    const auto r = renderer.renderString("```mermaid\ngraph TD;\nA-->B;\n```\n");
    EXPECT_NE(r.html.find("language-mermaid"), std::string::npos);
}

TEST_F(MarkdownRendererTest, DataUrlImagePassThrough) {
    const std::string md = "![dot](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAAB)\n";
    const auto        r  = renderer.renderString(md);
    EXPECT_NE(r.html.find("src=\"data:image/png;base64,"), std::string::npos);
}

TEST_F(MarkdownRendererTest, FullPageHasThemeAndContent) {
    const auto r = renderer.renderString("# x\n", "/tmp/x.md");
    EXPECT_NE(r.fullPage.find("theme-stylesheet"),   std::string::npos);
    EXPECT_NE(r.fullPage.find("hyprmark-content"),   std::string::npos);
    EXPECT_NE(r.fullPage.find("<h1"),                std::string::npos);
    EXPECT_NE(r.fullPage.find("data-source=\"/tmp/x.md\""), std::string::npos);
}

TEST_F(MarkdownRendererTest, RenderFileFromFixture) {
    const auto r = renderer.renderFile(fixture("basic.md"));
    EXPECT_FALSE(r.html.empty());
    EXPECT_GE(r.headings.size(), 3u);
    EXPECT_EQ(r.sourcePath, fixture("basic.md"));
    EXPECT_FALSE(r.sourceDir.empty());
}

TEST_F(MarkdownRendererTest, RenderNonExistentFileGivesErrorFragment) {
    const auto r = renderer.renderFile("/tmp/hyprmark-no-such-file.md");
    EXPECT_NE(r.html.find("hyprmark-error"),      std::string::npos);
    EXPECT_NE(r.html.find("Could not open file"), std::string::npos);
}
