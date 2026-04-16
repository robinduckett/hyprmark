#include "config/ConfigManager.hpp"
#include "core/IpcClient.hpp"
#include "core/IpcProtocol.hpp"
#include "core/hyprmark.hpp"
#include "defines.hpp"
#include "helpers/Log.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstddef>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

void help() {
    std::println("Usage: hyprmark [options] [FILE]\n\n"
                 "Options:\n"
                 "  -v, --verbose                 Enable verbose logging\n"
                 "  -q, --quiet                   Disable logging\n"
                 "  -c FILE, --config FILE        Use FILE as the config instead of ~/.config/hypr/hyprmark.conf\n"
                 "  --dispatch CMD [ARGS]         Send CMD (with optional ARGS) to a running hyprmark instance and exit\n"
                 "  -V, --version                 Show version information\n"
                 "  -h, --help                    Show this help message\n"
                 "\n"
                 "FILE: optional path to a Markdown document. If omitted, hyprmark shows an empty drop zone.");
}

static std::optional<std::string> parseArg(const std::vector<std::string>& args, const std::string& flag, std::size_t& i) {
    if (i + 1 < args.size())
        return args[++i];

    std::println(stderr, "Error: Missing value for {} option.", flag);
    return std::nullopt;
}

static void printVersion() {
    constexpr bool ISTAGGEDRELEASE = std::string_view(HYPRMARK_COMMIT) == HYPRMARK_VERSION_COMMIT;
    if (ISTAGGEDRELEASE)
        std::println("hyprmark version v{}", HYPRMARK_VERSION);
    else
        std::println("hyprmark version v{} (commit {})", HYPRMARK_VERSION, HYPRMARK_COMMIT);
}

int main(int argc, char** argv) {
    std::string              configPath;
    std::string              filePath;
    std::vector<std::string> dispatchArgs;
    bool                     dispatchMode = false;

    std::vector<std::string> args(argv, argv + argc);

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "--help" || arg == "-h") {
            help();
            return 0;
        }

        if (arg == "--version" || arg == "-V") {
            printVersion();
            return 0;
        }

        if (arg == "--verbose" || arg == "-v")
            Debug::verbose = true;

        else if (arg == "--quiet" || arg == "-q")
            Debug::quiet = true;

        else if ((arg == "--config" || arg == "-c")) {
            if (auto value = parseArg(args, arg, i); value)
                configPath = *value;
            else
                return 1;

        } else if (arg == "--dispatch") {
            dispatchMode = true;
            // capture all remaining args as the dispatch command + args
            while (i + 1 < args.size())
                dispatchArgs.push_back(args[++i]);
            if (dispatchArgs.empty()) {
                std::println(stderr, "Error: --dispatch requires a command.");
                return 1;
            }

        } else if (arg.starts_with("-")) {
            std::println(stderr, "Unknown option: {}", arg);
            help();
            return 1;

        } else {
            // positional: file path
            if (!filePath.empty()) {
                std::println(stderr, "Error: at most one file path may be given (got '{}' and '{}').", filePath, arg);
                return 1;
            }
            filePath = arg;
        }
    }

    printVersion();

    // Dispatch mode: send a command to the running instance and exit.
    if (dispatchMode) {
        const QString    cmd = QString::fromStdString(dispatchArgs.front());
        QStringList      args;
        for (size_t i = 1; i < dispatchArgs.size(); ++i)
            args.push_back(QString::fromStdString(dispatchArgs[i]));
        const auto res = Ipc::sendCommand(cmd, args);
        if (!res.connected) {
            std::println(stderr, "No running hyprmark instance to dispatch to (no listener at {}).",
                         Ipc::socketPath().toStdString());
            return 2;
        }
        // Print the raw reply so shell consumers can inspect it.
        std::println("{}", QString::fromUtf8(res.reply).trimmed().toStdString());
        return res.ok ? 0 : 1;
    }

    // Multi-window routing: if another hyprmark process is listening, tell
    // it to open a new window (or raise/focus if no file arg) and exit.
    // Second `hyprmark foo.md` invocations therefore get a second window.
    if (configPath.empty()) {
        QStringList forwardArgs;
        QString     forwardCmd;
        if (!filePath.empty()) {
            forwardCmd  = QStringLiteral("open");
            forwardArgs << QString::fromStdString(filePath);
        } else {
            forwardCmd = QStringLiteral("raise");
        }
        const auto res = Ipc::sendCommand(forwardCmd, forwardArgs);
        if (res.connected) {
            Debug::log(LOG, "forwarded to existing instance: {}", forwardCmd.toStdString());
            return res.ok ? 0 : 1;
        }
        // fall through: no server yet — we become one.
    }

    if (!configPath.empty())
        Debug::log(LOG, "config override: {}", configPath);
    if (!filePath.empty())
        Debug::log(LOG, "file: {}", filePath);
    else
        Debug::log(LOG, "no file given — empty state will be shown");

    try {
        g_pConfigManager = makeUnique<CConfigManager>(configPath);
        g_pConfigManager->init();
    } catch (const std::exception& ex) {
        Debug::log(CRIT, "ConfigManager threw: {}", ex.what());
        return 1;
    }

    try {
        g_pHyprmark = makeUnique<CHyprmark>();
        return g_pHyprmark->run(argc, argv, configPath, filePath);
    } catch (const std::exception& ex) {
        Debug::log(CRIT, "hyprmark threw: {}", ex.what());
        return 1;
    }
}
