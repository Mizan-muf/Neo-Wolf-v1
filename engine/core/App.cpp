#include "engine/core/App.h"

#include <SDL.h>

#include <algorithm>
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
constexpr double kMaxFrameSeconds = 0.1;     // dt clamp: a stall never becomes a huge simulation step
constexpr double kStatsIntervalSeconds = 10.0;
constexpr int kDefaultRefreshRate = 60;

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
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return buffer;
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
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
    return buffer;
}

// Frame-rate cap for when the renderer has no vsync: the display's refresh rate if it is
// plausible, else 60 Hz.
int CapRefreshRate(const PlatformSDL& platform) {
    const int rate = platform.DisplayRefreshRate();
    return (rate >= 30 && rate <= 500) ? rate : kDefaultRefreshRate;
}
} // namespace

int App::Run(const CommandLineOptions& options) {
    // The per-user folder holds the log and progress.json. --pref-dir / NEOWOLF_PREF_DIR keep
    // automated runs away from the player's real files (known issue B15).
    const PrefDirResult prefDir = ResolvePrefDir(options.prefDir);
    StartLogFile(prefDir);
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

    EngineOptions engineOptions{};
    engineOptions.dataDir = options.dataDir;
    engineOptions.startLevel = options.level;
    engineOptions.prefDir = prefDir.directory;
    engineOptions.preview = options.preview;
    engineOptions.fullscreen = options.windowMode == WindowModeOption::Fullscreen;
    engineOptions.errorDialogs = options.frames == 0;  // never block an automated run on a dialog

    Engine engine;
    if (!engine.Init(engineOptions)) {
        LogError("Startup failed; exiting with code 1.");
        engine.Shutdown();
        return 1;
    }

    PlatformSDL& platform = *engine.platform_;
    const bool vsync = platform.HasVSync();
    const int capRate = CapRefreshRate(platform);
    if (!vsync) {
        LogInfo("No vsync: capping the frame rate at " + std::to_string(capRate) + " FPS.");
    }
    if (options.frames > 0) {
        LogInfo("Running " + std::to_string(options.frames) + " frame(s), then exiting (--frames).");
    }

    using clock = std::chrono::steady_clock;
    const clock::duration capPeriod =
        std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(1.0 / capRate));
    auto previous = clock::now();
    auto nextFrameDeadline = previous + capPeriod;
    auto statsStart = previous;
    std::uint64_t totalFrames = 0;
    int statsFrames = 0;
    double statsWorstFrame = 0.0;
    const auto runStart = previous;

    LogInfo("Main loop started.");

    while (engine.IsRunning()) {
        const auto now = clock::now();
        const double rawDelta = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const double dt = std::clamp(rawDelta, 0.0, kMaxFrameSeconds);

        engine.Update(dt);
        engine.Render();

        ++totalFrames;
        ++statsFrames;
        statsWorstFrame = std::max(statsWorstFrame, rawDelta);

        if (options.frames > 0 && totalFrames >= static_cast<std::uint64_t>(options.frames)) {
            break;
        }

        const double statsWindow = std::chrono::duration<double>(now - statsStart).count();
        if (statsWindow >= kStatsIntervalSeconds) {
            LogInfo("FPS " + FormatDouble(statsFrames / statsWindow, 1) + " (worst frame " +
                    FormatDouble(statsWorstFrame * 1000.0, 1) + " ms)");
            statsFrames = 0;
            statsWorstFrame = 0.0;
            statsStart = now;
        }

        // Frame pacing. With vsync the present blocks; without it (software renderer,
        // vsync disabled by the driver or SDL_RENDER_VSYNC=0) or while minimised, sleep
        // until the next frame slot.
        if (!vsync || platform.IsMinimized()) {
            const auto afterFrame = clock::now();
            if (afterFrame < nextFrameDeadline) {
                const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(nextFrameDeadline - afterFrame);
                if (wait.count() > 0) {
                    SDL_Delay(static_cast<Uint32>(wait.count()));
                }
                nextFrameDeadline += capPeriod;
            } else {
                nextFrameDeadline = afterFrame + capPeriod;  // running late: do not try to catch up
            }
        }
    }

    if (!options.screenshotPath.empty()) {
        std::string error;
        if (PlatformSDL::SaveFramebufferBmp(engine.framebuffer_, options.screenshotPath, error)) {
            LogInfo("Screenshot saved: " + options.screenshotPath);
        } else {
            LogWarning("Screenshot not saved (" + options.screenshotPath + "): " + error);
        }
    }

    const double runSeconds = std::chrono::duration<double>(clock::now() - runStart).count();
    LogInfo("Ran " + std::to_string(totalFrames) + " frame(s) in " + FormatDouble(runSeconds, 2) + " s (average " +
            FormatDouble(runSeconds > 0.0 ? static_cast<double>(totalFrames) / runSeconds : 0.0, 1) + " FPS).");

    engine.Shutdown();
    LogInfo("Exit code 0.");
    return 0;
}
