#include "engine/core/Paths.h"

#include <SDL.h>

#include <cstdlib>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
constexpr int kBasePathParents = 3;

std::string GetEnvironmentUtf8(const char* name) {
#if defined(_WIN32)
    // The wide API, so a non-ASCII value survives (getenv returns the ANSI code page).
    const std::wstring wideName = fs::path(name).wstring();
    const DWORD size = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
    if (size == 0) {
        return {};
    }
    std::wstring value(size, L'\0');
    const DWORD written = GetEnvironmentVariableW(wideName.c_str(), value.data(), size);
    if (written == 0 || written >= size) {
        return {};
    }
    value.resize(written);
    return PathToUtf8(fs::path(value));
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
#endif
}

fs::path Normalized(const fs::path& path) {
    fs::path normal = path.lexically_normal();
    // "dir/" and "dir" are the same directory; drop the empty trailing element.
    if (!normal.empty() && normal.filename().empty() && normal.has_relative_path()) {
        normal = normal.parent_path();
    }
    return normal;
}

fs::path MakeAbsolute(const fs::path& path, const std::string& workingDir) {
    if (path.is_absolute() || workingDir.empty()) {
        return path;
    }
    return PathFromUtf8(workingDir) / path;
}

void AddCandidate(std::vector<ContentRootCandidate>& out, const fs::path& directory, const std::string& source) {
    if (directory.empty()) {
        return;
    }
    const fs::path normal = Normalized(directory);
    for (const ContentRootCandidate& existing : out) {
        if (existing.directory == normal) {
            return;
        }
    }
    out.push_back(ContentRootCandidate{normal, source});
}
} // namespace

fs::path PathFromUtf8(std::string_view utf8) {
    // u8path is deprecated in C++20 (use char8_t there); it is the portable C++17 way.
    return fs::u8path(utf8.begin(), utf8.end());
}

std::string PathToUtf8(const fs::path& path) {
    // C++17: u8string() returns std::string (std::u8string from C++20).
    return path.u8string();
}

bool IsContentRoot(const fs::path& directory) {
    std::error_code ec;
    return fs::is_directory(directory / "game" / "maps", ec) && fs::is_directory(directory / "game" / "defs", ec);
}

std::vector<ContentRootCandidate> BuildContentRootCandidates(const ContentRootInputs& inputs) {
    std::vector<ContentRootCandidate> candidates;

    if (!inputs.dataOption.empty()) {
        AddCandidate(candidates, MakeAbsolute(PathFromUtf8(inputs.dataOption), inputs.workingDir), "--data");
        return candidates;
    }
    if (!inputs.dataEnv.empty()) {
        AddCandidate(candidates, MakeAbsolute(PathFromUtf8(inputs.dataEnv), inputs.workingDir), "NEOWOLF_DATA_DIR");
        return candidates;
    }

    if (!inputs.basePath.empty()) {
        fs::path dir = Normalized(PathFromUtf8(inputs.basePath));
        AddCandidate(candidates, dir, "executable directory");
        for (int i = 1; i <= kBasePathParents; ++i) {
            const fs::path parent = dir.parent_path();
            if (parent.empty() || parent == dir) {
                break;
            }
            dir = parent;
            AddCandidate(candidates, dir, "executable directory, " + std::to_string(i) + " level(s) up");
        }
        AddCandidate(candidates, PathFromUtf8(inputs.basePath) / ".." / "Resources", "macOS app bundle Resources");
    }

    if (!inputs.workingDir.empty()) {
        const fs::path cwd = Normalized(PathFromUtf8(inputs.workingDir));
        AddCandidate(candidates, cwd, "working directory");
        const fs::path parent = cwd.parent_path();
        if (!parent.empty() && parent != cwd) {
            AddCandidate(candidates, parent, "parent of the working directory");
        }
    }
    return candidates;
}

ContentRootResult FindContentRoot(const std::vector<ContentRootCandidate>& candidates) {
    ContentRootResult result{};
    for (const ContentRootCandidate& candidate : candidates) {
        result.searched.push_back(candidate.directory);
        if (IsContentRoot(candidate.directory)) {
            result.found = true;
            result.root = candidate.directory;
            result.source = candidate.source;
            return result;
        }
    }
    return result;
}

ContentRootInputs GatherContentRootInputs(const std::string& dataOption) {
    ContentRootInputs inputs{};
    inputs.dataOption = dataOption;
    inputs.dataEnv = GetEnvironmentUtf8("NEOWOLF_DATA_DIR");

    if (char* basePath = SDL_GetBasePath()) {
        inputs.basePath = basePath;
        SDL_free(basePath);
    }

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        inputs.workingDir = PathToUtf8(cwd);
    }
    return inputs;
}

std::string JoinContentPath(const fs::path& root, std::string_view relative) {
    const fs::path rel = PathFromUtf8(relative);
    if (root.empty() || rel.is_absolute()) {
        return std::string(relative);
    }
    return PathToUtf8((root / rel).make_preferred());
}

std::string DescribeContentRootSearch(const ContentRootResult& result) {
    std::string text = "Searched for a folder containing game/maps and game/defs in:";
    if (result.searched.empty()) {
        text += "\n  (no candidate directories)";
    }
    for (const fs::path& dir : result.searched) {
        text += "\n  " + PathToUtf8(dir);
    }
    return text;
}
