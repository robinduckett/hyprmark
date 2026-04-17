#include "ui/HamburgerMenu.hpp"
#include "ui/TitleBar.hpp"

#include <QTest>

class TestTitleBar : public QObject {
    Q_OBJECT
  private slots:
    void constructsAndOwnsMenu() {
        CTitleBar t;
        QVERIFY(t.menu() != nullptr);
    }

    void menuIsHamburgerMenu() {
        CTitleBar t;
        QVERIFY(qobject_cast<CHamburgerMenu*>(t.menu()) != nullptr);
    }
};

QTEST_MAIN(TestTitleBar)
#include "test_title_bar.moc"
