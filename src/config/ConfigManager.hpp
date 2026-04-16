#pragma once

#include "../defines.hpp"

#include <QObject>
#include <hyprlang.hpp>

#include <optional>
#include <string>

class CFileWatcher;

class CConfigManager : public QObject {
    Q_OBJECT

  public:
    explicit CConfigManager(std::string configPath);
    ~CConfigManager() override;

    // register defaults + parse the file (if any)
    void init();

    // Begin watching the config file for changes; diffs against the previous
    // snapshot and emits one settingChanged signal per changed key.
    void enableHotReload();

  signals:
    // Emitted when a key's value changed as the result of a reload.
    // `key` is the full "section:name" path.
    void settingChanged(QString key);
    void reloaded(); // fired after all deltas, even if zero

  public:
#ifndef Q_MOC_RUN
    template <typename T>
    Hyprlang::CSimpleConfigValue<T> getValue(const std::string& name) {
        return Hyprlang::CSimpleConfigValue<T>(&m_config, name.c_str());
    }
#endif

    // strongly-typed convenience getters
    std::string defaultTheme();
    bool        liveReload();
    bool        showToc();
    float       defaultZoom();
    bool        enableMath();
    bool        enableMermaid();
    bool        allowRemoteImages();

    // keybind section (all strings)
    std::string keybind(const std::string& action);

    // the config path we ended up using (for diagnostics + watcher wiring)
    const std::string& currentPath() const {
        return m_configCurrentPath;
    }

    // Persist a single key back to the config file. Preserves surrounding
    // comments and user whitespace. Called by the remote-images opt-in flow.
    // Returns true on success, false on I/O error.
    //
    // `section` is e.g. "general", `key` e.g. "allow_remote_images", `value`
    // is the literal token written to the right of '='.
    bool writeKey(const std::string& section, const std::string& key, const std::string& value);

    // Convenience wrapper that also updates the in-memory parsed value.
    void setAllowRemoteImages(bool enabled);

  private:
    struct SSnapshot {
        std::string default_theme;
        bool        live_reload = true;
        bool        show_toc = false;
        float       default_zoom = 1.0f;
        bool        enable_math = true;
        bool        enable_mermaid = true;
        bool        allow_remote_images = false;
        std::string kb_open, kb_open_new_window, kb_new_window, kb_cycle_theme, kb_toggle_toc, kb_find, kb_zoom_in, kb_zoom_out, kb_zoom_reset, kb_export_pdf, kb_close;
    };
    SSnapshot snapshot() const;

    std::string       m_configCurrentPath;
    bool              m_configFileExists = false;
    Hyprlang::CConfig m_config;

    CFileWatcher* m_pWatcher = nullptr;
    SSnapshot     m_lastSnapshot;
};

inline UP<CConfigManager> g_pConfigManager;
