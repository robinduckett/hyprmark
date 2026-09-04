#include "core/FileOpenRouter.hpp"

#include <QApplication>
#include <QFileOpenEvent>
#include <QTest>
#include <QUrl>

#include <string>
#include <vector>

// CFileOpenRouter is how macOS "Open With" / `open -a` requests reach the
// app: Qt posts a QFileOpenEvent to the QApplication object. These tests
// synthesise that event so the plumbing is covered on every platform.
class TestFileOpenRouter : public QObject {
    Q_OBJECT

    struct SInstalled {
        CFileOpenRouter router;
        explicit SInstalled(CFileOpenRouter::Handler h) : router(std::move(h)) {
            qApp->installEventFilter(&router);
        }
        ~SInstalled() {
            qApp->removeEventFilter(&router);
        }
    };

  private slots:
    void forwardsFilePath() {
        std::vector<std::string> got;
        SInstalled               f([&](const std::string& p) { got.push_back(p); });

        QFileOpenEvent ev(QStringLiteral("/tmp/notes/readme.md"));
        QVERIFY(QCoreApplication::sendEvent(qApp, &ev));

        QCOMPARE(got.size(), std::size_t{1});
        QCOMPARE(got.front(), std::string("/tmp/notes/readme.md"));
    }

    void forwardsLocalFileUrl() {
        std::vector<std::string> got;
        SInstalled               f([&](const std::string& p) { got.push_back(p); });

        QFileOpenEvent ev(QUrl::fromLocalFile(QStringLiteral("/tmp/with space/a.md")));
        QCoreApplication::sendEvent(qApp, &ev);

        QCOMPARE(got.size(), std::size_t{1});
        QCOMPARE(got.front(), std::string("/tmp/with space/a.md"));
    }

    void ignoresNonFileUrl() {
        std::vector<std::string> got;
        SInstalled               f([&](const std::string& p) { got.push_back(p); });

        QFileOpenEvent ev(QUrl(QStringLiteral("https://example.com/readme.md")));
        QCoreApplication::sendEvent(qApp, &ev);

        QVERIFY(got.empty());
    }

    void ignoresUnrelatedEvents() {
        std::vector<std::string> got;
        SInstalled               f([&](const std::string& p) { got.push_back(p); });

        QEvent ev(QEvent::User);
        QCoreApplication::sendEvent(qApp, &ev);

        QVERIFY(got.empty());
    }
};

QTEST_MAIN(TestFileOpenRouter)
#include "test_file_open_router.moc"
