#include "ui/FindBar.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>

class TestFindBar : public QObject {
    Q_OBJECT
  private slots:
    void activateShows() {
        CFindBar bar;
        QVERIFY(!bar.isVisible());
        bar.show();
        bar.activate();
        QVERIFY(bar.isVisible());
    }

    void dismissHidesAndEmitsCleared() {
        CFindBar bar;
        bar.show();
        bar.activate();
        QSignalSpy spy(&bar, &CFindBar::findCleared);
        bar.dismiss();
        QVERIFY(!bar.isVisible());
        QCOMPARE(spy.count(), 1);
    }

    void typingEmitsFindRequested() {
        CFindBar bar;
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        bar.activate();
        auto* input = bar.findChild<QLineEdit*>();
        QVERIFY(input != nullptr);
        QSignalSpy spy(&bar, &CFindBar::findRequested);
        QTest::keyClicks(input, "hello");
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().at(0).toString(), QStringLiteral("hello"));
    }

    void setResultUpdatesCounter() {
        CFindBar bar;
        bar.show();
        bar.activate();
        bar.setResult(2, 7);
        auto labels = bar.findChildren<QLabel*>();
        bool found  = false;
        for (auto* l : labels) {
            if (l->text().contains(QStringLiteral("2")) && l->text().contains(QStringLiteral("7"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void setResultZeroClearsCounter() {
        CFindBar bar;
        bar.show();
        bar.activate();
        bar.setResult(1, 3);
        bar.setResult(0, 0);
        auto labels = bar.findChildren<QLabel*>();
        for (auto* l : labels) {
            if (l->objectName() == QStringLiteral("findResult")) {
                QCOMPARE(l->text(), QString());
                return;
            }
        }
        QFAIL("findResult label not found");
    }
};

QTEST_MAIN(TestFindBar)
#include "test_find_bar.moc"
