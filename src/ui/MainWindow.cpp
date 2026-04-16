#include "MainWindow.hpp"

#include "../config/ConfigManager.hpp"
#include "../helpers/Log.hpp"
#include "../render/FileWatcher.hpp"
#include "../render/MarkdownRenderer.hpp"
#include "../render/ThemeManager.hpp"
#include "../core/hyprmark.hpp"
#include "EmptyState.hpp"
#include "FindBar.hpp"
#include "HamburgerMenu.hpp"
#include "RenderView.hpp"
#include "TitleBar.hpp"
#include "TocPanel.hpp"
#include "UrlInterceptor.hpp"

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <filesystem>

namespace {
    bool hasAcceptedExtension(const QString& pathOrName) {
        static const QStringList EXTS = {QStringLiteral(".md"), QStringLiteral(".markdown"), QStringLiteral(".mdown"),
                                          QStringLiteral(".mkd"), QStringLiteral(".mkdn"), QStringLiteral(".mdtxt")};
        const QString lower = pathOrName.toLower();
        for (const auto& e : EXTS) {
            if (lower.endsWith(e))
                return true;
        }
        return false;
    }
} // namespace

CMainWindow::CMainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("hyprmark");
    setMinimumSize(600, 400);
    resize(900, 700);
    setAcceptDrops(true);

    // Central: vertical stack of [title bar strip, content stacked widget].
    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_pTitleBar = new CTitleBar(central);
    layout->addWidget(m_pTitleBar);

    m_pFindBar = new CFindBar(central);
    layout->addWidget(m_pFindBar);

    m_pStack      = new QStackedWidget(central);
    m_pEmptyState = new CEmptyState(m_pStack);
    m_pRenderView = new CRenderView(m_pStack);
    m_pStack->addWidget(m_pEmptyState); // index 0
    m_pStack->addWidget(m_pRenderView); // index 1
    m_pStack->setCurrentWidget(m_pEmptyState);
    layout->addWidget(m_pStack, 1);

    setCentralWidget(central);

    // TOC dock — right side, hidden initially unless the config asks otherwise.
    m_pTocPanel = new CTocPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_pTocPanel);
    m_pTocPanel->setVisible(g_pConfigManager && g_pConfigManager->showToc());
    connect(m_pTocPanel, &CTocPanel::navigateTo, this, [this](const QString& id) {
        if (m_pRenderView) m_pRenderView->scrollToAnchor(id);
    });

    // Find-bar wiring. The bar lives in our central widget; the actual
    // search runs inside the QWebEngineView.
    connect(m_pFindBar, &CFindBar::findRequested, this, [this](const QString& q) {
        if (m_pRenderView) m_pRenderView->findText(q);
    });
    connect(m_pFindBar, &CFindBar::findNext,     this, [this]() { if (m_pRenderView) m_pRenderView->findNext(); });
    connect(m_pFindBar, &CFindBar::findPrevious, this, [this]() { if (m_pRenderView) m_pRenderView->findPrevious(); });
    connect(m_pFindBar, &CFindBar::findCleared,  this, [this]() { if (m_pRenderView) m_pRenderView->findClear(); });
    connect(m_pRenderView, &CRenderView::findResult, m_pFindBar, &CFindBar::setResult);

    // Status bar: hidden until a message is pushed.
    statusBar()->hide();

    // hook the empty state's open button
    connect(m_pEmptyState, &CEmptyState::openRequested, this, &CMainWindow::promptForFile);

    // hamburger menu signals
    auto* menu = m_pTitleBar->menu();
    connect(menu, &CHamburgerMenu::openRequested,             this, &CMainWindow::promptForFile);
    connect(menu, &CHamburgerMenu::openInNewWindowRequested,  this, &CMainWindow::promptForFileInNewWindow);
    connect(menu, &CHamburgerMenu::newEmptyWindowRequested,   this, &CMainWindow::newEmptyWindow);
    connect(menu, &CHamburgerMenu::toggleTocRequested,        this, &CMainWindow::toggleToc);
    connect(menu, &CHamburgerMenu::findRequested,        this, &CMainWindow::findInPage);
    connect(menu, &CHamburgerMenu::zoomInRequested,      this, &CMainWindow::zoomIn);
    connect(menu, &CHamburgerMenu::zoomOutRequested,     this, &CMainWindow::zoomOut);
    connect(menu, &CHamburgerMenu::zoomResetRequested,   this, &CMainWindow::zoomReset);
    connect(menu, &CHamburgerMenu::exportPdfRequested,   this, &CMainWindow::exportPdf);
    connect(menu, &CHamburgerMenu::themeSelected,        this, &CMainWindow::applyTheme);
    connect(menu, &CHamburgerMenu::aboutRequested,       this, [this]() {
        QMessageBox::about(this, tr("About hyprmark"),
                           tr("hyprmark v%1\nMarkdown viewer for Hyprland.").arg(HYPRMARK_VERSION));
    });
    connect(menu, &CHamburgerMenu::quitRequested,        this, [this]() { qApp->quit(); });
    connect(menu, &CHamburgerMenu::disableRemoteImagesRequested, this, [this]() {
        if (!g_pConfigManager) return;
        g_pConfigManager->setAllowRemoteImages(false);
        if (m_pRenderView && m_pRenderView->interceptor())
            m_pRenderView->interceptor()->setAllowRemote(false);
        if (m_pTitleBar && m_pTitleBar->menu())
            m_pTitleBar->menu()->setRemoteImagesVisible(false);
        showInfo(tr("Remote images disabled"), 2000);
        // Re-render so the blocker picks this up immediately.
        if (m_hasLoadedContent && !m_currentFile.empty())
            onSourceFileChanged();
    });

    // reflect initial menu state
    if (g_pConfigManager && m_pTitleBar && m_pTitleBar->menu())
        m_pTitleBar->menu()->setRemoteImagesVisible(g_pConfigManager->allowRemoteImages());

    if (g_pConfigManager) {
        m_pRenderView->interceptor()->setAllowRemote(g_pConfigManager->allowRemoteImages());
        m_currentTheme = QString::fromStdString(g_pConfigManager->defaultTheme());
        m_zoom         = static_cast<double>(g_pConfigManager->defaultZoom());
    }

    // React to the URL interceptor seeing a remote image. Emitted from a
    // non-main thread; Qt's queued-connection default handles the marshalling.
    connect(m_pRenderView->interceptor(), &CUrlInterceptor::remoteBlocked,
            this, [this]() { showRemoteImagesPrompt(); });
    if (m_currentTheme.isEmpty() && g_pThemeManager)
        m_currentTheme = QString::fromStdString(g_pThemeManager->defaultName());
    if (g_pThemeManager)
        menu->rebuildThemeMenu(m_currentTheme);

    wireKeybinds();

    // QWebEngineView's internal RenderWidgetHostViewQtDelegateWidget accepts
    // drops on its own to implement Chromium's built-in drag-drop. We want
    // drops to hit our dropEvent() instead, so install a global application
    // event filter and route DragEnter/Drop events to self.
    qApp->installEventFilter(this);

    // Source-file watcher for live-reload of markdown edits.
    m_pSourceWatcher = new CFileWatcher(this);
    connect(m_pSourceWatcher, &CFileWatcher::changed, this, &CMainWindow::onSourceFileChanged);

    // Config hot-reload wiring.
    if (g_pConfigManager) {
        g_pConfigManager->enableHotReload();
        connect(g_pConfigManager.get(), &CConfigManager::settingChanged, this, &CMainWindow::onConfigChanged);
        connect(g_pConfigManager.get(), &CConfigManager::reloaded,       this, &CMainWindow::onConfigReloaded);
    }
}

CMainWindow::~CMainWindow() {
    // Remove the global event filter we installed for drag-and-drop routing.
    // Leaving it in place would let qApp dispatch events to a dangling `this`
    // during QApplication teardown, which crashes at shutdown.
    if (qApp)
        qApp->removeEventFilter(this);
}

void CMainWindow::wireKeybinds() {
    // Every binding is configurable via ~/.config/hypr/hyprmark.conf. Fall back
    // to the spec defaults if ConfigManager isn't available.
    auto keybind = [](const char* action, const char* fallback) -> QString {
        if (!g_pConfigManager)
            return QString::fromUtf8(fallback);
        const auto v = g_pConfigManager->keybind(action);
        return v.empty() ? QString::fromUtf8(fallback) : QString::fromStdString(v);
    };

    auto bind = [this](const QString& seq, auto slot) {
        if (seq.isEmpty())
            return;
        auto* sc = new QShortcut(QKeySequence(seq), this);
        // WindowShortcut (the default): each window handles its own
        // keystrokes so two+ windows don't produce "ambiguous shortcut
        // overload" collisions when they share the same binding.
        sc->setContext(Qt::WindowShortcut);
        connect(sc, &QShortcut::activated, this, slot);
        m_shortcuts.push_back(sc);
    };

    for (auto* sc : m_shortcuts)
        sc->deleteLater();
    m_shortcuts.clear();

    bind(keybind("open",            "Ctrl+O"),     &CMainWindow::promptForFile);
    bind(keybind("open_new_window", "Ctrl+Alt+O"), &CMainWindow::promptForFileInNewWindow);
    bind(keybind("new_window",      "Ctrl+Alt+N"), &CMainWindow::newEmptyWindow);
    bind(keybind("cycle_theme",     "Ctrl+T"),     &CMainWindow::cycleTheme);
    bind(keybind("toggle_toc",  "Ctrl+B"), &CMainWindow::toggleToc);
    bind(keybind("find",        "Ctrl+F"), &CMainWindow::findInPage);
    bind(keybind("zoom_in",     "Ctrl++"), &CMainWindow::zoomIn);
    bind(keybind("zoom_out",    "Ctrl+-"), &CMainWindow::zoomOut);
    bind(keybind("zoom_reset",  "Ctrl+0"), &CMainWindow::zoomReset);
    bind(keybind("export_pdf",  "Ctrl+P"), &CMainWindow::exportPdf);
    // Close THIS window only; app exits when the last one closes via Qt's
    // quitOnLastWindowClosed. "Quit" in the hamburger menu still exits all.
    bind(keybind("close",       "Ctrl+W"), &QWidget::close);
}

bool CMainWindow::isAcceptedMarkdownUrl(const QUrl& url) {
    if (!url.isLocalFile())
        return false;
    return hasAcceptedExtension(url.toLocalFile());
}

void CMainWindow::dragEnterEvent(QDragEnterEvent* event) {
    const auto* mime = event->mimeData();
    if (mime->hasUrls()) {
        for (const auto& u : mime->urls()) {
            if (isAcceptedMarkdownUrl(u)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    if (mime->hasFormat(QStringLiteral("text/markdown")) || mime->hasFormat(QStringLiteral("text/x-markdown"))) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void CMainWindow::dropEvent(QDropEvent* event) {
    const auto* mime = event->mimeData();
    if (mime->hasUrls()) {
        for (const auto& u : mime->urls()) {
            if (isAcceptedMarkdownUrl(u)) {
                openFile(u.toLocalFile().toStdString());
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

bool CMainWindow::eventFilter(QObject* watched, QEvent* event) {
    const auto t = event->type();
    if (t == QEvent::DragEnter || t == QEvent::DragMove) {
        // Only intercept drags that land on us (or a child). Other top-level
        // widgets on the desktop are none of our business.
        if (auto* w = qobject_cast<QWidget*>(watched); w && w->window() == this) {
            auto* dee = static_cast<QDragEnterEvent*>(event);
            const auto* mime = dee->mimeData();
            bool ok = false;
            if (mime->hasUrls()) {
                for (const auto& u : mime->urls()) {
                    if (isAcceptedMarkdownUrl(u)) { ok = true; break; }
                }
            } else if (mime->hasFormat(QStringLiteral("text/markdown")) || mime->hasFormat(QStringLiteral("text/x-markdown"))) {
                ok = true;
            }
            if (ok) {
                dee->acceptProposedAction();
                return true; // consume; no child widget needs to see it
            }
        }
    } else if (t == QEvent::Drop) {
        if (auto* w = qobject_cast<QWidget*>(watched); w && w->window() == this) {
            auto* de = static_cast<QDropEvent*>(event);
            const auto* mime = de->mimeData();
            if (mime->hasUrls()) {
                for (const auto& u : mime->urls()) {
                    if (isAcceptedMarkdownUrl(u)) {
                        openFile(u.toLocalFile().toStdString());
                        de->acceptProposedAction();
                        return true;
                    }
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void CMainWindow::promptForFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Markdown File"), QString(),
        tr("Markdown (*.md *.markdown *.mdown *.mkd *.mkdn *.mdtxt);;All Files (*)"));
    if (path.isEmpty())
        return;
    openFile(path.toStdString());
}

void CMainWindow::promptForFileInNewWindow() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Markdown File in New Window"), QString(),
        tr("Markdown (*.md *.markdown *.mdown *.mkd *.mkdn *.mdtxt);;All Files (*)"));
    if (path.isEmpty() || !g_pHyprmark)
        return;
    g_pHyprmark->newWindow(path.toStdString());
}

void CMainWindow::newEmptyWindow() {
    if (!g_pHyprmark)
        return;
    g_pHyprmark->newWindow({});
}

void CMainWindow::openFile(const std::filesystem::path& path) {
    if (!g_pRenderer || !g_pThemeManager) {
        Debug::log(ERR, "openFile called before renderer/theme manager initialised");
        return;
    }
    m_currentFile = path;

    const auto themeUrl = g_pThemeManager->themeUrl(m_currentTheme.toStdString());
    g_pRenderer->setThemeUrl(themeUrl.empty() ? g_pThemeManager->themeUrl(g_pThemeManager->defaultName()) : themeUrl);

    const auto rendered = g_pRenderer->renderFile(path);
    setWindowTitle(QStringLiteral("hyprmark — %1").arg(QString::fromStdString(path.filename().string())));

    const QString body       = QString::fromStdString(rendered.html);
    const QString sourcePath = QString::fromStdString(rendered.sourcePath);

    if (!m_hasLoadedContent) {
        QUrl baseUrl;
        if (!rendered.sourceDir.empty())
            baseUrl = QUrl::fromLocalFile(QString::fromStdString(rendered.sourceDir) + QDir::separator());
        m_pRenderView->loadInitial(QString::fromStdString(rendered.fullPage), baseUrl);
        m_hasLoadedContent = true;
    } else {
        m_pRenderView->swapBody(body, sourcePath);
    }

    m_pStack->setCurrentWidget(m_pRenderView);

    if (std::abs(m_zoom - 1.0) > 1e-6)
        m_pRenderView->setScale(m_zoom);

    if (m_pTocPanel)
        m_pTocPanel->rebuild(rendered.headings);

    if (g_pConfigManager && g_pConfigManager->liveReload())
        attachSourceWatcher(QString::fromStdString(m_currentFile.string()));
    else
        detachSourceWatcher();
}

void CMainWindow::attachSourceWatcher(const QString& path) {
    if (!m_pSourceWatcher)
        return;
    m_pSourceWatcher->watch(path);
}

void CMainWindow::detachSourceWatcher() {
    if (m_pSourceWatcher)
        m_pSourceWatcher->watch(QString());
}

void CMainWindow::onSourceFileChanged() {
    if (m_currentFile.empty() || !m_hasLoadedContent)
        return;
    if (!g_pRenderer)
        return;
    // Read + re-render + swap in place, preserving scroll.
    const auto rendered = g_pRenderer->renderFile(m_currentFile);
    m_pRenderView->swapBody(QString::fromStdString(rendered.html), QString::fromStdString(rendered.sourcePath));
    if (m_pTocPanel)
        m_pTocPanel->rebuild(rendered.headings);
    showInfo(tr("Reloaded"), 1500);
}

void CMainWindow::onConfigChanged(const QString& key) {
    if (!g_pConfigManager)
        return;
    const auto k = key.toStdString();

    if (k == "general:default_theme") {
        applyTheme(QString::fromStdString(g_pConfigManager->defaultTheme()));
    } else if (k == "general:default_zoom") {
        m_zoom = g_pConfigManager->defaultZoom();
        if (m_pRenderView) m_pRenderView->setScale(m_zoom);
    } else if (k == "general:live_reload") {
        if (g_pConfigManager->liveReload() && !m_currentFile.empty())
            attachSourceWatcher(QString::fromStdString(m_currentFile.string()));
        else
            detachSourceWatcher();
    } else if (k == "general:enable_math" || k == "general:enable_mermaid" || k == "general:allow_remote_images") {
        if (k == "general:allow_remote_images" && m_pRenderView && m_pRenderView->interceptor())
            m_pRenderView->interceptor()->setAllowRemote(g_pConfigManager->allowRemoteImages());
        // Trigger a re-render so CSS/JS observe the new state.
        if (m_hasLoadedContent && !m_currentFile.empty())
            onSourceFileChanged();
    } else if (k.starts_with("keybind:")) {
        wireKeybinds();
    }
}

void CMainWindow::onConfigReloaded() {
    showInfo(tr("Config reloaded"), 1500);
}

void CMainWindow::applyTheme(const QString& name) {
    if (!g_pThemeManager)
        return;
    const auto url = g_pThemeManager->themeUrl(name.toStdString());
    if (url.empty()) {
        showWarn(tr("Theme '%1' not found").arg(name));
        return;
    }
    m_currentTheme = name;
    if (g_pRenderer)
        g_pRenderer->setThemeUrl(url);
    if (m_hasLoadedContent)
        m_pRenderView->swapTheme(QString::fromStdString(url));
    if (m_pTitleBar && m_pTitleBar->menu())
        m_pTitleBar->menu()->rebuildThemeMenu(m_currentTheme);
    showInfo(tr("Theme: %1").arg(name));
}

void CMainWindow::cycleTheme() {
    if (!g_pThemeManager)
        return;
    const auto themes = g_pThemeManager->listThemes();
    if (themes.empty())
        return;
    auto it = std::find_if(themes.begin(), themes.end(),
                           [&](const STheme& t) { return QString::fromStdString(t.name) == m_currentTheme; });
    const auto next = (it == themes.end()) ? themes.begin() : std::next(it);
    applyTheme(QString::fromStdString((next == themes.end() ? themes.front() : *next).name));
}

void CMainWindow::zoomIn() {
    m_zoom = std::min(m_zoom + 0.1, 3.0);
    if (m_pRenderView) m_pRenderView->setScale(m_zoom);
    showInfo(tr("Zoom: %1%").arg(int(m_zoom * 100)));
}

void CMainWindow::zoomOut() {
    m_zoom = std::max(m_zoom - 0.1, 0.4);
    if (m_pRenderView) m_pRenderView->setScale(m_zoom);
    showInfo(tr("Zoom: %1%").arg(int(m_zoom * 100)));
}

void CMainWindow::zoomReset() {
    m_zoom = 1.0;
    if (m_pRenderView) m_pRenderView->setScale(m_zoom);
    showInfo(tr("Zoom: 100%"));
}

void CMainWindow::findInPage() {
    if (!m_pFindBar)
        return;
    if (m_pFindBar->isVisible())
        m_pFindBar->dismiss();
    else
        m_pFindBar->activate();
}

void CMainWindow::toggleToc() {
    if (!m_pTocPanel)
        return;
    m_pTocPanel->setVisible(!m_pTocPanel->isVisible());
    showInfo(m_pTocPanel->isVisible() ? tr("TOC: on") : tr("TOC: off"), 1500);
}

void CMainWindow::exportPdf() {
    if (!m_hasLoadedContent) {
        showWarn(tr("Open a document before exporting to PDF."));
        return;
    }
    // Default filename: <source>.pdf in the user's home if we have a source.
    QString defaultName = QStringLiteral("hyprmark.pdf");
    if (!m_currentFile.empty()) {
        defaultName = QString::fromStdString(m_currentFile.filename().replace_extension(".pdf").string());
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export to PDF"), defaultName, tr("PDF (*.pdf)"));
    if (path.isEmpty())
        return;

    connect(m_pRenderView->page(), &QWebEnginePage::pdfPrintingFinished, this,
            [this](const QString& filePath, bool success) {
                if (success)
                    showInfo(tr("Exported to %1").arg(filePath));
                else
                    showError(tr("PDF export failed: %1").arg(filePath));
            }, Qt::SingleShotConnection);

    m_pRenderView->page()->printToPdf(path);
}

void CMainWindow::showInfo(const QString& text, int timeoutMs) {
    statusBar()->showMessage(text, timeoutMs);
    statusBar()->setVisible(true);
    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, this, [this]() {
            if (statusBar()->currentMessage().isEmpty())
                statusBar()->hide();
        });
    }
}

void CMainWindow::showWarn(const QString& text) {
    statusBar()->showMessage(QStringLiteral("⚠ ") + text);
    statusBar()->setStyleSheet("background: rgba(255,180,0,0.2);");
    statusBar()->setVisible(true);
}

void CMainWindow::showError(const QString& text) {
    statusBar()->showMessage(QStringLiteral("✖ ") + text);
    statusBar()->setStyleSheet("background: rgba(255,80,80,0.2);");
    statusBar()->setVisible(true);
}

void CMainWindow::showRemoteImagesPrompt() {
    if (!g_pConfigManager || g_pConfigManager->allowRemoteImages())
        return;
    if (m_pRemotePrompt)
        return; // already showing

    auto* bar = new QWidget(this);
    auto* hl  = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 2, 8, 2);
    hl->setSpacing(8);

    auto* lbl = new QLabel(tr("⚠ This document contains remote images that were blocked."));
    auto* btn = new QPushButton(tr("Enable remote images"));
    auto* x   = new QToolButton();
    x->setText(QStringLiteral("×"));
    x->setAutoRaise(true);

    hl->addWidget(lbl, 1);
    hl->addWidget(btn);
    hl->addWidget(x);

    bar->setStyleSheet("background: rgba(255,180,0,0.18);");
    statusBar()->addWidget(bar, 1);
    statusBar()->setVisible(true);

    connect(btn, &QPushButton::clicked, this, [this]() {
        if (!g_pConfigManager) return;
        g_pConfigManager->setAllowRemoteImages(true);
        if (m_pRenderView && m_pRenderView->interceptor())
            m_pRenderView->interceptor()->setAllowRemote(true);
        if (m_pTitleBar && m_pTitleBar->menu())
            m_pTitleBar->menu()->setRemoteImagesVisible(true);
        hideRemoteImagesPrompt();
        if (m_hasLoadedContent && !m_currentFile.empty())
            onSourceFileChanged();
        showInfo(tr("Remote images enabled"), 2000);
    });
    connect(x, &QToolButton::clicked, this, [this]() { hideRemoteImagesPrompt(); });

    m_pRemotePrompt = bar;
}

void CMainWindow::hideRemoteImagesPrompt() {
    if (!m_pRemotePrompt)
        return;
    statusBar()->removeWidget(m_pRemotePrompt);
    m_pRemotePrompt->deleteLater();
    m_pRemotePrompt = nullptr;
    if (statusBar()->currentMessage().isEmpty())
        statusBar()->hide();
}
