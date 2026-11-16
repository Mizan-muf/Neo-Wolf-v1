#include "engine/core/App.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

#include "engine/core/Engine.h"
#include "engine/core/Log.h"
#include "engine/core/Paths.h"
#include "engine/platform/PlatformSDL.h"

namespace {
using Clock = std::chrono::steady_clock;

constexpr double kMaxFrameSeconds = 0.1;  // dt clamp: a stall never becomes a huge simulation step
constexpr double kStatsIntervalSeconds = 10.0;
constexpr int kDefaultRefreshRate = 60;
// Reported refresh rates outside this range are treated as unknown.
constexpr int kMinPlausibleRefreshRate = 30;
constexpr int kMaxPlausibleRefreshRate = 500;
constexpr double kMillisecondsPerSecond = 1000.0;

// Starts <pref dir>/neowolf.log for this run (the previous run's log is kept as
// neowolf.prev.log). Without a per-user folder the game logs to the console only.
void StartLogFile(const PrefDirResult& prefDir) {
    if (prefDir.directory.empty()) {
        LogWarning("No per-user folder for the log file (" + prefDir.error + "); logging to the console only.");
        return;
    }
    const std::string logPath = PathToUtf8(prefDir.directory / "neowolf.log");
    if (!StartFreshLogFile(logPath, PathToUtf8(prefDir.directory / "neowolf.prev.log"))) {
        LogWarning("Could not create the log file " + logPath + "; logging to the console only.");
    }
}

std::string FormatDouble(double value, int decimals) {
    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.*f", decimals, value);
    return buffer.data();
}

// Today's date (local time) for the first log line; log lines only carry the time of day.
std::string CurrentDate() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::array<char, 32> buffer{};
    std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d", &local);
    return buffer.data();
}

// The first log lines: version and date, SDL version and platform, where the log and the
// per-user folder are.
void LogStartupInfo(const PrefDirResult& prefDir) {
    LogInfo(VersionString() + " starting on " + CurrentDate() + ".");
    SDL_version linked{};
    SDL_GetVersion(&linked);
    LogInfo("SDL " + std::to_string(linked.major) + "." + std::to_string(linked.minor) + "." +
            std::to_string(linked.patch) + ", platform " + SDL_GetPlatform());
    const std::string logPath = GetLogFilePath();
    LogInfo("Log file: " + (logPath.empty() ? std::string("(none)") : logPath));
    if (!prefDir.directory.empty()) {
        LogInfo("Per-user folder: " + PathToUtf8(prefDir.directory) + " (" + prefDir.source + ")");
    }
}

EngineOptions MakeEngineOptions(const CommandLineOptions& options, const PrefDirResult& prefDir) {
    EngineOptions engineOptions{};
    engineOptions.dataDir = options.dataDir;
    engineOptions.startLevel = options.level;
    engineOptions.testLevelBoot = true;  // campaign.json "bootIntoTestLevel" applies to the game itself
    engineOptions.prefDir = prefDir.directory;
    engineOptions.preview = options.preview;
    engineOptions.renderSize = options.renderSize;
    engineOptions.fullscreen = options.windowMode == WindowModeOption::Fullscreen;
    engineOptions.errorDialogs = options.frames == 0;  // never block an automated run on a dialog
    return engineOptions;
}

// Frame-rate cap for when the renderer has no vsync: the display's refresh rate if it is
// plausible, else 60 Hz.
int CapRefreshRate(const PlatformSDL& platform) {
    const int rate = platform.DisplayRefreshRate();
    return (rate >= kMinPlausibleRefreshRate && rate <= kMaxPlausibleRefreshRate) ? rate : kDefaultRefreshRate;
}

// Frame rate and worst frame time, logged every kStatsIntervalSeconds.
class FrameStats {
public:
    explicit FrameStats(Clock::time_point start) : start_(start) {}

    void AddFrame(double frameSeconds) {
        ++frames_;
        worstFrameSeconds_ = std::max(worstFrameSeconds_, frameSeconds);
    }

    void LogIfDue(Clock::time_point now) {
        const double window = std::chrono::duration<double>(now - start_).count();
        if (window < kStatsIntervalSeconds) {
            return;
        }
        LogInfo("FPS " + FormatDouble(frames_ / window, 1) + " (worst frame " +
                FormatDouble(worstFrameSeconds_ * kMillisecondsPerSecond, 1) + " ms)");
        frames_ = 0;
        worstFrameSeconds_ = 0.0;
        start_ = now;
    }

private:
    Clock::time_point start_;
    int frames_ = 0;
    double worstFrameSeconds_ = 0.0;
};

// Frame pacing for when the present does not wait for vsync (software renderer, vsync
// disabled by the driver or SDL_RENDER_VSYNC=0) or the window is minimised: sleeps until the
// next frame slot.
class FramePacer {
public:
    FramePacer(Clock::time_point start, int rate)
        : period_(std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / rate))),
          nextFrameDeadline_(start + period_) {}

    void WaitForNextFrame() {
        const auto afterFrame = Clock::now();
        if (afterFrame >= nextFrameDeadline_) {
            nextFrameDeadline_ = afterFrame + period_;  // running late: do not try to catch up
            return;
        }
        const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(nextFrameDeadline_ - afterFrame);
        if (wait.count() > 0) {
            SDL_Delay(static_cast<Uint32>(wait.count()));
        }
        nextFrameDeadline_ += period_;
    }

private:
    Clock::duration period_;
    Clock::time_point nextFrameDeadline_;
};

// Updates and renders until the engine stops or `frameLimit` frames ran (0 = no limit).
// Returns the number of frames.
std::uint64_t RunMainLoop(Engine& engine, const PlatformSDL& platform, Clock::time_point start, int capRate,
                          int frameLimit) {
    const bool vsync = platform.HasVSync();
    FramePacer pacer(start, capRate);
    FrameStats stats(start);
    auto previous = start;
    std::uint64_t totalFrames = 0;
    while (engine.IsRunning()) {
        const auto now = Clock::now();
        const double rawDelta = std::chrono::duration<double>(now - previous).count();
        previous = now;
        engine.Update(std::clamp(rawDelta, 0.0, kMaxFrameSeconds));
        engine.Render();

        ++totalFrames;
        stats.AddFrame(rawDelta);
        if (frameLimit > 0 && totalFrames >= static_cast<std::uint64_t>(frameLimit)) {
            break;
        }
        stats.LogIfDue(now);
        if (!vsync || platform.IsMinimized()) {
            pacer.WaitForNextFrame();
        }
    }
    return totalFrames;
}

void SaveScreenshot(const Engine& engine, const std::string& path) {
    std::string error;
    if (PlatformSDL::SaveFramebufferBmp(engine.framebuffer_, path, error)) {
        LogInfo("Screenshot saved: " + path);
    } else {
        LogWarning("Screenshot not saved (" + path + "): " + error);
    }
}
}  // namespace

int App::Run(const CommandLineOptions& options) {
    // The per-user folder holds the log and progress.json. --pref-dir / NEOWOLF_PREF_DIR keep
    // automated runs away from the player's real files (known issue B15).
    const PrefDirResult prefDir = ResolvePrefDir(options.prefDir);
    StartLogFile(prefDir);
    LogStartupInfo(prefDir);

    Engine engine;
    if (!engine.Init(MakeEngineOptions(options, prefDir))) {
        LogError("Startup failed; exiting with code 1.");
        engine.Shutdown();
        return 1;
    }

    const PlatformSDL& platform = *engine.platform_;
    const int capRate = CapRefreshRate(platform);
    if (!platform.HasVSync()) {
        LogInfo("No vsync: capping the frame rate at " + std::to_string(capRate) + " FPS.");
    }
    if (options.frames > 0) {
        LogInfo("Running " + std::to_string(options.frames) + " frame(s), then exiting (--frames).");
    }

    const auto runStart = Clock::now();
    LogInfo("Main loop started.");
    const std::uint64_t totalFrames = RunMainLoop(engine, platform, runStart, capRate, options.frames);

    if (!options.screenshotPath.empty()) {
        SaveScreenshot(engine, options.screenshotPath);
    }

    const double runSeconds = std::chrono::duration<double>(Clock::now() - runStart).count();
    LogInfo("Ran " + std::to_string(totalFrames) + " frame(s) in " + FormatDouble(runSeconds, 2) + " s (average " +
            FormatDouble(runSeconds > 0.0 ? static_cast<double>(totalFrames) / runSeconds : 0.0, 1) + " FPS).");

    engine.Shutdown();
    LogInfo("Exit code 0.");
    return 0;
}
