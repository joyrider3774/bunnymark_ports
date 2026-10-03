# BunnyMark for TIC-80

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria, as a [TIC-80](https://tic80.com) cart.
Bunnies bounce around a box; you keep adding them and watch the frame rate drop.

The release files are `TIC80_BunnyMark.tic`, the cart for TIC-80 itself (`load` it, or drop it
on TIC-80), and `Vircon32_BunnyMark_TIC80.v32`, the same cart made into a Vircon32 cartridge by
[v32lua](https://github.com/wedge1020/v32lua). The source is `bunnymark.lua`, the cart as a TIC-80
`.lua` project: TIC-80 opens it as it is.

## Buttons

| Button (keyboard) | What it does |
|---|---|
| A (Z) | +1 bunny |
| B (X) | +100 bunnies |
| X (A) | +500 bunnies |
| Y (S) | +1000 bunnies |
| A + B | back to 0 bunnies |

Bunnies are added once per press, when the button is let go. Letting go of A and B after
A + B adds nothing. In the Vircon32 version they are the Vircon32 gamepad's A, B, X and Y.

## The screen

The whole 240x136 screen: **FPS** (counted over the last second) and **bunnies** (at most
20,000) on top, the box below. The box and the text shake while a button that adds bunnies is
held, as on the Playdate.

## What it measures

Each frame (each `TIC()` call) moves every bunny (Lua numbers), clears the screen, draws every
bunny with `spr` (tiles 0, 1, 16 and 17 as one 16x16 sprite, colour 2 left out), then the box
and the text. TIC-80 calls `TIC()` at most 60 times a second, so 60 fps is the most it shows.

## Differences from the Playdate version

- The screen is 240x136; the bunny is 16x16, the size every port uses so their frame rates
  can be compared, and moves at half the Playdate's speed. The bunnies bounce at the box's edges.
- The palette is TIC-80's default (Sweetie 16) with colour 0 pure black and 12 pure white.

## The Vircon32 version

`v32lua` compiles the `.tic` (TIC-80's API on Vircon32's 640x360 screen, scaled), then the
Vircon32 DevTools assemble and pack it. It draws every sprite through the Vircon32 GPU and runs
the Lua as native code, so its frame rate is the Vircon32's, not TIC-80's.

The colours in the cart's calls are literal numbers: `v32lua` turns a palette index into a
colour only when it is a literal, and `cls(c)` with `c` a variable cleared the screen to that
number as a raw colour (TIC-80 itself takes either).

## Building

`tools/build_releases.py --only TIC80` in the folder above writes `TIC80_BunnyMark.tic` from
`bunnymark.lua` (the code, the tiles and the palette as cart chunks); `--only Vircon32` also
builds `Vircon32_BunnyMark_TIC80.v32` from that `.tic`, with `v32lua` (`--v32lua`) and the
Vircon32 DevTools (`--vircon32`).

The tiles and the palette are written into `bunnymark.lua` by `tools/make_sprites.py`.
