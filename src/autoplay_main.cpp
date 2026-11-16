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
#include <utility>
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
// shellapi.h needs the types windows.h declares, so it must come after it (and in its own
// include block, so clang-format does not sort it first).
#include <shellapi.h>
#endif

namespace {
#if defined(_WIN32)
// UTF-8 arguments from the UTF-16 command line, so non-ASCII --data paths survive.
std::vector<std::string> CommandLineArgs(int argc, char** argv) {
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
std::vector<std::string> CommandLineArgs(int argc, char** argv) {
    return {argv + (argc > 0 ? 1 : 0), argv + argc};
}
#endif

constexpr const char* kProgramName = "neo_wolf_autoplay";
constexpr int kTemporaryFolderAttempts = 100;  // name clashes tolerated before giving up

// A fresh, empty folder under the system's temporary directory.
std::filesystem::path MakeTemporaryPrefDir() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path base = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < kTemporaryFolderAttempts; ++attempt) {
        const std::filesystem::path candidate =
            base / ("neowolf-autoplay-" + std::to_string(nonce) + "-" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

// Deletes a folder and everything in it when it goes out of scope (an empty path: nothing).
class FolderRemover {
public:
    explicit FolderRemover(std::filesystem::path folder) : folder_(std::move(folder)) {}
    ~FolderRemover() {
        if (!folder_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(folder_, ec);
        }
    }
    FolderRemover(const FolderRemover&) = delete;
    FolderRemover& operator=(const FolderRemover&) = delete;
    FolderRemover(FolderRemover&&) = delete;
    FolderRemover& operator=(FolderRemover&&) = delete;

private:
    std::filesystem::path folder_;
};

// Opens `utf8Path` for writing (binary, truncated), creating its parent folders first.
bool OpenOutputFile(const std::string& utf8Path, std::ofstream& file) {
    const std::filesystem::path path = PathFromUtf8(utf8Path);
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    file.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
    return static_cast<bool>(file);
}

// Writes `text`, closes the file and reports where it went ("<label>: <path>"), or on stderr
// that `what` could not be written. Returns false on failure.
bool WriteAndClose(std::ofstream& file, const std::string& text, const std::string& utf8Path, const char* what,
                   const char* label) {
    file << text;
    file.close();
    if (!file) {
        std::cerr << kProgramName << ": cannot write " << what << " to " << utf8Path << "\n";
        return false;
    }
    std::cout << label << ": " << utf8Path << "\n";
    return true;
}
}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): an exception here is a bug; terminating keeps it visible
int main(int argc, char* argv[]) {
    const std::string program = kProgramName;
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
    const bool temporaryPrefDir = parsed.prefDir.empty();
    if (temporaryPrefDir) {
        options.prefDir = MakeTemporaryPrefDir();
        if (options.prefDir.empty()) {
            std::cerr << program << ": cannot create a temporary folder for progress.json; pass --pref-dir\n";
            return 1;
        }
    } else {
        options.prefDir = PathFromUtf8(parsed.prefDir);
        std::error_code ec;
        std::filesystem::create_directories(options.prefDir, ec);
    }
    const FolderRemover removeTemporaryPrefDir(temporaryPrefDir ? options.prefDir : std::filesystem::path());

    // --trace: open the file before playing, so a bad path fails at once.
    std::ofstream traceFile;
    std::string trace;
    if (!parsed.tracePath.empty()) {
        if (!OpenOutputFile(parsed.tracePath, traceFile)) {
            std::cerr << program << ": cannot write the trace to " << parsed.tracePath << "\n";
            return 1;
        }
        options.trace = &trace;
    }

    const AutoplayRunReport report = RunAutoplay(options);
    std::cout << FormatAutoplayTable(report) << std::flush;

    int exitCode = report.Success() ? 0 : 1;
    if (traceFile.is_open() && !WriteAndClose(traceFile, trace, parsed.tracePath, "the trace", "Trace")) {
        exitCode = 1;
    }
    if (!parsed.jsonPath.empty()) {
        std::ofstream jsonFile;
        OpenOutputFile(parsed.jsonPath, jsonFile);  // a failure shows when writing
        if (!WriteAndClose(jsonFile, FormatAutoplayJson(report), parsed.jsonPath, "the JSON report", "JSON report")) {
            exitCode = 1;
        }
    }
    return exitCode;
}
