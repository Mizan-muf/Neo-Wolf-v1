> **Superseded: history only (archived 2026-11-16, Roadmap Phase 10).** The original 24-step
> build order from the first commit. Steps 19–20 (a C# interop layer and gameplay in C#) were
> never built and are out of scope for v1 ([ROADMAP.md](../ROADMAP.md#decisions)). The plan
> that is followed now is [ROADMAP.md](../ROADMAP.md); the original checklist, reconciled with
> the code, is [Checklist.md](../Checklist.md). [mermaid-diagram.png](mermaid-diagram.png) is
> the matching architecture sketch from the same commit (it shows the never-built C# module).
> Nothing below was verified against the code.

1. SDL window
2. Main loop
3. CPU framebuffer
4. Input system
5. Grid map
6. Player movement + collision
7. Raycast walls
8. Wall textures
9. Floor/ceiling
10. Sprites
11. Doors/triggers/pickups
12. Asset loading from files
13. HUD/debug UI
14. Lighting
15. Particles
16. Audio
17. Stable native engine
18. C API boundary
19. C# interop
20. Move gameplay logic to C#
21. File-driven gameplay defs
22. Prepare reload architecture
23. Build playable sample
24. Clean up and package