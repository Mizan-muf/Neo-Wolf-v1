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
