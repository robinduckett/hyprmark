#include "MarkdownRenderer.hpp"

#include "../helpers/Log.hpp"

#include <md4c-html.h>
#include <md4c.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace {
    // md4c emits HTML in chunks via this callback.
    void appendChunk(const MD_CHAR* data, MD_SIZE size, void* userdata) {
        auto* out = static_cast<std::string*>(userdata);
        out->append(data, size);
    }

    // Unicode-naive slugger. For the app this is good enough — the DOM side
    // uses these only as in-page anchors. Handles ASCII + dedup; non-ASCII
    // falls through as-is and may produce broken anchors in edge cases, which
    // is acceptable for v1.
    std::string slug(const std::string& raw, std::vector<std::string>& seen) {
        std::string s;
        s.reserve(raw.size());
        for (char c : raw) {
            const auto uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc))
                s.push_back(static_cast<char>(std::tolower(uc)));
            else if (std::isspace(uc) || c == '-' || c == '_')
                s.push_back('-');
            // else drop
        }
        // collapse consecutive dashes
        std::string collapsed;
        collapsed.reserve(s.size());
        bool prevDash = false;
        for (char c : s) {
            if (c == '-') {
                if (prevDash)
                    continue;
                prevDash = true;
            } else
                prevDash = false;
            collapsed.push_back(c);
        }
        while (!collapsed.empty() && collapsed.front() == '-')
            collapsed.erase(collapsed.begin());
        while (!collapsed.empty() && collapsed.back() == '-')
            collapsed.pop_back();
        if (collapsed.empty())
            collapsed = "section";

        // dedup
        std::string candidate = collapsed;
        int         n         = 2;
        while (std::find(seen.begin(), seen.end(), candidate) != seen.end())
            candidate = collapsed + "-" + std::to_string(n++);
        seen.push_back(candidate);
        return candidate;
    }

    std::string stripTags(const std::string& html) {
        std::string out;
        out.reserve(html.size());
        bool inTag = false;
        for (char c : html) {
            if (c == '<')
                inTag = true;
            else if (c == '>')
                inTag = false;
            else if (!inTag)
                out.push_back(c);
        }
        return out;
    }

    // Inject an id="..." attribute into every <hN>...</hN> (1 <= N <= 6) in the
    // body HTML. Needed for TOC scroll-to-anchor. Operates on the body-only
    // output of md_html (which doesn't generate IDs itself).
    std::string injectHeadingIds(const std::string& html, std::vector<SHeading>& outHeadings) {
        static const std::regex HEADING(R"(<h([1-6])>([\s\S]*?)</h\1>)", std::regex::ECMAScript);
        std::string             out;
        std::vector<std::string> used;
        out.reserve(html.size() + 128);

        auto it    = html.cbegin();
        const auto end = html.cend();
        std::smatch m;

        while (std::regex_search(it, end, m, HEADING)) {
            out.append(m.prefix().first, m.prefix().second);
            const int   level = std::stoi(m[1].str());
            const auto  inner = m[2].str();
            const auto  text  = stripTags(inner);
            const auto  id    = slug(text, used);
            out += "<h" + std::to_string(level) + " id=\"" + id + "\">" + inner + "</h" + std::to_string(level) + ">";
            outHeadings.push_back(SHeading{.level = level, .id = id, .text = text});
            it = m.suffix().first;
        }
        out.append(it, end);
        return out;
    }
} // namespace

CMarkdownRenderer::CMarkdownRenderer() = default;

void CMarkdownRenderer::setAssetBase(const std::filesystem::path& assetBase) {
    m_assetBase      = assetBase;
    m_cachedTemplate.clear();
}

void CMarkdownRenderer::setThemeUrl(const std::string& url) {
    m_themeUrl = url;
}

SRendered CMarkdownRenderer::renderString(const std::string& markdown, const std::string& sourcePath) {
    SRendered result;
    result.sourcePath = sourcePath;
    if (!sourcePath.empty()) {
        std::error_code ec;
        auto            p = std::filesystem::absolute(sourcePath, ec);
        if (!ec)
            result.sourceDir = p.parent_path().string();
    }

    std::string bodyRaw;
    const unsigned flags = MD_DIALECT_GITHUB | MD_FLAG_COLLAPSEWHITESPACE;
    const int rc = md_html(markdown.data(), static_cast<MD_SIZE>(markdown.size()),
                           &appendChunk, &bodyRaw, flags, 0);
    if (rc != 0) {
        Debug::log(ERR, "md_html returned {}; falling back to escaped plain text", rc);
        std::string escaped = "<pre>";
        for (char c : markdown) {
            switch (c) {
                case '&': escaped += "&amp;"; break;
                case '<': escaped += "&lt;"; break;
                case '>': escaped += "&gt;"; break;
                default:  escaped.push_back(c);
            }
        }
        escaped += "</pre>";
        bodyRaw = std::move(escaped);
    }

    result.html     = injectHeadingIds(bodyRaw, result.headings);
    result.fullPage = makeFullPage(result.html, result.sourcePath);
    return result;
}

SRendered CMarkdownRenderer::renderFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        SRendered err;
        err.sourcePath = path.string();
        err.html       = "<div class=\"hyprmark-error\"><h1>Could not open file</h1><p>" + path.string() + "</p></div>";
        err.fullPage   = makeFullPage(err.html, err.sourcePath);
        return err;
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    return renderString(ss.str(), path.string());
}

std::string CMarkdownRenderer::loadTemplate() {
    if (!m_cachedTemplate.empty())
        return m_cachedTemplate;

    const auto    tpl = m_assetBase / "template.html";
    std::ifstream f(tpl, std::ios::binary);
    if (!f) {
        Debug::log(ERR, "template not found at {}; using minimal fallback", tpl.string());
        m_cachedTemplate = R"(<!DOCTYPE html><html><head><meta charset="utf-8">)"
                           R"(<link id="theme-stylesheet" rel="stylesheet" href="{{THEME_URL}}"></head>)"
                           R"(<body><main id="hyprmark-content" data-source="{{SOURCE_PATH}}">{{CONTENT}}</main></body></html>)";
    } else {
        std::ostringstream ss;
        ss << f.rdbuf();
        m_cachedTemplate = ss.str();
    }
    return m_cachedTemplate;
}

std::string CMarkdownRenderer::makeFullPage(const std::string& bodyHtml, const std::string& sourcePath) {
    std::string tpl = loadTemplate();

    auto replace = [&](const std::string& needle, const std::string& value) {
        auto pos = tpl.find(needle);
        while (pos != std::string::npos) {
            tpl.replace(pos, needle.size(), value);
            pos = tpl.find(needle, pos + value.size());
        }
    };

    // Compute a file:// URL for the asset base so relative refs inside the
    // template resolve regardless of the source doc's location.
    std::error_code ec;
    const auto      absAsset = std::filesystem::absolute(m_assetBase, ec);
    const auto      assetUrl = "file://" + (ec ? m_assetBase.string() : absAsset.string());

    replace("{{THEME_URL}}",   m_themeUrl.empty() ? assetUrl + "/themes/hypr-dark.css" : m_themeUrl);
    replace("{{ASSET_BASE}}",  assetUrl);
    replace("{{CONTENT}}",     bodyHtml);
    replace("{{SOURCE_PATH}}", sourcePath);
    return tpl;
}

