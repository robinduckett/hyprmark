#include "render/FileWatcher.hpp"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace {
    QCoreApplication& qtApp() {
        static int               argc = 0;
        static QCoreApplication* app  = [] {
            static char* argv[] = {(char*)"tests", nullptr};
            return new QCoreApplication(argc, argv);
        }();
        return *app;
    }

    void appendByte(const std::filesystem::path& p) {
        std::ofstream f(p, std::ios::app);
        f << 'x';
    }

    std::string makeTempFile() {
        QTemporaryFile tmp;
        tmp.setAutoRemove(false);
        if (!tmp.open())
            return {};
        const auto path = tmp.fileName().toStdString();
        tmp.close();
        return path;
    }
} // namespace

TEST(FileWatcher, DebouncesRapidWritesToSingleEmission) {
    qtApp();
    const auto path = makeTempFile();
    ASSERT_FALSE(path.empty());

    CFileWatcher w;
    w.setDebounceMs(30);
    QSignalSpy spy(&w, &CFileWatcher::changed);
    w.watch(QString::fromStdString(path));

    for (int i = 0; i < 5; ++i) {
        appendByte(path);
        QTest::qWait(2);
    }
    QTest::qWait(150);

    EXPECT_EQ(spy.count(), 1);
    if (spy.count() > 0) {
        EXPECT_EQ(spy.at(0).at(0).toString().toStdString(), path);
    }
    std::filesystem::remove(path);
}

TEST(FileWatcher, MuteSuppressesEmissionDuringWindow) {
    qtApp();
    const auto path = makeTempFile();
    ASSERT_FALSE(path.empty());

    CFileWatcher w;
    w.setDebounceMs(10);
    QSignalSpy spy(&w, &CFileWatcher::changed);
    w.watch(QString::fromStdString(path));

    w.muteFor(100);
    appendByte(path);
    QTest::qWait(40);
    EXPECT_EQ(spy.count(), 0);

    QTest::qWait(120);
    appendByte(path);
    QTest::qWait(60);
    EXPECT_GE(spy.count(), 1);
    std::filesystem::remove(path);
}

TEST(FileWatcher, MuteAutoReleasesAfterMs) {
    qtApp();
    const auto path = makeTempFile();
    ASSERT_FALSE(path.empty());

    CFileWatcher w;
    w.setDebounceMs(10);
    QSignalSpy spy(&w, &CFileWatcher::changed);
    w.watch(QString::fromStdString(path));

    w.muteFor(50);
    QTest::qWait(80);
    appendByte(path);
    QTest::qWait(60);
    EXPECT_GE(spy.count(), 1);
    std::filesystem::remove(path);
}

TEST(FileWatcher, EmptyPathStopsWatching) {
    qtApp();
    const auto path = makeTempFile();
    ASSERT_FALSE(path.empty());

    CFileWatcher w;
    w.setDebounceMs(10);
    QSignalSpy spy(&w, &CFileWatcher::changed);
    w.watch(QString::fromStdString(path));
    w.watch("");
    appendByte(path);
    QTest::qWait(60);
    EXPECT_EQ(spy.count(), 0);
    std::filesystem::remove(path);
}
