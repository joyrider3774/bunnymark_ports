# BunnyMark for the CHGame, the Gamebuino AKA, the Arduboy, the ESPboy, the Thumby Color, Pimoroni and Adafruit handhelds, Vircon32, SDL2/SDL3 and the web

Ports of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria: bunnies bounce around a
box, and you keep adding them until the frame rate gives in. Each frame moves every
bunny, draws them all and puts the frame on the screen, so the frame rate shows how
fast the machine fills pixels and gets them onto its display.

Every port draws the same 16x16 bunny (the Playdate's 32x32 image at half size), moving at
half the Playdate's speed, so the frame rates of different machines can be compared: each
bunny is the same work everywhere, and only the screen each clears and sends differs.

| | CHGame | AKA | Arduboy | ESPboy | Vircon32 | SDL2 / SDL3 (PC, web) |
|---|---|---|---|---|---|---|
| Details | [chgame/BunnyMark](chgame/BunnyMark/README.md) | [aka](aka/README.md) | [arduboy](arduboy/README.md) | [espboy](espboy/README.md) | [vircon32](vircon32/README.md) | [sdl](sdl/README.md) |
| Screen | 128x128 | 320x240 | 128x64, 1 bit | 128x128 | 640x360 | 400x240, like the Playdate |
| +1 bunny (on release) | A | A | A | A | A | X / pad A |
| +10 | - | - | UP | - | - | - |
| +100 | B | B | B | B | B | Z / pad B |
| +500 | SELECT | C | - | UP | X | C / pad X |
| +1000 | - | D | - | DOWN | Y | V / pad Y |
| back to 0 | A + B | A + B | A + B | A + B | A + B | X + Z / pad A + B |
| display setting to switch | START: panel mode 16 / 12 / 18 bpp | L1 / R1: refresh rate 40 / 60 / 80 / 100 Hz | - | right side button: frame buffer 16 / 8 / 1 bpp; two builds, LovyanGFX and TFT_eSPI | - (fixed 60 fps; shows CPU and GPU budget) | M: GPU / CPU drawing, S: vsync |
| leave | hold START 3 s: the SD game menu | hold MENU + RUN 0.5 s: the launcher (RUN alone switches off) | - | - | - | Escape |
| most bunnies | 1800 (20 KB of RAM) | 50,000 (in PSRAM) | 390 (2.5 KB of RAM) | 3000 (80 KB of RAM) | 100,000 | 1,000,000 |

**PyBadge and PyGamer** ([pybadge](pybadge/README.md)), **PicoSystem**
([picosystem](picosystem/README.md)), **Explorer 2350 and Tufty 2350**
([pimoroni2350](pimoroni2350/README.md)), **Thumby Color** ([thumbycolor](thumbycolor/README.md)):
four sketches sharing the game and the frame (`common/BunnyGame.h`, `common/BunnyFrame.h`), each
with its own display driver. Every device uses its whole screen, 1:1 (160x128, 240x240, 320x240
and 128x128); +1, +100, +500, +1000 and back to 0 on each device's buttons, and one button
switches the frame between 16, 8 and 1 bpp while it runs. The Thumby Color comes twice, built for
the RP2350's Cortex-M33 cores and for its Hazard3 RISC-V cores, to compare the two on one chip.
Run on a PicoSystem, an Explorer, a Tufty and a PyGamer; the PyBadge and the Thumby Color built,
not yet run.

**TIC-80** ([tic80](tic80/README.md)) and **PICO-8** ([pico8](pico8/README.md)): Lua carts for
the two fantasy consoles themselves (`TIC80_BunnyMark.tic`, `PICO8_BunnyMark.p8`), each on its
console's whole screen (240x136, 128x128), plus the same carts made into Vircon32 cartridges by
[v32lua](https://github.com/wedge1020/v32lua) (`Vircon32_BunnyMark_TIC80.v32`,
`Vircon32_BunnyMark_PICO8.v32`). TIC-80: A +1, B +100, X +500, Y +1000; PICO-8: O +1, X +100,
UP +500, DOWN +1000; both the first two together back to 0. The Vircon32 version also comes
in Lua (`Vircon32_BunnyMark_Lua.v32`, for v32lua's native API) and in C++
(`Vircon32_BunnyMark_CPP.v32`, a `Bunny` class in a `std::vector`, through
[v32c++](https://github.com/wedge1020/v32cxx)), to compare the three languages on one console
([vircon32](vircon32/README.md#the-lua-and-c-versions)).

Each port's README has what the screen shows, what each part of a frame costs, how
its display setting changes the frame rate, how it differs from the Playdate
version and how to build it.

## Building every version

`python tools/build_releases.py` builds every port into `releases/`, named
`<device>_BunnyMark<variant>.<ext>` like the *_embedded games' releases:

| File | |
|---|---|
| `CHGame_BunnyMark.bin` | CHGame |
| `Arduboy_BunnyMark.hex`, `.bin` | Arduboy |
| `ESPboy_BunnyMark_LovyanGFX.bin`, `_TFT_eSPI.bin` | ESPboy, one per display library |
| `PyBadge_`, `PyGamer_`, `PicoSystem_`, `Explorer_`, `Tufty_BunnyMark.uf2` | the Adafruit and Pimoroni handhelds |
| `ThumbyColor_BunnyMark_ARM.uf2`, `_RISCV.uf2` | the Thumby Color, one per kind of RP2350 core |
| `Aka_BunnyMark.zip` | the AKA launcher's SD card folder |
| `Windows_BunnyMark_SDL2.exe`, `_SDL3.exe` | SDL on Windows |
| `Web_BunnyMark_SDL2.zip`, `_SDL3.zip` | SDL in a browser (Emscripten): index.html, .js, .wasm |
| `Vircon32_BunnyMark.v32` | the Vircon32 cartridge |

`--only <device>...` builds some, `--list` shows what would be built, `--help` the
folders it looks for (Arduino IDE 1.8 and 2, bateske/CHGame's libraries, ESP-IDF and the
AKA library, MSYS2, the Vircon32 DevTools, v32lua, v32c++ and v32opt, the Emscripten SDK). The GitHub workflow (`.github/workflows/build-releases.yml`, run by
hand or on a `v*` tag) installs pinned versions of all of it on a Linux runner, builds
every version the same way and uploads `releases/` as an artifact. A second job builds the
SDL port for Linux (x64, arm64) and macOS (Intel, Apple silicon), one artifact per runner.

## The bunny

`python tools/make_sprites.py` makes every `bunny.h` from `assets/wabbit_alpha.png`:
the 32x32 original shrunk to 16x16 (each 2x2 block black, white or transparent by
majority, which keeps the eyes and the outline), in the form each port draws it (and the
Vircon32 texture). The image is the Playdate version's (`Source/images/wabbit_alpha.png`),
itself raylib's bunny. The generated headers are included, so the image is only
needed to make them again.
