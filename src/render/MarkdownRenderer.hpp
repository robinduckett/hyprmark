#pragma once

#include "../defines.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct SHeading {
    int         level = 0;
    std::string id;
    std::string text;
};

struct SRendered {
    std::string           html;       // body-only markdown → HTML (no wrapping document)
    std::string           fullPage;   // complete HTML document (body spliced into template)
    std::vector<SHeading> headings;   // auto-extracted for TOC
    std::string           sourcePath; // absolute canonical path if rendered from disk
    std::string           sourceDir;  // directory containing the source (for relative images)
};

class CMarkdownRenderer {
  public:
    CMarkdownRenderer();
    ~CMarkdownRenderer() = default;

    // Where to look up template.html / themes / vendor assets. Explicitly passed
    // rather than baked in so tests can point at the in-tree assets/ dir.
    void setAssetBase(const std::filesystem::path& assetBase);
    void setThemeUrl(const std::string& url); // file:// URL to the active theme CSS

    // Render raw markdown bytes.
    SRendered renderString(const std::string& markdown, const std::string& sourcePath = {});

    // Render the file at `path`. Caller is responsible for checking existence first
    // (we return an error-fragment SRendered if the file can't be read, with html
    // describing what went wrong so the app can surface it in-view).
    SRendered renderFile(const std::filesystem::path& path);

  private:
    std::string loadTemplate();
    std::string makeFullPage(const std::string& bodyHtml, const std::string& sourcePath);
    static std::vector<SHeading> extractHeadings(const std::string& bodyHtml);

    std::filesystem::path m_assetBase = "/usr/share/hyprmark";
    std::string           m_themeUrl;        // e.g. file:///usr/share/hyprmark/themes/hypr-dark.css
    std::string           m_cachedTemplate;  // lazy-loaded
};

inline UP<CMarkdownRenderer> g_pRenderer;
