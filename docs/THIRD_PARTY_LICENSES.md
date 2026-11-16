# Third-party licences

(In a release package, links to repository files other than `../LICENSE.md` point at the source
repository.)

Neo-Wolf itself is licensed under the PolyForm Noncommercial License 1.0.0 ([LICENSE.md](../LICENSE.md));
SDL2 (zlib) and nlohmann/json (MIT) are permissive licences compatible with distributing the game
under it, provided their notices below ship with it (every release package does, in `LICENSES/`).
The game's images and sounds are all made by the repository's own generators and need no
third-party notice ([ASSET_CREDITS.md](ASSET_CREDITS.md)).

Neo-Wolf uses two third-party libraries. Both licences allow use in closed and open source,
commercial or not; they differ in what a distribution must contain. SDL's source also bundles
components under their own licences; the ones compiled into the game are audited
[below](#components-bundled-in-sdl2).

| Library | Version | How it is used | Licence | What a binary distribution must do |
|---|---|---|---|---|
| [SDL2](https://www.libsdl.org/) | 2.32.10 | Downloaded at configure time (pinned source release, SHA-256 checked in `CMakeLists.txt`) and **linked statically** into `neo_wolf`. `NEOWOLF_SYSTEM_SDL2=ON` uses an installed SDL2 instead. | zlib | Nothing is required. Acknowledging SDL in the documentation is appreciated. Altered SDL sources must be marked as altered (Neo-Wolf does not alter them). The release packages ship SDL's `LICENSE.txt` as `LICENSES/SDL2-LICENSE.txt` and name SDL in `README.txt`. |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | Header-only, vendored in `third_party/nlohmann/json.hpp` (see [third_party/README.md](../third_party/README.md)); compiled into the engine. | MIT | Include the copyright notice and the licence text below in "all copies or substantial portions of the Software". The release packages ship `LICENSES/nlohmann-json-LICENSE.MIT.txt` and this file (`LICENSES/THIRD_PARTY_LICENSES.md`), which also names the other copyright holders of code inside `json.hpp` (all MIT, [below](#nlohmannjson-mit-licence)). |

## What a release package must carry

| Notice | Why | In the package |
|---|---|---|
| PolyForm Noncommercial 1.0.0 with the "Required Notice" line | the project's own licence (its Notices clause) | `LICENSE.md` |
| nlohmann/json MIT notice, with the copyright lines of the code inside `json.hpp` | MIT: required in all copies | `LICENSES/nlohmann-json-LICENSE.MIT.txt`, and the full list in this file |
| HIDAPI BSD-3-Clause notice (bundled in SDL, linked) | BSD-3-Clause: binary distributions must reproduce it | `LICENSES/SDL2-hidapi-LICENSE-bsd.txt` |
| yuv2rgb BSD-3-Clause notice (bundled in SDL, linked) | BSD-3-Clause: binary distributions must reproduce it | `LICENSES/SDL2-yuv2rgb-LICENSE.txt` |
| SDL2 zlib licence | not required in a binary (zlib asks only that source notices stay); shipped as an acknowledgement | `LICENSES/SDL2-LICENSE.txt` |
| this file | collects the above and the audit | `LICENSES/THIRD_PARTY_LICENSES.md` |

`cmake/Packaging.cmake` installs them; DevKit `package` (`tools/devkit/packager.py`) and
`tools/ci/package_smoke.py` fail a package that lacks any of them.

## Components bundled in SDL2

Audit of SDL2 2.32.10 as Neo-Wolf builds it (static library, Neo-Wolf's options in
`CMakeLists.txt`), done for Phase 10 on macOS on 2026-11-16. Method: the SDL sources compiled
(`build/macos/compile_commands.json`, 185 SDL files; the `package-macos` build compiles the
same list), the headers they include (`ninja -t deps`), every licence or copyright line in
those files that is not SDL's own, and then whether the component's code is actually in the
linked executable (`nm -arch arm64` and `-arch x86_64` on `build/package-macos/neo_wolf`). A
file can be compiled yet contribute nothing (its code is behind an `#if`, or the linker drops
an unreferenced object from the static library), so the last step decides.

| Component (path in SDL's source) | Licence | Compiled | In the macOS executable | Binary notice needed? |
|---|---|---|---|---|
| SDL itself | zlib | yes | yes | No (shipped anyway, as an acknowledgement). |
| HIDAPI, `src/hidapi/` (`SDL_hidapi.c` includes `mac/hid.c`; Windows: `windows/hid.c`): game controller access | the user's choice of GPL-3.0, BSD-3-Clause or the original HIDAPI licence (`src/hidapi/LICENSE.txt`) | yes | yes (`PLATFORM_hid_*`, `HIDAPI_*` symbols) | Neo-Wolf uses it under the **BSD-style licence**, which requires the notice: shipped as `LICENSES/SDL2-hidapi-LICENSE-bsd.txt`. (The original HIDAPI licence only asks that the notice stays in the source files; BSD was chosen because its terms are standard and unambiguous.) |
| yuv2rgb, `src/video/yuv2rgb/` (Adrien Descamps): YUV texture conversion | BSD-3-Clause (`src/video/yuv2rgb/LICENSE`) | yes | yes (`yuv420_*_std`, `yuvnv12_*`; on x86_64 also `*_sseu`) | **Yes**: shipped as `LICENSES/SDL2-yuv2rgb-LICENSE.txt`. |
| `src/libm/` (Sun Microsystems fdlibm) | "freely granted, provided that this notice is preserved" | yes | **no** on macOS (no `SDL_uclibc_*` symbol: SDL uses the system's maths library) | Not shipped. Windows is expected to be the same (with `SDL_LIBC=ON`, which `CMakeLists.txt` sets for MSVC, SDL's CMake marks every maths function as available from the C runtime), but no Windows executable has been inspected. |
| `src/stdlib/SDL_malloc.c` (dlmalloc, Doug Lea) | public domain (CC0) | yes | no (`HAVE_MALLOC`: SDL uses the system `malloc`) | No. |
| `src/stdlib/SDL_qsort.c` (Gareth McCaughan) | zlib | yes | no (`HAVE_QSORT`: a wrapper round the system `qsort`) | No. |
| `src/stdlib/SDL_crc16.c`, `SDL_crc32.c` (public domain); `SDL_strtokr.c` (CC0) | public domain / CC0 | yes | yes (`SDL_strtokr` is a wrapper round the system `strtok_r` on macOS, `HAVE_STRTOK_R`) | No. |
| `src/render/software/SDL_rotate.c` (from SDL_gfx, Andreas Schiffler) | zlib | yes | yes | No. |
| `src/joystick/controller_type.c`, `controller_list.h`, `hidapi/steam/*.h` (Valve); `hidapi/SDL_hidapi_steamdeck.c` (Max Maisel) | zlib | yes | yes | No. |
| `src/events/imKStoUCS.c` (X11 keysym table) | X11/MIT-style | yes | no (X11 only; no `KeySymToUcs4` symbol) | No. |
| Khronos headers: `src/video/khronos/` (Vulkan, EGL, GLES2), `SDL_opengl_glext.h`, `SDL_egl.h`; Mesa's `SDL_opengl.h` | Apache-2.0 (Vulkan, EGL), MIT or MIT-style (GLES2, glext, khrplatform, Mesa) | included as headers | declarations only: types, constants and function prototypes, no code of theirs | **Not certain.** We believe interface headers that contribute only declarations create no notice obligation for the executable, so none is shipped, but this is a judgement, not a verified legal conclusion; if in doubt, ship their notices too. |

Not audited: the **Windows** executable (the MSVC build compiles SDL's Windows back ends,
`src/core/windows`, `src/video/windows`, `src/render/direct3d*` and HIDAPI's `windows/hid.c`;
by source inspection these are SDL's zlib code plus the same HIDAPI and yuv2rgb components, so
the same notices apply, but no Windows binary was inspected) and the **Linux** package (best
effort, known issue B12). Using a system SDL2 (`NEOWOLF_SYSTEM_SDL2=ON`) links SDL as built by
someone else; its notices then come with that SDL: configure warns that `LICENSES/SDL2-LICENSE.txt` cannot be
installed, the HIDAPI and yuv2rgb notices are not installed either, and DevKit `package` /
`package_smoke.py` reject such a package (they require all five files in `LICENSES/`).

The C++ standard library and the C runtime are the platform's (Apple's libc++ and system
libraries, linked dynamically; on Windows the Microsoft C runtime, linked statically). We are
not aware of a notice requirement for them in a binary, but have not checked the Visual Studio
licence terms in detail.

This audit describes what the build did on 2026-11-16 and is not legal advice. Repeat it when
SDL is updated or Neo-Wolf's SDL options change.

---

## SDL2: zlib licence

Text from `LICENSE.txt` in the SDL2 2.32.10 source release:

```text
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## nlohmann/json: MIT licence

Text from `third_party/nlohmann/LICENSE.MIT` (release 3.12.0). The header `json.hpp` also
contains code by these copyright holders, all under the same MIT licence (from its
`SPDX-FileCopyrightText` lines): Björn Hoehrmann (2008–2009, the UTF-8 decoder), Florian
Loitsch (2009, the Grisu2 number formatting), Evan Nemerson (2016–2021, Hedley macros) and The
Abseil Authors (2018, integer sequence utilities).

```text
MIT License

Copyright (c) 2013-2025 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## HIDAPI (bundled in SDL2): BSD-style licence

Neo-Wolf uses HIDAPI under the BSD-style licence, one of the three it offers
(`src/hidapi/LICENSE.txt` in SDL's source). Text from `src/hidapi/LICENSE-bsd.txt` in the
SDL2 2.32.10 source release:

```text
Copyright (c) 2010, Alan Ott, Signal 11 Software
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Signal 11 Software nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

## yuv2rgb (bundled in SDL2): BSD-3-Clause licence

Text from `src/video/yuv2rgb/LICENSE` in the SDL2 2.32.10 source release:

```text
Copyright (c) 2016, Adrien Descamps
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of yuv2rgb nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
