#include "core/IpcProtocol.hpp"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

TEST(IpcProtocol, SocketPathIsHyprmarkSpecific) {
    // Under Hyprland: $XDG_RUNTIME_DIR/hypr/$HIS/hyprmark.sock — ends with "hyprmark.sock".
    // Outside Hyprland (tests/CI):       /tmp/hyprmark-<uid>.sock — starts with "/tmp/hyprmark-".
    // Either is acceptable; both contain "hyprmark" and a .sock suffix.
    const auto p = Ipc::socketPath();
    EXPECT_TRUE(p.contains("hyprmark"));
    EXPECT_TRUE(p.endsWith(".sock"));
}

TEST(IpcProtocol, EncodeRequestIsLineJson) {
    QStringList args;
    args << "/tmp/x.md";
    const auto bytes = Ipc::encodeRequest("open", args);
    ASSERT_TRUE(bytes.endsWith('\n'));
    const auto doc = QJsonDocument::fromJson(bytes);
    ASSERT_TRUE(doc.isObject());
    const auto o = doc.object();
    EXPECT_EQ(o.value("cmd").toString(), "open");
    ASSERT_TRUE(o.value("args").isArray());
    ASSERT_EQ(o.value("args").toArray().size(), 1);
    EXPECT_EQ(o.value("args").toArray().at(0).toString(), "/tmp/x.md");
}

TEST(IpcProtocol, ParseLineRoundTrip) {
    const auto req = Ipc::encodeRequest("theme", QStringList{"nord"});
    const auto obj = Ipc::parseLine(req);
    EXPECT_EQ(obj.value("cmd").toString(), "theme");
    EXPECT_EQ(obj.value("args").toArray().at(0).toString(), "nord");
}

TEST(IpcProtocol, ParseLineMalformedIsEmpty) {
    EXPECT_TRUE(Ipc::parseLine("{not json").isEmpty());
    EXPECT_TRUE(Ipc::parseLine("").isEmpty());
    // Arrays are not accepted as the top-level.
    EXPECT_TRUE(Ipc::parseLine("[1,2,3]").isEmpty());
}

TEST(IpcProtocol, OkReplyIsOk) {
    const auto reply = Ipc::okReply();
    const auto obj   = Ipc::parseLine(reply);
    EXPECT_TRUE(obj.value("ok").toBool());
}

TEST(IpcProtocol, OkReplyExtraMerged) {
    QJsonObject extra;
    extra.insert("themes", QJsonArray{"a", "b"});
    const auto obj = Ipc::parseLine(Ipc::okReply(extra));
    EXPECT_TRUE(obj.value("ok").toBool());
    EXPECT_EQ(obj.value("themes").toArray().size(), 2);
}

TEST(IpcProtocol, ErrorReplyCarriesMessage) {
    const auto obj = Ipc::parseLine(Ipc::errorReply("bad things"));
    EXPECT_FALSE(obj.value("ok").toBool());
    EXPECT_EQ(obj.value("error").toString(), "bad things");
}
