# BunnyMark for Vircon32

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria, for the
[Vircon32](https://www.vircon32.com) fantasy console. Bunnies bounce around a box; you
keep adding them and watch what gives out first.

The release file is `Vircon32_BunnyMark.v32`, a cartridge for the Vircon32 emulator,
desktop or web (`Vircon32Web.html?rom=Vircon32_BunnyMark.v32`, served over http).
`Vircon32_BunnyMark_Lua.v32` and `Vircon32_BunnyMark_CPP.v32` are the same program written in
Lua and in C++ (see
[The Lua and C++ versions](#the-lua-and-c-versions)); `Vircon32_BunnyMark_TIC80.v32` and
`Vircon32_BunnyMark_PICO8.v32` are the [TIC-80](../tic80/README.md) and
[PICO-8](../pico8/README.md) carts, made into Vircon32 cartridges.

## Buttons

| Gamepad | Keyboard (emulator default) | What it does |
|---|---|---|
| A | X | +1 bunny |
| B | Z | +100 bunnies |
| X | S | +500 bunnies |
| Y | A | +1000 bunnies |
| A + B | X + Z | back to 0 bunnies |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing.

## The screen

```
FPS 60  CPU 5%  GPU 16%              bunnies: 101
+------------------------------------------------+
|                bunnies bouncing                |
+------------------------------------------------+
```

- **FPS**: frames per second, counted over the last 60 of the console's frames.
- **CPU**: the CPU time the last frame's work took, as a share of one frame's 250,000
  cycles (15 MHz at 60 fps). Past 100% the work takes more than one frame.
- **GPU**: the pixels the frame asks the GPU to draw (the clear and every bunny's 16x16
  region), as a share of its budget of 9 screens a frame. Past 100% the last bunnies of
  the frame are not drawn.
- **bunnies**: how many there are, at most 100,000.

The box and the text shake while a button that adds bunnies is held, as on the Playdate.

## What it measures

Vircon32 always runs at 60 frames a second: `end_frame()` waits for the next one. So the
frame rate only shows whole fractions, 60, 30, 20 and so on, and the two percentages show
how close each part of the machine is to its limit:

1. **Move** (CPU): every bunny moves by its speed and turns around at the box's edges.
   The Playdate's code, floats and all; Vircon32's floats are full-speed hardware.
2. **Draw** (CPU and GPU): every bunny is one `draw_region_at`, the GPU drawing the
   bunny's 16x16 texture region. The call costs CPU time; the pixels cost GPU budget.

The GPU draws at most 640 x 360 x 9 = 2,073,600 pixels a frame and silently drops what
comes after. A bunny counts as its whole 16x16 region, transparent pixels included, so
about 7,200 bunnies would fill that budget; the CPU runs out first, at about 2,700. The
box and the text are drawn before the bunnies so they are never the ones dropped.

In the Vircon32 web emulator:

| Bunnies | FPS | CPU | GPU |
|---|---|---|---|
| 0 | 60 | 1% | 11% |
| 101 | 60 | 5% | 12% |
| 2,101 | 60 | 79% | 37% |
| 4,101 | 30 | 154% | 61% |

## Differences from the Playdate version

- The screen is 640x360 instead of 400x240: the box has the Playdate's margins on the
  bigger screen, and the bunnies bounce at its edges. The bunny is 16x16, the Playdate's image at half size, at
  half its speed: the size every port uses so their frame rates can be compared.
- At most 100,000 bunnies instead of 50,000: Vircon32 has room, and the CPU and GPU
  budgets are reached long before either.
- Vircon32's C is its own dialect (every type one 32-bit word, arrays declared as
  `int[10] name`, no `?:`, no `#if`), so the code is the Playdate's logic rewritten in it.

## The Lua and C++ versions

The same program in two more languages, so the three compare what each language costs on one
console: the box, speeds, buttons, screen and texture are the C version's, and so is the GPU
work, one draw per bunny.

- **Lua**, `lua/bunnymark.lua`: `BunnyMark.c` line for line, for
  [v32lua](https://github.com/wedge1020/v32lua) and its native Vircon32 API
  (`ioports.gpu.draw()` where C calls `draw_region_at`). v32lua compiles Lua to Vircon32
  assembly, every value a NaN-boxed float, the bunnies in Lua tables. It has no garbage
  collector, so the status line is built again only when a figure in it changes (the CPU
  figure once a second, with the frame rate) instead of every frame.
- **C++**, `cpp/bunnymark.cpp`: for [v32c++](https://github.com/wedge1020/v32cxx), which turns
  C++ into Vircon32 C for the DevTools' own compiler. Each bunny is a `Bunny` object with
  `move()` and `draw()`, all of them in one `std::vector<Bunny>` (v32c++'s built-in generic),
  with room for every bunny reserved at the start. In the generated C, `bunnies[i].move()` is
  two calls per bunny: the vector's `operator[]` and the method.

In the Vircon32 web emulator, with the same button presses (the counts reached differ a little
between runs):

| Bunnies | C | C++ | Lua |
|---|---|---|---|
| 0 | 60 fps, CPU 1% | 60 fps, CPU 1% | 60 fps, CPU 0% |
| 101 | 60 fps, CPU 5% | 60 fps, CPU 7% | 60 fps, CPU 19% |
| 1,601 | | | 15 fps, CPU 301% |
| 2,101 | 60 fps, CPU 75% | 30 fps, CPU 116% | |
| 2,601 | | | 12 fps, CPU 489% |
| 3,101 | 30 fps, CPU 110% | | |
| 4,101 | | 20 fps, CPU 226% | |

Per bunny, C++ takes about one and a half times the CPU time of C, and Lua about five times.

## Optimized builds

Every Vircon32 cartridge also comes as an `_Opt` one (`Vircon32_BunnyMark_Opt.v32`,
`Vircon32_BunnyMark_CPP_Opt.v32`, `..._Lua_Opt`, `..._TIC80_Opt`, `..._PICO8_Opt`): the same
program, its assembly put through [v32opt](https://github.com/wedge1020/v32opt) at `-O3`
(peepholes, dead code elimination, global constant folding and function inlining) before it is
assembled. Side by side with the plain one it shows what the optimizer gains for each compiler.

## Building

With the Vircon32 DevTools (`compile`, `assemble`, `png2vircon`, `packrom`):

```
compile BunnyMark.c -o obj/BunnyMark.asm
assemble obj/BunnyMark.asm -o obj/BunnyMark.vbin
png2vircon assets/bunny.png -o obj/bunny.vtex
packrom BunnyMark.xml -o bin/BunnyMark.v32
```

or `tools/build_releases.py --only Vircon32` in the folder above (`--vircon32` names
the DevTools folder), which also builds the Lua version and the TIC-80 and PICO-8 ones with
v32lua (`--v32lua`), the C++ version with v32c++ (`--v32cxx`), and every `_Opt` cartridge with
v32opt (`--v32opt`). `assets/bunny.png` (the bunny and a black block for the box's lines) comes
from `tools/make_sprites.py`.
