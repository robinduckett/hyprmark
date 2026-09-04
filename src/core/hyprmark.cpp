#include "hyprmark.hpp"
#include "../config/ConfigManager.hpp"
#include "../helpers/Log.hpp"
#include "../render/MarkdownRenderer.hpp"
#include "../render/ThemeManager.hpp"
#include "../ui/MainWindow.hpp"
#include "IpcServer.hpp"

#include <QApplication>
#include <QWindow>

#include <algorithm>
#include <filesystem>

CHyprmark::CHyprmark() = default;

CHyprmark::~CHyprmark() = default;

int CHyprmark::run(int argc, char** argv, const std::string& configPath, const std::string& filePath) {
    m_pApp        = std::make_unique<QApplication>(argc, argv);
    m_pApp->setApplicationName("hyprmark");
    m_pApp->setApplicationDisplayName("hyprmark");
    m_pApp->setOrganizationName("hypr");
    m_pApp->setApplicationVersion(HYPRMARK_VERSION);
    m_pApp->setDesktopFileName("hyprmark");

    Debug::log(LOG, "Qt {} on {}", qVersion(), QGuiApplication::platformName().toStdString());

    (void)configPath; // config already parsed in main() before we got here; accepted here for symmetry with future flows

    // renderer + theme manager are globals but only needed once Qt is up so
    // constructing them here keeps destruction order sane (they hold no Qt
    // state, but future milestones might connect signals).
    g_pThemeManager = makeUnique<CThemeManager>();
    g_pRenderer     = makeUnique<CMarkdownRenderer>();

    // asset base: walk up from the binary looking for assets/template.html.
    // Linux dev builds: build/../assets; macOS .app bundles add extra nesting
    // (hyprmark.app/Contents/MacOS/hyprmark) so we try several ancestors.
    std::error_code             fsEc;
    const std::filesystem::path exeDir = std::filesystem::absolute(std::filesystem::path(argv[0]), fsEc).parent_path();
    bool                        foundAssets = false;
    if (!fsEc) {
#ifdef __APPLE__
        // Installed .app bundle: assets live in Contents/Resources/.
        const auto bundleResources = exeDir.parent_path() / "Resources";
        if (std::filesystem::exists(bundleResources / "template.html")) {
            g_pRenderer->setAssetBase(bundleResources);
            g_pThemeManager->setBuiltinDir(bundleResources / "themes");
            Debug::log(LOG, "Using bundle asset base: {}", bundleResources.string());
            foundAssets = true;
        }
#endif
        if (!foundAssets) {
            auto dir = exeDir;
            for (int i = 0; i < 5 && !dir.empty() && dir.has_parent_path(); ++i) {
                dir = dir.parent_path();
                const auto candidate = dir / "assets";
                if (std::filesystem::exists(candidate / "template.html")) {
                    g_pRenderer->setAssetBase(candidate);
                    g_pThemeManager->setBuiltinDir(candidate / "themes");
                    Debug::log(LOG, "Using dev asset base: {}", candidate.string());
                    foundAssets = true;
                    break;
                }
            }
        }
    }

    m_pApp->setQuitOnLastWindowClosed(true);
    newWindow(filePath); // empty path => empty-state

    // Start the IPC server so subsequent invocations dispatch to us. It
    // queries g_pHyprmark->activeWindow() on each command so it naturally
    // tracks focus as new windows get created/closed.
    g_pIpcServer = makeUnique<CIpcServer>();
    if (!g_pIpcServer->listen())
        Debug::log(WARN, "IPC server disabled; --dispatch will not work while this is running.");

    return m_pApp->exec();
}

CMainWindow* CHyprmark::newWindow(const std::string& filePath) {
    auto* w = new CMainWindow();
    w->setAttribute(Qt::WA_DeleteOnClose, true);
    // Remove from list when it's destroyed so activeWindow()/lookup stays accurate.
    QObject::connect(w, &QObject::destroyed, m_pApp.get(), [this, w](QObject*) {
        std::erase(m_windows, w);
    });
    m_windows.push_back(w);

    if (!filePath.empty())
        w->openFile(filePath);
    w->show();
    w->raise();
    w->activateWindow();
    return w;
}

CMainWindow* CHyprmark::activeWindow() const {
    if (auto* a = qobject_cast<CMainWindow*>(QApplication::activeWindow()))
        return a;
    if (!m_windows.empty())
        return m_windows.back();
    return nullptr;
}
