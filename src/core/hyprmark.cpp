#include "hyprmark.hpp"
#include "../config/ConfigManager.hpp"
#include "../helpers/Log.hpp"
#include "../render/MarkdownRenderer.hpp"
#include "../render/ThemeManager.hpp"
#include "../ui/MainWindow.hpp"
#include "FileOpenRouter.hpp"
#include "IpcServer.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QWindow>

#include <algorithm>
#include <filesystem>

CHyprmark::CHyprmark() = default;

CHyprmark::~CHyprmark() {
    shutdown(); // no-op if run() already tore everything down
}

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

    // Documents opened via the platform (macOS Finder "Open With", `open -a`)
    // arrive as QFileOpenEvent on the application object, not argv.
    m_pFileOpenRouter = new CFileOpenRouter([this](const std::string& p) { openFromSystem(p); }, m_pApp.get());
    m_pApp->installEventFilter(m_pFileOpenRouter);

    newWindow(filePath); // empty path => empty-state

    // Start the IPC server so subsequent invocations dispatch to us. It
    // queries g_pHyprmark->activeWindow() on each command so it naturally
    // tracks focus as new windows get created/closed.
    g_pIpcServer = makeUnique<CIpcServer>();
    if (!g_pIpcServer->listen())
        Debug::log(WARN, "IPC server disabled; --dispatch will not work while this is running.");

    const int rc = m_pApp->exec();

    // Tear down here, while main() is still on the stack, rather than letting
    // the globals die in the static-destructor phase after main() returns.
    // QApplication's destructor runs Qt's post-routines, which is where Qt
    // WebEngine shuts Chromium down (GPU thread, Skia contexts, accessibility
    // caches, ...). By the time static destructors run, function-local
    // statics inside Qt/Chromium are already gone and that shutdown crashes
    // (SIGSEGV in GrDirectContext / qAccessibleCleanup on macOS, surfacing as
    // "hyprmark quit unexpectedly" a while after the window closed).
    shutdown();
    return rc;
}

void CHyprmark::shutdown() {
    if (!m_pApp)
        return;

    // 1. Windows. Every QWebEngineView/QWebEnginePage must be destroyed
    //    before WebEngine's post-routine runs. Windows closed via close()
    //    have a DeferredDelete pending; anything still alive (quit via IPC
    //    or the menu with several windows open) is deleted directly, which
    //    also cancels its pending deferred delete.
    const auto windows = m_windows; // destroyed() erases from m_windows as we go
    for (auto* w : windows)
        delete w;
    m_windows.clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // 2. QObject-owning globals that need a live event dispatcher to die
    //    cleanly (QLocalServer unlinks its socket; CConfigManager owns a
    //    QFileSystemWatcher with a background thread on macOS).
    g_pIpcServer.reset();
    g_pConfigManager.reset();

    // 3. Plain globals created in run(), reverse order.
    g_pRenderer.reset();
    g_pThemeManager.reset();

    // 4. The application itself (runs WebEngine/Chromium shutdown).
    m_pFileOpenRouter = nullptr; // child of m_pApp
    m_pApp.reset();
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

void CHyprmark::openFromSystem(const std::string& filePath) {
    // Prefer the window the user is looking at if it is still empty, then any
    // other empty window, otherwise a fresh one. This makes the common
    // Finder flow (app launches empty, then the open request lands) fill
    // the initial window instead of leaving a stray empty one behind.
    CMainWindow* target = nullptr;
    if (auto* a = activeWindow(); a && !a->hasDocument())
        target = a;
    if (!target) {
        auto it = std::find_if(m_windows.begin(), m_windows.end(), [](CMainWindow* w) { return w && !w->hasDocument(); });
        if (it != m_windows.end())
            target = *it;
    }

    if (!target) {
        newWindow(filePath);
        return;
    }
    target->openFile(filePath);
    target->show();
    target->raise();
    target->activateWindow();
}

CMainWindow* CHyprmark::activeWindow() const {
    if (auto* a = qobject_cast<CMainWindow*>(QApplication::activeWindow()))
        return a;
    if (!m_windows.empty())
        return m_windows.back();
    return nullptr;
}
