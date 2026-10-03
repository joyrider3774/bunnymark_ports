# BunnyMark for the Gamebuino AKA

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria. Bunnies bounce around a
box; you keep adding them and watch the frame rate drop.

## Buttons

| Button | What it does |
|---|---|
| A | +1 bunny |
| B | +100 bunnies |
| C | +500 bunnies |
| D | +1000 bunnies |
| A + B | back to 0 bunnies |
| L1 | screen refresh rate one step down (100 → 80 → 60 → 40 Hz) |
| R1 | screen refresh rate one step up |
| MENU + RUN (hold 0.5 s) | back to the launcher |
| RUN on its own | switches the AKA off, as in every AKA game |

Bunnies are added, and the refresh rate changed, once per press, when the button is
let go. Letting go of A and B after A + B adds nothing.

## The screen

```
FPS 81 @ 100 Hz          bunnies: 100
+-----------------------------------+
|        bunnies bouncing           |
+-----------------------------------+
```

- **FPS**: frames per second, counted over the last second. It is counted again
  from zero after a refresh rate change.
- **@ 100 Hz**: the refresh rate asked of the screen.
- **bunnies**: how many there are, at most 50,000 (as on the Playdate).

The box and the text shake while A, B, C or D is held, as on the Playdate.

## What it measures

Each frame does these things, one after the other, and the FPS is how many of these
frames fit in a second:

1. **Move**: every bunny moves by its speed and turns around at the box's edges, in
   floats, exactly the Playdate's code. The bunnies live in PSRAM, so with
   thousands of them this also reads and writes external RAM.
2. **Draw**: the frame buffer (320x240, 16-bit, in internal RAM) is cleared to white
   and every bunny is drawn: the 16x16 bunny, as runs of black and white filled
   straight into the buffer. This is what grows with the bunny count: how fast the
   chip fills pixels.
3. **Wait for the screen**: the library waits for the panel's tearing signal (its
   next refresh) before sending a frame, so a frame never tears.
4. **Send**: the frame goes to the panel over the 8-bit parallel bus through DMA,
   150 KB in about 7.7 ms, and the library waits until it has gone.

Because of step 3, the FPS can only be the refresh rate, half of it, a third of it,
and so on: a frame that does not fit in one refresh waits for the next one.

## The refresh rate

The library sets the screen's colour mode once (16-bit) and offers no other, but it
lets a game change the refresh rate while it runs (`graphics.set_refresh_rate`, 40 to
100 Hz). Since the library waits for each refresh, that rate is the ceiling on the
frame rate.

With 100 bunnies the AKA emulator gives (its timing model, not measured on a device):

| Refresh rate | FPS |
|---|---|
| 100 Hz | 81 |
| 80 Hz | 79 |
| 60 Hz | 59 |
| 40 Hz | 39 |

At 80, 60 and 40 Hz a frame fits in one refresh. At 100 Hz drawing plus sending is right
on the edge of 10 ms, so some frames take one refresh and some two. With more bunnies
every frame takes two refreshes at the higher rates, and then a lower refresh rate can
give a higher frame rate.

The library turns the rate into the panel's own divider, which has coarse steps, so
the screen's actual rate can be a little off from the label.

## Differences from the Playdate version

- The screen is 320x240 instead of 400x240: the box is 80 pixels narrower. The bunnies
  bounce at its edges; the movement code is otherwise the Playdate's.
- The bunny is 16x16, the Playdate's image at half size, and moves at half its speed:
  the size every port uses so their frame rates can be compared.
- RUN is read past the library: the library's button poll switches the AKA off as
  soon as it sees RUN, which would make MENU + RUN impossible. RUN on its own still
  switches off when it is let go.

## Building

An ESP-IDF project against [Gamebuino_AKA_lib](https://github.com/jmp42/Gamebuino_AKA_lib)
(`AKA_LIB_DIR`, default `C:/github/Gamebuino_AKA_lib`): `idf.py build`, or
`tools/build_releases.py --only Aka` in the folder above, which makes
`Aka_BunnyMark.zip`: the folder for the AKA launcher's SD card, with the app as
`firmware.bin` beside `card/bunnymark`'s `meta.json` and 160x120 `screen.bmp`. Unzip it
onto the card.

`main/bunny.h` comes from `tools/make_sprites.py` in the folder above.
