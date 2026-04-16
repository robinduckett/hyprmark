#include "ConfigManager.hpp"

#include "../helpers/Log.hpp"
#include "../render/FileWatcher.hpp"

#include <hyprlang.hpp>
#include <hyprutils/path/Path.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

    // Find hyprmark.conf in the standard XDG/HOME locations. Returns empty
    // string if nothing is found (caller falls back to defaults-only mode).
    std::string findDefaultConfigPath() {
        try {
            const auto paths = Hyprutils::Path::findConfig("hyprmark");
            if (paths.first.has_value())
                return paths.first.value();
        } catch (...) {
            // fall through
        }
        return {};
    }

    constexpr const char* DEFAULT_THEME    = "hypr-dark";
    constexpr float       DEFAULT_ZOOM     = 1.0f;

    // built-in section-qualified key names — kept together so we can compare
    // against them for diff-apply in M8.
    constexpr const char* KEY_DEFAULT_THEME       = "general:default_theme";
    constexpr const char* KEY_LIVE_RELOAD         = "general:live_reload";
    constexpr const char* KEY_SHOW_TOC            = "general:show_toc";
    constexpr const char* KEY_DEFAULT_ZOOM        = "general:default_zoom";
    constexpr const char* KEY_ENABLE_MATH         = "general:enable_math";
    constexpr const char* KEY_ENABLE_MERMAID      = "general:enable_mermaid";
    constexpr const char* KEY_ALLOW_REMOTE_IMAGES = "general:allow_remote_images";

} // namespace

CConfigManager::CConfigManager(std::string configPath) :
    QObject(nullptr),
    m_configCurrentPath(configPath.empty() ? findDefaultConfigPath() : configPath),
    m_configFileExists(!m_configCurrentPath.empty() && std::filesystem::exists(m_configCurrentPath)),
    m_config(m_configCurrentPath.empty() ? "" : m_configCurrentPath.c_str(),
             Hyprlang::SConfigOptions{
                 .throwAllErrors    = false, // surface as warnings, keep parsing
                 .allowMissingConfig = true,
             }) {
}

CConfigManager::~CConfigManager() = default;

void CConfigManager::init() {
    // ------- general -------
    m_config.addConfigValue(KEY_DEFAULT_THEME, Hyprlang::STRING{DEFAULT_THEME});
    m_config.addConfigValue(KEY_LIVE_RELOAD, Hyprlang::INT{1});
    m_config.addConfigValue(KEY_SHOW_TOC, Hyprlang::INT{0});
    m_config.addConfigValue(KEY_DEFAULT_ZOOM, Hyprlang::FLOAT{DEFAULT_ZOOM});
    m_config.addConfigValue(KEY_ENABLE_MATH, Hyprlang::INT{1});
    m_config.addConfigValue(KEY_ENABLE_MERMAID, Hyprlang::INT{1});
    m_config.addConfigValue(KEY_ALLOW_REMOTE_IMAGES, Hyprlang::INT{0});

    // ------- keybind -------
    m_config.addConfigValue("keybind:open", Hyprlang::STRING{"Ctrl+O"});
    m_config.addConfigValue("keybind:open_new_window", Hyprlang::STRING{"Ctrl+Alt+O"});
    m_config.addConfigValue("keybind:new_window", Hyprlang::STRING{"Ctrl+Alt+N"});
    m_config.addConfigValue("keybind:cycle_theme", Hyprlang::STRING{"Ctrl+T"});
    m_config.addConfigValue("keybind:toggle_toc", Hyprlang::STRING{"Ctrl+B"});
    m_config.addConfigValue("keybind:find", Hyprlang::STRING{"Ctrl+F"});
    m_config.addConfigValue("keybind:zoom_in", Hyprlang::STRING{"Ctrl+Plus"});
    m_config.addConfigValue("keybind:zoom_out", Hyprlang::STRING{"Ctrl+Minus"});
    m_config.addConfigValue("keybind:zoom_reset", Hyprlang::STRING{"Ctrl+0"});
    m_config.addConfigValue("keybind:export_pdf", Hyprlang::STRING{"Ctrl+P"});
    m_config.addConfigValue("keybind:close", Hyprlang::STRING{"Ctrl+W"});

    m_config.commence();

    if (m_configFileExists) {
        const auto result = m_config.parse();
        if (result.error)
            Debug::log(WARN, "Config {} has errors:\n{}\nProceeding with available values.", m_configCurrentPath, result.getError());
        else
            Debug::log(LOG, "Loaded config from {}", m_configCurrentPath);
    } else if (m_configCurrentPath.empty())
        Debug::log(LOG, "No config file found in XDG paths; using built-in defaults.");
    else
        Debug::log(LOG, "Config file {} does not exist; using built-in defaults.", m_configCurrentPath);

    m_lastSnapshot = snapshot();
}

void CConfigManager::enableHotReload() {
    if (m_pWatcher || m_configCurrentPath.empty())
        return;

    m_pWatcher = new CFileWatcher(this);
    m_pWatcher->watch(QString::fromStdString(m_configCurrentPath));

    connect(m_pWatcher, &CFileWatcher::changed, this, [this](const QString&) {
        Debug::log(LOG, "Config reload: {}", m_configCurrentPath);
        const auto before = m_lastSnapshot;
        const auto result = m_config.parse();
        if (result.error)
            Debug::log(WARN, "Config reload errors:\n{}\nKeeping whatever parsed cleanly.", result.getError());
        m_lastSnapshot = snapshot();

        auto diff = [&](const char* key, auto before, auto after) {
            if (before != after)
                emit settingChanged(QString::fromUtf8(key));
        };
        diff("general:default_theme",       before.default_theme,       m_lastSnapshot.default_theme);
        diff("general:live_reload",         before.live_reload,         m_lastSnapshot.live_reload);
        diff("general:show_toc",            before.show_toc,            m_lastSnapshot.show_toc);
        diff("general:default_zoom",        before.default_zoom,        m_lastSnapshot.default_zoom);
        diff("general:enable_math",         before.enable_math,         m_lastSnapshot.enable_math);
        diff("general:enable_mermaid",      before.enable_mermaid,      m_lastSnapshot.enable_mermaid);
        diff("general:allow_remote_images", before.allow_remote_images, m_lastSnapshot.allow_remote_images);

        diff("keybind:open",            before.kb_open,            m_lastSnapshot.kb_open);
        diff("keybind:open_new_window", before.kb_open_new_window, m_lastSnapshot.kb_open_new_window);
        diff("keybind:new_window",      before.kb_new_window,      m_lastSnapshot.kb_new_window);
        diff("keybind:cycle_theme",     before.kb_cycle_theme,     m_lastSnapshot.kb_cycle_theme);
        diff("keybind:toggle_toc",   before.kb_toggle_toc,   m_lastSnapshot.kb_toggle_toc);
        diff("keybind:find",         before.kb_find,         m_lastSnapshot.kb_find);
        diff("keybind:zoom_in",      before.kb_zoom_in,      m_lastSnapshot.kb_zoom_in);
        diff("keybind:zoom_out",     before.kb_zoom_out,     m_lastSnapshot.kb_zoom_out);
        diff("keybind:zoom_reset",   before.kb_zoom_reset,   m_lastSnapshot.kb_zoom_reset);
        diff("keybind:export_pdf",   before.kb_export_pdf,   m_lastSnapshot.kb_export_pdf);
        diff("keybind:close",        before.kb_close,        m_lastSnapshot.kb_close);

        emit reloaded();
    });
}

CConfigManager::SSnapshot CConfigManager::snapshot() const {
    // mutable getter; the template needs non-const m_config. Use const_cast
    // since the mutation is just caching inside CSimpleConfigValue.
    auto* self = const_cast<CConfigManager*>(this);
    SSnapshot s;
    s.default_theme       = self->defaultTheme();
    s.live_reload         = self->liveReload();
    s.show_toc            = self->showToc();
    s.default_zoom        = self->defaultZoom();
    s.enable_math         = self->enableMath();
    s.enable_mermaid      = self->enableMermaid();
    s.allow_remote_images = self->allowRemoteImages();
    s.kb_open             = self->keybind("open");
    s.kb_open_new_window  = self->keybind("open_new_window");
    s.kb_new_window       = self->keybind("new_window");
    s.kb_cycle_theme      = self->keybind("cycle_theme");
    s.kb_toggle_toc       = self->keybind("toggle_toc");
    s.kb_find             = self->keybind("find");
    s.kb_zoom_in          = self->keybind("zoom_in");
    s.kb_zoom_out         = self->keybind("zoom_out");
    s.kb_zoom_reset       = self->keybind("zoom_reset");
    s.kb_export_pdf       = self->keybind("export_pdf");
    s.kb_close            = self->keybind("close");
    return s;
}

std::string CConfigManager::defaultTheme() {
    const auto v = getValue<Hyprlang::STRING>(KEY_DEFAULT_THEME);
    return std::string(*v ? *v : DEFAULT_THEME);
}

bool CConfigManager::liveReload() {
    return *getValue<Hyprlang::INT>(KEY_LIVE_RELOAD) != 0;
}

bool CConfigManager::showToc() {
    return *getValue<Hyprlang::INT>(KEY_SHOW_TOC) != 0;
}

float CConfigManager::defaultZoom() {
    return *getValue<Hyprlang::FLOAT>(KEY_DEFAULT_ZOOM);
}

bool CConfigManager::enableMath() {
    return *getValue<Hyprlang::INT>(KEY_ENABLE_MATH) != 0;
}

bool CConfigManager::enableMermaid() {
    return *getValue<Hyprlang::INT>(KEY_ENABLE_MERMAID) != 0;
}

bool CConfigManager::allowRemoteImages() {
    return *getValue<Hyprlang::INT>(KEY_ALLOW_REMOTE_IMAGES) != 0;
}

std::string CConfigManager::keybind(const std::string& action) {
    const auto v = getValue<Hyprlang::STRING>(std::string("keybind:") + action);
    return std::string(*v ? *v : "");
}

namespace {
    // Determine where to write the config if none has been created yet. The
    // XDG config dir is preferred; we create intermediate directories as
    // needed. Returns an empty path if we cannot determine a location.
    std::filesystem::path resolveWritePath(const std::string& currentPath) {
        if (!currentPath.empty())
            return currentPath;

        const char* xdg  = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        std::filesystem::path base;
        if (xdg && *xdg)
            base = xdg;
        else if (home && *home)
            base = std::filesystem::path(home) / ".config";
        else
            return {};

        std::error_code ec;
        const auto dir = base / "hypr";
        std::filesystem::create_directories(dir, ec);
        return dir / "hyprmark.conf";
    }

    // Atomic write: write to tmp, rename over target.
    bool atomicWrite(const std::filesystem::path& target, const std::string& content) {
        const auto tmp = target.string() + ".hyprmark.tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) return false;
            f.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!f) return false;
        }
        std::error_code ec;
        std::filesystem::rename(tmp, target, ec);
        if (ec) {
            std::filesystem::remove(tmp);
            return false;
        }
        return true;
    }
}

bool CConfigManager::writeKey(const std::string& section, const std::string& key, const std::string& value) {
    const auto targetPath = resolveWritePath(m_configCurrentPath);
    if (targetPath.empty()) {
        Debug::log(ERR, "writeKey: no XDG_CONFIG_HOME nor HOME to locate config directory");
        return false;
    }

    // Read current contents (may be empty if file doesn't exist yet).
    std::string original;
    {
        std::ifstream f(targetPath, std::ios::binary);
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            original = ss.str();
        }
    }

    // Split into lines preserving EOLs.
    std::vector<std::string> lines;
    {
        std::string cur;
        for (char c : original) {
            cur.push_back(c);
            if (c == '\n') { lines.push_back(std::move(cur)); cur.clear(); }
        }
        if (!cur.empty()) lines.push_back(std::move(cur));
    }

    // Simple state machine: walk lines; detect section headers; when inside
    // the target section, find or insert the key; else, append a new section.
    auto trim = [](const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
        while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r')) --b;
        return s.substr(a, b - a);
    };

    bool inTarget = false;
    bool replaced = false;
    std::string currentSection;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        const auto   t   = trim(line);

        if (!t.empty() && t.back() == '{' && t.size() > 1) {
            const auto name = trim(t.substr(0, t.size() - 1));
            currentSection  = name;
            inTarget        = (name == section);
            continue;
        }
        if (t == "}") {
            if (inTarget && !replaced) {
                // Insert key just before the closing brace.
                std::string indent = "    ";
                lines.insert(lines.begin() + i, indent + key + " = " + value + "\n");
                replaced = true;
                ++i; // skip past inserted line
            }
            inTarget = false;
            continue;
        }

        if (inTarget && !replaced) {
            // look for `key = ...` (ignoring comments)
            std::string noComment = line;
            auto hashPos = noComment.find('#');
            // simple: if # is outside the key token, strip from there
            if (hashPos != std::string::npos) noComment = noComment.substr(0, hashPos);
            auto eq = noComment.find('=');
            if (eq != std::string::npos) {
                auto lhs = trim(noComment.substr(0, eq));
                if (lhs == key) {
                    // Preserve leading whitespace of the original line.
                    size_t leading = 0;
                    while (leading < line.size() && (line[leading] == ' ' || line[leading] == '\t'))
                        ++leading;
                    std::string rewritten = line.substr(0, leading) + key + " = " + value;
                    if (!line.empty() && line.back() == '\n')
                        rewritten.push_back('\n');
                    lines[i] = rewritten;
                    replaced = true;
                }
            }
        }
    }

    if (!replaced) {
        // Section didn't exist — append a fresh block at the end.
        if (!lines.empty() && lines.back().find_last_not_of(" \t\r\n") != std::string::npos
            && (lines.back().back() != '\n'))
            lines.back().push_back('\n');
        if (!lines.empty()) lines.push_back("\n");
        lines.push_back(section + " {\n");
        lines.push_back(std::string("    ") + key + " = " + value + "\n");
        lines.push_back("}\n");
    }

    std::string out;
    for (const auto& l : lines) out += l;

    // Silence the watcher so our own write doesn't trigger a reload.
    if (m_pWatcher)
        m_pWatcher->muteFor(300);

    if (!atomicWrite(targetPath, out)) {
        Debug::log(ERR, "writeKey: atomic write to {} failed", targetPath.string());
        return false;
    }

    // Keep the in-memory path up to date in case we created the file for the
    // first time.
    m_configCurrentPath = targetPath.string();
    m_configFileExists  = true;
    return true;
}

void CConfigManager::setAllowRemoteImages(bool enabled) {
    m_config.parseDynamic("general:allow_remote_images", enabled ? "1" : "0");
    m_lastSnapshot.allow_remote_images = enabled;
    writeKey("general", "allow_remote_images", enabled ? "true" : "false");
}
