#include "HamburgerMenu.hpp"

#include "../render/ThemeManager.hpp"

#include <QAction>
#include <QActionGroup>

CHamburgerMenu::CHamburgerMenu(QWidget* parent) : QMenu(parent) {
    auto* openAct = addAction(tr("Open…"));
    connect(openAct, &QAction::triggered, this, &CHamburgerMenu::openRequested);

    auto* openNewAct = addAction(tr("Open in New Window…"));
    connect(openNewAct, &QAction::triggered, this, &CHamburgerMenu::openInNewWindowRequested);

    auto* newEmptyAct = addAction(tr("New Empty Window"));
    connect(newEmptyAct, &QAction::triggered, this, &CHamburgerMenu::newEmptyWindowRequested);

    m_pThemeMenu = addMenu(tr("Theme"));

    auto* tocAct = addAction(tr("Toggle TOC"));
    connect(tocAct, &QAction::triggered, this, &CHamburgerMenu::toggleTocRequested);

    auto* findAct = addAction(tr("Find…"));
    connect(findAct, &QAction::triggered, this, &CHamburgerMenu::findRequested);

    addSeparator();

    auto* zoomInAct  = addAction(tr("Zoom In"));
    auto* zoomOutAct = addAction(tr("Zoom Out"));
    auto* zoomResetAct = addAction(tr("Reset Zoom"));
    connect(zoomInAct,    &QAction::triggered, this, &CHamburgerMenu::zoomInRequested);
    connect(zoomOutAct,   &QAction::triggered, this, &CHamburgerMenu::zoomOutRequested);
    connect(zoomResetAct, &QAction::triggered, this, &CHamburgerMenu::zoomResetRequested);

    addSeparator();

    auto* exportAct = addAction(tr("Export PDF…"));
    connect(exportAct, &QAction::triggered, this, &CHamburgerMenu::exportPdfRequested);

    m_pDisableRemoteAct = addAction(tr("Disable remote images"));
    m_pDisableRemoteAct->setVisible(false);
    connect(m_pDisableRemoteAct, &QAction::triggered, this, &CHamburgerMenu::disableRemoteImagesRequested);

    addSeparator();

    auto* aboutAct = addAction(tr("About hyprmark"));
    connect(aboutAct, &QAction::triggered, this, &CHamburgerMenu::aboutRequested);

    auto* quitAct = addAction(tr("Quit"));
    connect(quitAct, &QAction::triggered, this, &CHamburgerMenu::quitRequested);
}

void CHamburgerMenu::rebuildThemeMenu(const QString& currentTheme) {
    if (!m_pThemeMenu)
        return;
    m_pThemeMenu->clear();
    delete m_pThemeGroup;
    m_pThemeGroup = new QActionGroup(m_pThemeMenu);
    m_pThemeGroup->setExclusive(true);

    if (!g_pThemeManager)
        return;

    const auto themes = g_pThemeManager->listThemes();
    for (const auto& t : themes) {
        auto* a = m_pThemeMenu->addAction(QString::fromStdString(t.name));
        a->setCheckable(true);
        a->setChecked(QString::fromStdString(t.name) == currentTheme);
        a->setActionGroup(m_pThemeGroup);
        const auto name = QString::fromStdString(t.name);
        connect(a, &QAction::triggered, this, [this, name]() { emit themeSelected(name); });
    }
}

void CHamburgerMenu::setRemoteImagesVisible(bool visible) {
    if (m_pDisableRemoteAct)
        m_pDisableRemoteAct->setVisible(visible);
}
