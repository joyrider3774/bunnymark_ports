# BunnyMark for SDL2 and SDL3

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria, for Windows, Linux and
macOS. Bunnies bounce around a box; you keep adding them and watch the frame rate
drop. One source file, `bunnymark.c`, builds against either SDL2 or SDL3.

It keeps the Playdate's own setup: a 400x240 screen (the window shows it at 2x and
can be resized), its box and its movement code with floats; the bunnies bounce at the
box's edges. The bunny is 16x16, the Playdate's image at half size, at half its speed: the
size every port uses so their frame rates can be compared.

## Keys

| Key | Gamepad | What it does |
|---|---|---|
| X | A (south) | +1 bunny |
| Z | B (east) | +100 bunnies |
| C | X (west) | +500 bunnies |
| V | Y (north) | +1000 bunnies |
| X + Z | A + B | back to 0 bunnies |
| M | Back | render mode: GPU / CPU |
| S | Start | vsync on / off |
| Escape | | quit |

Bunnies are added once per press, when the key is let go (the same keys the AKA
emulator uses for A, B, C and D). Letting go of X and Z after X + Z adds nothing.

## The screen

```
FPS 1128 @ GPU, vsync off
bunnies: 10000
+-------------------------------------+
|           bunnies bouncing          |
+-------------------------------------+
```

- **FPS**: frames per second, counted over the last second. It is counted again
  from zero after a mode or vsync change.
- **@ GPU / CPU**: the render mode.
- **vsync**: whether frames wait for the monitor.
- **bunnies**: how many there are, at most 1,000,000 (the Playdate stops at 50,000;
  a PC goes much further).

The window title says which SDL and which renderer SDL picked (e.g. "SDL3, direct3d11").
The box and the text shake while a key that adds bunnies is held, as on the Playdate.

## What it measures

Each frame moves every bunny (a few float additions and compares each, on the CPU),
draws them all and shows the frame. The FPS is how many of these fit in a second.
How the bunnies are drawn is the render mode:

- **GPU**: the screen is cleared and every bunny is one copy of a 16x16 texture
  through the SDL renderer, which batches them into draw calls for the graphics
  card. This is what raylib's original does, and it measures how fast SDL and the
  driver push sprites.
- **CPU**: every bunny is filled into a 400x240 frame buffer in memory as runs of
  black and white pixels (the way the AKA port draws), and that frame is uploaded
  to a texture once and drawn. This measures how fast the processor fills pixels,
  like the handhelds have to.

**Vsync** off (the default, as the Playdate version runs unthrottled) shows the raw
frame rate; on, the frame rate stops at the monitor's refresh rate.

On the PC this was written on (Windows, MSYS2 build), 10,000 bunnies (measured with the
earlier 32x32 bunny; the 16x16 one draws a quarter of the pixels, so not measured again yet):

| | GPU | CPU |
|---|---|---|
| SDL2 (direct3d) | ~1100 fps | ~300 fps |
| SDL3 (direct3d11) | ~1060 fps | ~320 fps |

With 100 bunnies and vsync off, SDL2 gave ~8500 fps and SDL3 ~4400 fps. With so
few bunnies the frame rate mostly measures the cost of a frame in the renderer
SDL picked, not the bunnies.

## Running it as a benchmark

```
bunnymark_sdl3 --bunnies 10000 --cpu --seconds 10 --shot out.bmp
```

| Option | |
|---|---|
| `--bunnies N` | start with N bunnies |
| `--cpu` | start in the CPU render mode |
| `--vsync` | start with vsync on |
| `--seconds S` | quit after S seconds, printing the SDL, the renderer and the frame rate every second |
| `--shot out.bmp` | save the screen when it quits (with `--seconds`) |

## In a browser

The same source built with Emscripten (`Web_BunnyMark_SDL2.zip` and `_SDL3.zip`:
`index.html` with its `.js` and `.wasm`; put them on any web server, or upload the zip as
an itch.io HTML game). The keys are the same; click the canvas first. In a browser the
page decides when a frame runs, so vsync means: **on**, on the display's refresh
(`requestAnimationFrame`), **off**, as soon as the browser allows (`setTimeout(0)`,
which browsers hold to a few milliseconds, so about 200 fps at most).

In Chrome on the same PC, 1,100 bunnies: SDL2 200 fps and SDL3 196 fps with vsync off
(the browser's limit, not the bunnies'), 60 with vsync on, in both drawing modes.

`web/shell.html` is the page around the canvas. Build it with
`tools/build_releases.py --only Web` in the folder above (`--emsdk` names the Emscripten
SDK), or by hand:

```
emcmake cmake -S . -B build_web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_web
```

(serve the folder over http to try it, e.g. `python -m http.server`; a page opened as a
file cannot load its .wasm).

## Building

CMake and SDL2 and/or SDL3 installed (found as CMake packages):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

gives `bunnymark_sdl2` and `bunnymark_sdl3`, each one whose SDL was found. With
MinGW and a static SDL library (MSYS2 has both) the .exe is linked statically and
needs no SDL DLL. `-DBUNNYMARK_SDL=2` or `3` builds one of them; with
`-DUSE_VENDORED_SDL=ON` as well, that SDL is downloaded and linked statically, so
nothing has to be installed.

`tools/build_releases.py --only Windows` in the folder above makes
`Windows_BunnyMark_SDL2.exe` and `Windows_BunnyMark_SDL3.exe`, with MSYS2 on Windows or,
with `--cross-windows`, with mingw-w64 on Linux and the downloaded SDLs (the CI does
this).

`bunny.h` comes from `tools/make_sprites.py` in the folder above; `font8x8_basic.h`
is the public domain 8x8 font from the Gamebuino AKA library (SDL2 has no text of
its own).
