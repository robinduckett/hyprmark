#include "ui/EmptyState.hpp"

#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

class TestEmptyState : public QObject {
    Q_OBJECT
  private slots:
    void constructs() {
        CEmptyState w;
        QVERIFY(w.findChildren<QPushButton*>().size() >= 1);
    }

    void emitsOpenRequestedOnChooseButtonClick() {
        CEmptyState w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QSignalSpy spy(&w, &CEmptyState::openRequested);
        auto*      btn = w.findChild<QPushButton*>();
        QVERIFY(btn != nullptr);
        QTest::mouseClick(btn, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestEmptyState)
#include "test_empty_state.moc"
