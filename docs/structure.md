SDL2 gets input
   ->
Engine Core starts frame
   ->
World/Simulation updates positions and collisions
   ->
C# Gameplay applies game rules
   ->
Renderer draws the world into a software framebuffer
   ->
SDL2 presents that framebuffer
   ->
next frame