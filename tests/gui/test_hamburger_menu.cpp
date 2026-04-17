#include "ui/HamburgerMenu.hpp"

#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>

class TestHamburgerMenu : public QObject {
    Q_OBJECT
  private slots:
    void constructs() {
        CHamburgerMenu m;
        QVERIFY(m.actions().size() > 0);
    }

    void hasThemeSubmenu() {
        CHamburgerMenu m;
        QMenu*         themeMenu = nullptr;
        for (auto* child : m.findChildren<QMenu*>()) {
            if (child->title().contains(QStringLiteral("Theme"), Qt::CaseInsensitive)) {
                themeMenu = child;
                break;
            }
        }
        QVERIFY(themeMenu != nullptr);
    }

    void openActionEmitsSignal() {
        CHamburgerMenu m;
        QSignalSpy     spy(&m, &CHamburgerMenu::openRequested);
        QAction*       openAct = nullptr;
        for (auto* a : m.actions()) {
            if (a->text().contains(QStringLiteral("Open"), Qt::CaseInsensitive) &&
                !a->text().contains(QStringLiteral("New"), Qt::CaseInsensitive)) {
                openAct = a;
                break;
            }
        }
        QVERIFY(openAct != nullptr);
        openAct->trigger();
        QCOMPARE(spy.count(), 1);
    }

    void findActionEmitsSignal() {
        CHamburgerMenu m;
        QSignalSpy     spy(&m, &CHamburgerMenu::findRequested);
        QAction*       findAct = nullptr;
        for (auto* a : m.actions()) {
            if (a->text().contains(QStringLiteral("Find"), Qt::CaseInsensitive)) {
                findAct = a;
                break;
            }
        }
        QVERIFY(findAct != nullptr);
        findAct->trigger();
        QCOMPARE(spy.count(), 1);
    }

    void quitActionEmitsSignal() {
        CHamburgerMenu m;
        QSignalSpy     spy(&m, &CHamburgerMenu::quitRequested);
        QAction*       quitAct = nullptr;
        for (auto* a : m.actions()) {
            if (a->text().contains(QStringLiteral("Quit"), Qt::CaseInsensitive)) {
                quitAct = a;
                break;
            }
        }
        QVERIFY(quitAct != nullptr);
        quitAct->trigger();
        QCOMPARE(spy.count(), 1);
    }

    void setRemoteImagesVisibleTogglesAction() {
        CHamburgerMenu m;
        QAction*       disableRemote = nullptr;
        for (auto* a : m.actions()) {
            if (a->text().contains(QStringLiteral("remote"), Qt::CaseInsensitive)) {
                disableRemote = a;
                break;
            }
        }
        QVERIFY(disableRemote != nullptr);
        QVERIFY(!disableRemote->isVisible()); // hidden by default
        m.setRemoteImagesVisible(true);
        QVERIFY(disableRemote->isVisible());
        m.setRemoteImagesVisible(false);
        QVERIFY(!disableRemote->isVisible());
    }

    void rebuildThemeMenuWithNoManagerDoesNotCrash() {
        CHamburgerMenu m;
        // g_pThemeManager is null in this test context — rebuildThemeMenu must
        // handle that gracefully (early-return, submenu stays cleared).
        m.rebuildThemeMenu(QStringLiteral("hypr-dark"));
        QVERIFY(true); // reached without throwing / segfaulting
    }
};

QTEST_MAIN(TestHamburgerMenu)
#include "test_hamburger_menu.moc"
