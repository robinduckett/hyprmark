#pragma once

#include "../defines.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct STheme {
    std::string           name;      // logical name, e.g. "hypr-dark"
    std::filesystem::path path;      // absolute path to the .css file
    bool                  isUser;    // true = from ~/.config/hypr/hyprmark/themes/
};

class CThemeManager {
  public:
    CThemeManager();
    ~CThemeManager() = default;

    // Explicit directories so tests can inject fixtures. Default constructor
    // chooses production defaults.
    void setBuiltinDir(const std::filesystem::path& dir);
    void setUserDir(const std::filesystem::path& dir);

    // Re-scan both directories.
    void rescan();

    std::vector<STheme>    listThemes() const;
    std::optional<STheme>  findTheme(const std::string& name) const;
    std::string            themeUrl(const std::string& name) const; // file:// URL or "" if unknown
    std::string            defaultName() const;

  private:
    void scanDir(const std::filesystem::path& dir, bool isUser, std::vector<STheme>& out);

    std::filesystem::path m_builtinDir;
    std::filesystem::path m_userDir;
    std::vector<STheme>   m_themes;
};

inline UP<CThemeManager> g_pThemeManager;
