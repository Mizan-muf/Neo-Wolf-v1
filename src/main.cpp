// Neo-Wolf game entry point: parses the command line, then runs the App.
//
// Windows: non-Debug builds are GUI-subsystem executables (no console window). They have no
// console of their own, so for --help, --version and command-line errors main() attaches to
// the console of the parent process (cmd.exe / PowerShell) if there is one. Output that is
// redirected to a file or pipe works without that. Arguments are read as UTF-16 and
// converted to UTF-8, so a non-ASCII --data path survives.

#include <iostream>
#include <string>
#include <vector>

#include "engine/core/App.h"
#include "engine/core/CommandLine.h"

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

#include <cstdio>
#endif

namespace {
#if defined(_WIN32)
std::string WideToUtf8(const wchar_t* wide) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), size, nullptr, nullptr);
    utf8.resize(static_cast<std::size_t>(size - 1));  // drop the terminating NUL
    return utf8;
}

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
        args.push_back(WideToUtf8(wideArgs[i]));
    }
    LocalFree(wideArgs);
    return args;
}

// A GUI-subsystem process starts without usable standard handles unless they were
// redirected. Attach to the parent's console so --help/--version text is visible there.
void AttachParentConsoleForOutput() {
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out != nullptr && out != INVALID_HANDLE_VALUE && GetFileType(out) != FILE_TYPE_UNKNOWN) {
        return;  // already a console, file or pipe
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* ignored = nullptr;
        freopen_s(&ignored, "CONOUT$", "w", stdout);
        freopen_s(&ignored, "CONOUT$", "w", stderr);
        std::cout.clear();
        std::cerr.clear();
    }
}
#else
std::vector<std::string> CommandLineArgs(int argc, char** argv) {
    return {argv + (argc > 0 ? 1 : 0), argv + argc};
}

void AttachParentConsoleForOutput() {}
#endif
}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): an exception here is a bug; terminating keeps it visible
int main(int argc, char* argv[]) {
    const CommandLineResult parsed = ParseCommandLine(CommandLineArgs(argc, argv));
    const std::string program = "neo_wolf";

    switch (parsed.action) {
        case CommandLineAction::ShowHelp:
            AttachParentConsoleForOutput();
            std::cout << VersionString() << "\n\n" << CommandLineUsage(program) << std::flush;
            return 0;
        case CommandLineAction::ShowVersion:
            AttachParentConsoleForOutput();
            std::cout << VersionString() << '\n' << std::flush;
            return 0;
        case CommandLineAction::Error:
            AttachParentConsoleForOutput();
            std::cerr << program << ": " << parsed.error << "\n\n" << CommandLineUsage(program) << std::flush;
            return 2;
        case CommandLineAction::Run:
            break;
    }

    return App::Run(parsed.options);
}
