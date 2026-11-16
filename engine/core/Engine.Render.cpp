#include "engine/core/Engine.h"

#include <cmath>
#include <string>
#include <vector>

#include "engine/core/Color.h"
#include "engine/core/Log.h"
#include "engine/platform/PlatformSDL.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/ParticleRenderer.h"
#include "engine/render/Raycaster.h"
#include "engine/render/SpriteRenderer.h"
#include "engine/ui/UiRenderer.h"

namespace {
constexpr float kFieldOfViewRadians = 1.04719755f;  // 60 degrees

void RenderCeilingAndFloor(Framebuffer& framebuffer, const Texture* ceilingTexture, const Texture* floorTexture) {
    const int halfHeight = framebuffer.Height() / 2;
    const int width = framebuffer.Width();

    for (int y = 0; y < halfHeight; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint32_t pixel = 0xFF2A2A2Au;
            if (ceilingTexture != nullptr && ceilingTexture->Width() > 0 && ceilingTexture->Height() > 0) {
                const int tx = x % ceilingTexture->Width();
                const int ty = y % ceilingTexture->Height();
                pixel = ceilingTexture->GetPixel(tx, ty);
            }
            framebuffer.PutPixel(x, y, pixel);
        }
    }

    for (int y = halfHeight; y < framebuffer.Height(); ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint32_t pixel = 0xFF3F3F3Fu;
            if (floorTexture != nullptr && floorTexture->Width() > 0 && floorTexture->Height() > 0) {
                const int tx = x % floorTexture->Width();
                const int ty = y % floorTexture->Height();
                pixel = floorTexture->GetPixel(tx, ty);
            }
            framebuffer.PutPixel(x, y, pixel);
        }
    }
}

}  // namespace

void Engine::Render() {
    if (!running_ || framebuffer_.Data() == nullptr) {
        return;
    }

    const GameState state = flow_.State();
    if (state == GameState::Menu || state == GameState::CampaignComplete) {
        // Full-screen screens: no world behind them.
        framebuffer_.ResetDrawBounds();
        RenderUi();
    } else {
        RenderWorld();
        // Only UI drawing is recorded in the draw bounds (test ui_layout_bounds).
        framebuffer_.ResetDrawBounds();
        RenderUi();
    }

    if (platform_ != nullptr) {
        platform_->PresentFramebuffer(framebuffer_);  // the frame's only present
    }
}

void Engine::RenderWorld() {
    RenderCeilingAndFloor(framebuffer_, ceilingTexture_, floorTexture_);

    RaycasterRenderParams rayParams{};
    rayParams.fieldOfViewRadians = kFieldOfViewRadians;
    rayParams.wallTexture = wallTexture_;
    rayParams.wallAltATexture = wallAltATexture_;
    rayParams.wallAltBTexture = wallAltBTexture_;
    rayParams.doorTexture = doorTexture_;
    rayParams.lockedDoorTextures = lockedDoorTextures_;
    rayParams.exitTexture = exitTexture_;
    rayParams.floorTexture = floorTexture_;
    rayParams.ceilingTexture = ceilingTexture_;
    rayParams.missingTexture = &missingTexture_;

    Raycaster::RenderWalls(map_, player_, framebuffer_, rayParams, latestRayHits_);

    latestWallDepthBuffer_.resize(latestRayHits_.size());
    for (std::size_t i = 0; i < latestRayHits_.size(); ++i) {
        latestWallDepthBuffer_[i] = latestRayHits_[i].distanceCorrected;
    }

    SpriteTextureSet spriteTextures{};
    spriteTextures.slots = spriteTextures_;
    spriteTextures.missing = &missingTexture_;

    SpriteRenderer::Render(map_, player_, framebuffer_, latestWallDepthBuffer_, sprites_, spriteTextures,
                           kFieldOfViewRadians);

    ParticleRenderer::Render(map_, player_, framebuffer_, latestWallDepthBuffer_, particleManager_.Particles(),
                             kFieldOfViewRadians);
}

void Engine::RenderUi() {
    const GameState state = flow_.State();
    const CampaignLevel* level = CurrentCampaignLevel();
    const int levelNumber = level != nullptr ? currentLevelIndex_ + 1 : 0;
    const std::string levelName = level != nullptr ? level->name : std::string();

    if (state == GameState::Menu) {
        if (flow_.Page() == MenuPage::Main) {
            UiRenderer::MenuView view{};
            view.campaignTitle = campaign_.title;
            for (const MenuItem item : flow_.MenuItems()) {
                view.items.emplace_back(MenuItemLabel(item));
            }
            view.selected = flow_.MenuCursor();
            UiRenderer::DrawMenuScreen(framebuffer_, view);
        } else {
            UiRenderer::LevelSelectView view{};
            view.campaignTitle = campaign_.title;
            for (std::size_t i = 0; i < campaign_.levels.size(); ++i) {
                view.levels.push_back(UiRenderer::LevelSelectEntry{static_cast<int>(i) + 1, campaign_.levels[i].name,
                                                                   flow_.IsLevelUnlocked(static_cast<int>(i))});
            }
            view.selected = flow_.LevelCursor();
            UiRenderer::DrawLevelSelectScreen(framebuffer_, view);
        }
        return;
    }
    if (state == GameState::CampaignComplete) {
        UiRenderer::DrawCampaignCompleteScreen(framebuffer_, campaign_.title,
                                               static_cast<int>(campaign_.levels.size()));
        return;
    }

    // Play and the overlays drawn on top of it.
    int reservedLeft = 0;
    if (minimapVisible_) {
        reservedLeft = UiRenderer::DrawMinimap(framebuffer_, map_, player_, sprites_);
    }
    if (state == GameState::Gameplay) {
        UiRenderer::DrawCrosshair(framebuffer_);
    }
    UiRenderer::HudView hud{};
    hud.health = playerHealth_;
    hud.ammo = playerAmmo_;
    hud.levelNumber = levelNumber;
    hud.levelName = levelName;
    hud.showLevelBanner = state == GameState::Gameplay && levelBannerTimer_ > 0.0f;
    hud.message = state == GameState::Gameplay ? hudMessage_ : std::string();
    hud.reservedLeft = reservedLeft;
    hud.keys = keys_;
    UiRenderer::DrawHud(framebuffer_, hud);

    if (debugOverlayVisible_) {
        const float fps = time_.deltaSeconds > 0.0 ? static_cast<float>(1.0 / time_.deltaSeconds) : 0.0f;
        UiRenderer::DrawDebugOverlay(
            framebuffer_, fps, player_.position.x, player_.position.y, player_.directionAngleRadians * 57.2957795f,
            static_cast<int>(std::floor(player_.position.x)), static_cast<int>(std::floor(player_.position.y)));
    }

    const std::string label = UiRenderer::LevelLabel(levelNumber, levelName);
    if (state == GameState::Paused) {
        UiRenderer::DrawPauseOverlay(framebuffer_, label);
    } else if (state == GameState::GameOver) {
        UiRenderer::DrawGameOverScreen(framebuffer_, label);
    } else if (state == GameState::LevelComplete || state == GameState::Victory) {
        UiRenderer::LevelCompleteView view{};
        view.levelNumber = levelNumber;
        view.levelName = levelName;
        view.results = flow_.LastResults();
        view.lastLevel = flow_.IsLastLevel();
        UiRenderer::DrawLevelCompleteScreen(framebuffer_, view);
    }
}
