> **Superseded: history only (archived 2026-11-16, Roadmap Phase 10).** An early sketch of the
> frame pipeline. The current, verified description is [ARCHITECTURE.md](../ARCHITECTURE.md)
> (frame loop, module map, rendering pipeline). The sketch below is **not verified** and differs
> from the code in places, for example: the frame starts with the command bridge and then the
> platform's events (not "SDL gets input" first), and the floor and ceiling are cast by the
> raycaster, not drawn as "sky". [mermaid-diagram.png](mermaid-diagram.png) is a related
> sketch from the first commit.

# Engine Architecture and Pipeline

```text
SDL2 gets input
   ->
Engine Core processes commands, updates time, and pumps events
   ->
GameState Machine controls current mode (Menu, Gameplay, Paused, GameOver, LevelComplete, CampaignComplete; Victory is deprecated) via GameFlow
   ->
Gameplay / Simulation Layer updates:
   - Player position, rotation, sliding collision against grid map
   - Hitscan combat, weapon fire, ammo consumption, and particles
   - Enemy AI state machine (Idle -> Alert -> Chase -> Attack -> Dead)
   - Interactions: pickups (health/ammo), door open/close, level triggers
   - Bidirectional Interop Command Bridge & Event Queue (C ABI)
   ->
Renderer draws the world into a 320x240 software framebuffer:
   - Sky & Floor textures
   - Raycasted walls with depth fog and lighting
   - Billboard sprites sorted back-to-front
   - Particle effects (sparks, sparkles, ambient dust)
   - State-specific UI overlays, HUD (Health, Ammo, Messages, Crosshair)
   ->
SDL2 presents that framebuffer to the window
   ->
Next frame
```
