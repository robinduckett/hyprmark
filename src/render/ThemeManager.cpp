#include "ThemeManager.hpp"

#include "../helpers/Log.hpp"

#include <algorithm>
#include <cstdlib>

namespace {
    std::filesystem::path defaultBuiltinDir() {
        return "/usr/share/hyprmark/themes";
    }

    std::filesystem::path defaultUserDir() {
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        std::filesystem::path base;
        if (xdg && *xdg)
            base = xdg;
        else if (home && *home)
            base = std::filesystem::path(home) / ".config";
        else
            return {};
        return base / "hypr" / "hyprmark" / "themes";
    }
}

CThemeManager::CThemeManager()
    : m_builtinDir(defaultBuiltinDir()), m_userDir(defaultUserDir()) {
    rescan();
}

void CThemeManager::setBuiltinDir(const std::filesystem::path& dir) {
    m_builtinDir = dir;
    rescan();
}

void CThemeManager::setUserDir(const std::filesystem::path& dir) {
    m_userDir = dir;
    rescan();
}

void CThemeManager::rescan() {
    m_themes.clear();
    scanDir(m_builtinDir, /*isUser=*/false, m_themes);
    scanDir(m_userDir, /*isUser=*/true, m_themes);
    std::sort(m_themes.begin(), m_themes.end(),
              [](const STheme& a, const STheme& b) { return a.name < b.name; });
}

void CThemeManager::scanDir(const std::filesystem::path& dir, bool isUser, std::vector<STheme>& out) {
    std::error_code ec;
    if (dir.empty() || !std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec)
            break;
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".css")
            continue;
        STheme t;
        t.name   = entry.path().stem().string();
        t.path   = entry.path();
        t.isUser = isUser;
        // user themes override built-ins with the same name
        auto existing = std::find_if(out.begin(), out.end(),
                                     [&](const STheme& s) { return s.name == t.name; });
        if (existing != out.end())
            *existing = t;
        else
            out.push_back(std::move(t));
    }
}

std::vector<STheme> CThemeManager::listThemes() const {
    return m_themes;
}

std::optional<STheme> CThemeManager::findTheme(const std::string& name) const {
    for (const auto& t : m_themes) {
        if (t.name == name)
            return t;
    }
    return std::nullopt;
}

std::string CThemeManager::themeUrl(const std::string& name) const {
    const auto t = findTheme(name);
    if (!t)
        return {};
    std::error_code ec;
    const auto abs = std::filesystem::absolute(t->path, ec);
    return "file://" + (ec ? t->path.string() : abs.string());
}

std::string CThemeManager::defaultName() const {
    if (findTheme("hypr-dark"))
        return "hypr-dark";
    if (!m_themes.empty())
        return m_themes.front().name;
    return "hypr-dark"; // best-effort fallback even if no CSS files exist
}
