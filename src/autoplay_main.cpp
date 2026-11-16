// neo_wolf_autoplay: plays Neo-Wolf headless with the autoplay bot and reports per level
// (Roadmap Phase 7, docs/TESTING.md "Autoplay bot"). A console program on every platform.
//
//   neo_wolf_autoplay --level 1
//   neo_wolf_autoplay --all --json autoplay-report.json
//   neo_wolf_autoplay --level 3 --trace level03-trace.txt
//
// Exit codes: 0 every requested level was completed, 1 otherwise (or the game could not start,
// or the report or trace could not be written), 2 usage error.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "engine/autoplay/AutoplayBot.h"
#include "engine/autoplay/AutoplayReport.h"
#include "engine/core/CommandLine.h"
#include "engine/core/Log.h"
#include "engine/core/Paths.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
#if defined(_WIN32)
// UTF-8 arguments from the UTF-16 command line, so non-ASCII --data paths survive.
std::vector<std::string> CommandLineArgs(int argc, char* argv[]) {
    int wideCount = 0;
    LPWSTR* wideArgs = CommandLineToArgvW(GetCommandLineW(), &wideCount);
    std::vector<std::string> args;
    if (wideArgs == nullptr) {
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        return args;
    }
    for (int i = 1; i < wideCount; ++i) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, wideArgs[i], -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(size > 1 ? static_cast<std::size_t>(size) : 1u, '\0');
        if (size > 1) {
            WideCharToMultiByte(CP_UTF8, 0, wideArgs[i], -1, utf8.data(), size, nullptr, nullptr);
            utf8.resize(static_cast<std::size_t>(size - 1));
        } else {
            utf8.clear();
        }
        args.push_back(utf8);
    }
    LocalFree(wideArgs);
    return args;
}
#else
std::vector<std::string> CommandLineArgs(int argc, char* argv[]) {
    return std::vector<std::string>(argv + (argc > 0 ? 1 : 0), argv + argc);
}
#endif

// A fresh, empty folder under the system's temporary directory.
std::filesystem::path MakeTemporaryPrefDir() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path base = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < 100; ++attempt) {
        const std::filesystem::path candidate =
            base / ("neowolf-autoplay-" + std::to_string(nonce) + "-" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}
} // namespace

int main(int argc, char* argv[]) {
    const std::string program = "neo_wolf_autoplay";
    const AutoplayCommandLine parsed = ParseAutoplayCommandLine(CommandLineArgs(argc, argv));
    switch (parsed.action) {
    case AutoplayCommandLine::Action::ShowHelp:
        std::cout << VersionString() << " autoplay\n\n" << AutoplayUsage(program) << std::flush;
        return 0;
    case AutoplayCommandLine::Action::ShowVersion:
        std::cout << VersionString() << " autoplay\n" << std::flush;
        return 0;
    case AutoplayCommandLine::Action::Error:
        std::cerr << program << ": " << parsed.error << "\n\n" << AutoplayUsage(program) << std::flush;
        return 2;
    case AutoplayCommandLine::Action::Run:
        break;
    }

    // The engine logs a lot at Info level; the table is the output that matters here.
    SetConsoleLogLevel(parsed.verbose ? LogLevel::Info : LogLevel::Warning);

    AutoplayRunOptions options{};
    options.dataDir = parsed.dataDir;
    options.level = parsed.all ? 0 : parsed.level;
    options.config.timeLimitSeconds = parsed.timeLimitSeconds;
    bool temporaryPrefDir = false;
    if (parsed.prefDir.empty()) {
        options.prefDir = MakeTemporaryPrefDir();
        temporaryPrefDir = !options.prefDir.empty();
        if (options.prefDir.empty()) {
            std::cerr << program << ": cannot create a temporary folder for progress.json; pass --pref-dir\n";
            return 1;
        }
    } else {
        options.prefDir = PathFromUtf8(parsed.prefDir);
        std::error_code ec;
        std::filesystem::create_directories(options.prefDir, ec);
    }

    // --trace: open the file before playing, so a bad path fails at once.
    std::ofstream traceFile;
    std::string trace;
    if (!parsed.tracePath.empty()) {
        const std::filesystem::path tracePath = PathFromUtf8(parsed.tracePath);
        std::error_code ec;
        if (tracePath.has_parent_path()) {
            std::filesystem::create_directories(tracePath.parent_path(), ec);
        }
        traceFile.open(tracePath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!traceFile) {
            std::cerr << program << ": cannot write the trace to " << parsed.tracePath << "\n";
            if (temporaryPrefDir) {
                std::filesystem::remove_all(options.prefDir, ec);
            }
            return 1;
        }
        options.trace = &trace;
    }

    const AutoplayRunReport report = RunAutoplay(options);
    std::cout << FormatAutoplayTable(report) << std::flush;

    int exitCode = report.Success() ? 0 : 1;
    if (traceFile.is_open()) {
        traceFile << trace;
        traceFile.close();
        if (!traceFile) {
            std::cerr << program << ": cannot write the trace to " << parsed.tracePath << "\n";
            exitCode = 1;
        } else {
            std::cout << "Trace: " << parsed.tracePath << "\n";
        }
    }
    if (!parsed.jsonPath.empty()) {
        const std::filesystem::path jsonPath = PathFromUtf8(parsed.jsonPath);
        std::error_code ec;
        if (jsonPath.has_parent_path()) {
            std::filesystem::create_directories(jsonPath.parent_path(), ec);
        }
        std::ofstream file(jsonPath, std::ios::out | std::ios::trunc | std::ios::binary);
        file << FormatAutoplayJson(report);
        file.close();
        if (!file) {
            std::cerr << program << ": cannot write the JSON report to " << parsed.jsonPath << "\n";
            exitCode = 1;
        } else {
            std::cout << "JSON report: " << parsed.jsonPath << "\n";
        }
    }

    if (temporaryPrefDir) {
        std::error_code ec;
        std::filesystem::remove_all(options.prefDir, ec);
    }
    return exitCode;
}
