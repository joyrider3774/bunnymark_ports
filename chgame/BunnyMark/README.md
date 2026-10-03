# BunnyMark for the CHGame

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria. Bunnies bounce around a
box; you keep adding them and watch the frame rate drop.

## Buttons

| Button | What it does |
|---|---|
| A | +1 bunny |
| B | +100 bunnies |
| SELECT | +500 bunnies |
| A + B | back to 0 bunnies |
| START (tap) | next panel mode: 16 → 12 → 18 → 16 bpp |
| START (hold 3 s) | back to the SD game menu |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing. A button that is already held when the game starts adds
nothing when it is let go.

## The screen

```
FPS 51 @ 16 bpp
bunnies 100
+--------------------+
|  bunnies bouncing  |
+--------------------+
```

- **FPS**: frames per second, counted over the last second. It is counted again
  from zero after a mode change, so the new mode's figure shows within a second.
- **@ 16 bpp**: the panel mode in use.
- **bunnies**: how many there are, at most 1800.

The box and the text shake while A, B or SELECT is held, as on the Playdate.

## What it measures

Each frame does three things, one after the other, and the FPS is how many of these
frames fit in a second:

1. **Move**: every bunny moves by its speed and turns around at the box's edges.
   That is a few additions and compares per bunny, in quarter pixels (the
   CH32X035 has no FPU), so it costs little.
2. **Draw**: the frame buffer is cleared to white and every bunny is drawn with
   CHGfx's span sprite (`gfx_sprite4`), a 16x16 bunny made of runs of black and
   white. This is what grows with the bunny count, and what the benchmark is mostly
   about: how fast the chip fills pixels.
3. **Send**: the whole frame goes to the panel (`gfx_flush`), over SPI through DMA,
   and the game waits until it has gone. This costs the same whatever the bunny
   count, but depends on the panel mode.

With no bunnies the frame rate is close to what sending alone allows. With hundreds,
drawing dominates.

## The panel modes

CHGfx keeps its frame buffer at 4 bits per pixel (16 colours, 8 KB) whatever the
mode; the mode is only how pixels are sent to the panel:

| Mode | Bytes per frame | Sending one frame | Max fps from sending alone |
|---|---|---|---|
| 12 bpp | 24,576 | 8.4 ms | 119 |
| 16 bpp | 32,768 | 11.1 ms | 90 |
| 18 bpp | 49,152 | 16.4 ms | 61 |

(CHGfx's own figures.) BunnyMark only uses black and white, so all three look the
same: switching shows what the sending costs. With 100 bunnies the CHGame emulator
gives 60 fps at 12 bpp, 51 at 16 bpp and 39 at 18 bpp (the emulator's timing model,
not measured on a device).

## Differences from the Playdate version

- The screen is 128x128 instead of 400x240, so the bunny is 16x16 (half size) and
  moves at half the speed, so the motion looks the same. Every port uses this bunny, so
  their frame rates can be compared.
- Positions are integers in quarter pixels instead of floats.
- At most 1800 bunnies instead of 50,000: the chip has 20 KB of RAM and the frame
  buffer takes 8 KB. To fit that many, a bunny is 3 bytes: x and y (9 bits each) in
  quarter pixels, and each axis' speed as a 3-bit index into a table of eight (-10
  to +10 quarter pixels a frame, symmetric, so bouncing is `7 - index`). A bunny
  that passes the box's edge is put back on it, as the 9-bit positions would
  otherwise wrap. That needs the USB serial left out of the build (below).
- Eight speeds per axis give only 64 different paths, so each new bunny starts a
  random 0-63 frames along its path, or bunnies added together would lie on top of
  each other.
- START holds 3 s to leave, timed in milliseconds like the *_embedded games. The
  CHGame library's own START exit counts frames at an assumed 60 fps, and BunnyMark
  runs unthrottled, so that one is switched off.

## Building

Arduino IDE 2 or arduino-cli, with the CHGame board package, CHGfx and the CHGame
library (all in [bateske/CHGame](https://github.com/bateske/CHGame)'s board
package). Board options: Optimize **-O2**, peripherals **game**, USB **upload only**
(that last one frees the RAM the 1800 bunnies need). `tools/build_releases.py --only
CHGame` in the folder above builds it so, into `CHGame_BunnyMark.bin`, with the
libraries of a bateske/CHGame checkout (`--chgame-libs`).

`bunny.h` comes from `tools/make_sprites.py` in the folder above.
