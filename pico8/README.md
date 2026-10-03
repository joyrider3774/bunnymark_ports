# BunnyMark for PICO-8

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria, as a [PICO-8](https://www.lexaloffle.com/pico-8.php)
cart. Bunnies bounce around a box; you keep adding them and watch the frame rate drop.

The release files are `PICO8_BunnyMark.p8`, the cart for PICO-8 itself (`load` it, or drop it on
PICO-8), and `Vircon32_BunnyMark_PICO8.v32`, the same cart made into a Vircon32 cartridge by
[v32lua](https://github.com/wedge1020/v32lua). The source is `bunnymark.p8`.

## Buttons

| Button (keyboard) | What it does |
|---|---|
| O (Z, C, N) | +1 bunny |
| X (X, V, M) | +100 bunnies |
| UP | +500 bunnies |
| DOWN | +1000 bunnies |
| O + X | back to 0 bunnies |

Bunnies are added once per press, when the button is let go. Letting go of O and X after
O + X adds nothing. In the Vircon32 version O and X are the gamepad's A and B.

## The screen

The whole 128x128 screen: **fps** and **cpu** on the first line, **bunnies** (at most 10,000)
on the second, the box below, as on the CHGame. The box and the text shake while a button that
adds bunnies is held.

- **fps**: PICO-8's own frame rate, `stat(7)`.
- **cpu**: the CPU used by the last frame, `stat(1)`, in percent of a 60 fps frame. Past 100%
  PICO-8 skips draws: `_update60` still runs every 1/60 s, `_draw` less often.

The bunnies are white on black, unlike the other ports: colour 0 is the transparent one in
PICO-8's sprites by default, and the only one in `v32lua`'s (it ignores `palt`), so the bunny's
black parts are left out and the black screen shows through. That way the cart looks the same in
PICO-8 and as a Vircon32 cartridge.

## What it measures

Each `_update60` moves every bunny (PICO-8's 16.16 fixed point numbers); each `_draw` clears the
screen and draws every bunny with `spr(0, x, y, 2, 2)`, then the box and the text. PICO-8
counts the cost of every Lua instruction and every pixel drawn against a fixed budget, so the
frame rate is the same on every computer.

## Differences from the Playdate version

- The screen is 128x128; the bunny is 16x16, the size every port uses so their frame rates can
  be compared, and moves at half the Playdate's speed. The bunnies bounce at the box's edges.
- White on black (see above).

## The Vircon32 version

`v32lua` compiles the cart (PICO-8's API on Vircon32's screen, the 128x128 picture scaled
between two side panels), then the Vircon32 DevTools assemble and pack it. Its `stat()` returns
0, so there the frame rate comes from the Vircon32's frame counter (`system.frames`) and the CPU
figure stays at 0%. The cart tells the two apart by `stat(0)`, the Lua memory in use, which is
never 0 in PICO-8 (where `system` does not exist, so that path is never taken).

## Building

`tools/build_releases.py --only PICO8` in the folder above copies the cart to
`PICO8_BunnyMark.p8`; `--only Vircon32` also builds `Vircon32_BunnyMark_PICO8.v32`, with
`v32lua` (`--v32lua`) and the Vircon32 DevTools (`--vircon32`).

The sprite sheet (`__gfx__`) is written into `bunnymark.p8` by `tools/make_sprites.py`.
