#pragma once

#include <QObject>
#include <QString>

// CBridge is the C++ object exposed to the rendered page as `hyprmarkHost`
// via QWebChannel. Invokable slots are callable from JS; signals are slots
// wired by hyprmark.js to do in-place content/theme swaps + scale.
class CBridge : public QObject {
    Q_OBJECT

  public:
    explicit CBridge(QObject* parent = nullptr);
    ~CBridge() override = default;

  public slots:
    // JS -> C++ callbacks exposed on window.hyprmarkHost.
    void copyToClipboard(const QString& text);
    void openExternal(const QString& url);
    void reportHeadings(const QString& json);

  signals:
    // host -> main-window notifications
    void headingsReported(const QString& json);
    void externalLinkRequested(const QString& url);
};
