# Neo-Wolf Asset Pipeline & Technical Guide

## Asset Directory Structure

All game content assets reside under `game/assets/`:

```text
game/assets/
|-- wall_main.ppm         # Primary solid wall texture
|-- wall_alt_a.ppm        # Secondary wall variant A texture
|-- wall_alt_b.ppm        # Secondary wall variant B texture
|-- floor.ppm             # Floor plane texture
|-- ceiling.ppm           # Ceiling plane texture
|-- sprite_enemy.ppm      # Enemy billboard sprite texture
|-- sprite_pickup.ppm     # Collectible item sprite texture
|-- sprite_decor.ppm      # Ambient decoration sprite texture
|-- sfx_door_open.wav     # Door open sound effect
|-- sfx_pickup.wav        # Item pickup sound effect
|-- sfx_enemy_alert.wav   # Enemy agro / alert sound effect
`-- sfx_weapon_fire.wav   # Weapon discharge sound effect
```

---

## 1. Texture Specifications (PPM Format)

Neo-Wolf uses the plain ASCII **Netpbm PPM (`P3`)** image format for software rasterization. This provides human-inspectable text files that require no external decoding libraries (e.g. libpng, stb_image).

### Format Specification
- **Magic Identifier**: `P3` (ASCII plaintext RGB format).
- **Dimensions**: Width and Height in pixels (recommended: `64x64`, `32x32`, `16x16`, `8x8`). Power-of-two dimensions ensure optimal sampling.
- **Max Color Value**: `255` (8 bits per channel).
- **Pixel Data**: White-space separated integer triplets representing `Red Green Blue` channels from `0` to `255`.

### Sample PPM File Structure
```text
P3
# Created for Neo-Wolf Engine
4 4
255
255 0 0    255 0 0    0 255 0    0 255 0
255 0 0    255 0 0    0 255 0    0 255 0
0 0 255    0 0 255    255 255 0  255 255 0
0 0 255    0 0 255    255 255 0  255 255 0
```

### Color Key Transparency for Sprites
For billboard sprites (`sprite_enemy.ppm`, `sprite_pickup.ppm`, `sprite_decor.ppm`), the pure magenta color `RGB(255, 0, 255)` (`#FF00FF`) is designated as the **alpha mask / transparent color key**.
- Pixels matching `R=255, G=0, B=255` are discarded by `SpriteRenderer` during depth-testing and framebuffer blitting.

---

## 2. Audio Specifications (WAV Format)

Neo-Wolf's `AudioSystem` loads and mixes uncompressed standard **RIFF WAVE** (`.wav`) audio files.

### Recommended Audio Profile
- **Container**: RIFF WAVE (`.wav`)
- **Format Tag**: `1` (Uncompressed Linear PCM)
- **Sample Rate**: `44,100 Hz` (or `22,050 Hz`, `11,025 Hz`)
- **Bit Depth**: `16-bit` signed integer (or `8-bit` unsigned)
- **Channels**: `1` (Mono) for directional sound effects, `2` (Stereo) for ambient music

---

## 3. Asset Tooling with DevKit CLI

The DevKit CLI provides procedural generation and inspection utilities for assets:

### Inspecting Texture Metadata
```bash
./tools/devkit/neo_devkit inspect-asset game/assets/sprite_enemy.ppm
```
Output:
```text
--- PPM Texture Inspector: sprite_enemy.ppm ---
  format                   : P3
  width                    : 16
  height                   : 16
  maxval                   : 255
  is_power_of_two          : True
  average_color_rgb        : (241.7, 49.3, 177.8)
  color_key_magenta_pixels : 129 (50.4% transparent)
```

### Inspecting Audio Files
```bash
./tools/devkit/neo_devkit inspect-asset game/assets/sfx_door_open.wav
```

### Generating Procedural Assets
```bash
# Generate a 64x64 checkerboard texture
./tools/devkit/neo_devkit create-asset --type checker --out game/assets/wall_test.ppm --width 64 --height 64

# Generate a 64x64 brick texture
./tools/devkit/neo_devkit create-asset --type brick --out game/assets/wall_brick.ppm --width 64 --height 64

# Generate a 440 Hz test tone WAV audio file
./tools/devkit/neo_devkit create-asset --type beep --out game/assets/sfx_test.wav
```
