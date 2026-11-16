#pragma once

// Content-root resolution and UTF-8 path helpers.
//
// Convention: every path held in a std::string inside the engine is UTF-8. Convert to
// std::filesystem::path only through PathFromUtf8() (and back through PathToUtf8()), so
// non-ASCII paths also work on Windows, where a narrow std::string would otherwise be read
// in the ANSI code page. SDL's own APIs (SDL_GetBasePath, SDL_GetPrefPath, SDL_RWFromFile,
// SDL_LoadWAV) already take and return UTF-8.

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

std::filesystem::path PathFromUtf8(std::string_view utf8);
std::string PathToUtf8(const std::filesystem::path& path);

// One directory that may be the content root, and where the guess came from
// ("--data", "NEOWOLF_DATA_DIR", "executable directory", ...).
struct ContentRootCandidate {
    std::filesystem::path directory;
    std::string source;
};

// Inputs to the search. Empty strings mean "not available". All UTF-8.
struct ContentRootInputs {
    std::string dataOption;  // --data <dir>
    std::string dataEnv;     // NEOWOLF_DATA_DIR
    std::string basePath;    // SDL_GetBasePath(): the executable's directory (Contents/Resources in a .app)
    std::string workingDir;  // the current working directory
};

struct ContentRootResult {
    bool found = false;
    std::filesystem::path root;                   // valid when found
    std::string source;                           // where root came from, when found
    std::vector<std::filesystem::path> searched;  // every directory checked, in order
};

// The sentinel: a content root contains both game/maps and game/defs.
bool IsContentRoot(const std::filesystem::path& directory);

// Candidate order. An explicit choice is exclusive, so a wrong one is reported instead of
// being silently replaced by another copy of the content:
//   1. --data, if given: the only candidate.
//   2. otherwise NEOWOLF_DATA_DIR, if set: the only candidate.
//   3. otherwise: the base path and up to 3 of its parents, <base>/../Resources (macOS
//      .app), then the working directory and its parent.
// Relative explicit directories are taken relative to workingDir. Duplicates are removed.
std::vector<ContentRootCandidate> BuildContentRootCandidates(const ContentRootInputs& inputs);

// Returns the first candidate that IsContentRoot(); records every directory checked.
ContentRootResult FindContentRoot(const std::vector<ContentRootCandidate>& candidates);

// Gathers the real inputs: the given --data value, the environment variable,
// SDL_GetBasePath() and std::filesystem::current_path().
ContentRootInputs GatherContentRootInputs(const std::string& dataOption);

// root / relative (relative is UTF-8 and may use '/'). An empty root or an absolute
// `relative` returns `relative` unchanged. The result is UTF-8.
std::string JoinContentPath(const std::filesystem::path& root, std::string_view relative);

// Multi-line, human-readable "searched: ..." text for error messages and the log.
std::string DescribeContentRootSearch(const ContentRootResult& result);
