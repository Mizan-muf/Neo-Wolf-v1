# Third-party licences

Neo-Wolf itself is licensed under the PolyForm Noncommercial License 1.0.0 ([LICENSE.md](../LICENSE.md));
SDL2 (zlib) and nlohmann/json (MIT) are permissive licences compatible with distributing the game
under it, provided their notices below ship with it (every release package does, in `LICENSES/`).

Neo-Wolf uses two third-party libraries. Both licences allow use in closed and open source,
commercial or not; they differ in what a distribution must contain.

| Library | Version | How it is used | Licence | What a binary distribution must do |
|---|---|---|---|---|
| [SDL2](https://www.libsdl.org/) | 2.32.10 | Downloaded at configure time (pinned source release, SHA-256 checked in `CMakeLists.txt`) and **linked statically** into `neo_wolf`. `NEOWOLF_SYSTEM_SDL2=ON` uses an installed SDL2 instead. | zlib | Nothing is required. Acknowledging SDL in the documentation is appreciated. Altered SDL sources must be marked as altered (Neo-Wolf does not alter them). The release packages ship SDL's `LICENSE.txt` as `LICENSES/SDL2-LICENSE.txt` and name SDL in `README.txt`. |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | Header-only, vendored in `third_party/nlohmann/json.hpp` (see [third_party/README.md](../third_party/README.md)); compiled into the engine. | MIT | Include the copyright notice and the licence text below in "all copies or substantial portions of the Software". The release packages ship `LICENSES/nlohmann-json-LICENSE.MIT.txt` and this file (`LICENSES/THIRD_PARTY_LICENSES.md`). |

Note on SDL: the SDL source release also contains a few components that carry their own
permissive licences (for example bundled third-party headers and helper code). Which of them
end up in a static build depends on the platform and SDL's configure options. This has not
been audited yet; check SDL's source tree (`build/<preset>/_deps/sdl2-src/`) before the first
release (Roadmap Phase 10; the Phase 9 packages ship only SDL's main `LICENSE.txt`).

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

Text from `third_party/nlohmann/LICENSE.MIT` (release 3.12.0):

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
