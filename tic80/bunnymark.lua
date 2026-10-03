-- title:   BunnyMark
-- author:  joyrider3774
-- desc:    a port of Timofffee's Playdate bunnymark, itself raylib's textures_bunnymark
-- site:    https://github.com/joyrider3774/bunnymark_ports
-- license: MIT License
-- version: 1.0
-- script:  lua

-- A +1 bunny, B +100, X +500, Y +1000 (keys Z, X, A, S), each once when the button is let go;
-- A + B takes them all away again. Every bunny moves and bounces off the box each frame; the
-- frame rate and the count are shown on top. See README.md.
--
-- The 16x16 bunny is the size every port draws, so the frame rates can be compared, and it moves
-- at half the Playdate's speed (tiles 0, 1, 16 and 17, written by tools/make_sprites.py).
-- TIC-80 calls TIC() at most 60 times a second, so 60 is the highest frame rate it shows.

MAX_BUNNIES = 20000
BOX_X = 2
BOX_Y = 10
BOX_W = 236
BOX_H = 124
BUNNY = 16
-- Colours, written as literal numbers in the calls below: 0 black, 12 white, and 2 the colour of
-- the bunny's transparent pixels. v32lua (which turns this cart into a Vircon32 one) looks a
-- palette index up only when it is a literal: cls(c) with c a variable cleared the screen to
-- that number as a raw colour

bx = {}
by = {}
bvx = {}
bvy = {}
count = 0

down = {} -- the buttons that were down last frame
resetHeld = false
fps = 0
frames = 0
since = time()

function addBunnies(n)
 for i = 1, n do
  if count >= MAX_BUNNIES then return end
  count = count + 1
  bx[count] = BOX_X + (BOX_W - BUNNY) / 2
  by[count] = BOX_Y + (BOX_H - BUNNY) / 2
  -- the Playdate's (rand() % 500 - 250) / 50 pixels, halved as the bunny is
  bvx[count] = (math.random(0, 499) - 250) / 100
  bvy[count] = (math.random(0, 499) - 250) / 100
 end
end

-- true once, on the frame button id is let go
function released(id)
 local now = btn(id)
 local was = down[id]
 down[id] = now
 return was and not now
end

function TIC()
 -- A + B takes the bunnies away; letting go of them afterwards adds nothing
 if btn(4) and btn(5) then
  count = 0
  resetHeld = true
 end
 local a = released(4)
 local b = released(5)
 if not resetHeld then
  if a then addBunnies(1) end
  if b then addBunnies(100) end
 end
 if not btn(4) and not btn(5) then resetHeld = false end
 if released(6) then addBunnies(500) end
 if released(7) then addBunnies(1000) end

 local maxX = BOX_X + BOX_W - BUNNY
 local maxY = BOX_Y + BOX_H - BUNNY
 for i = 1, count do
  local x = bx[i] + bvx[i]
  local y = by[i] + bvy[i]
  bx[i] = x
  by[i] = y
  if x > maxX or x < BOX_X then bvx[i] = -bvx[i] end
  if y > maxY or y < BOX_Y then bvy[i] = -bvy[i] end
 end

 cls(12)
 for i = 1, count do
  spr(0, bx[i], by[i], 2, 1, 0, 0, 2, 2)
 end

 -- the box and the text shake while a button that adds bunnies is held, as on the Playdate
 local rx = 0
 local ry = 0
 if btn(4) or btn(5) or btn(6) or btn(7) then
  rx = math.random(0, 2) - 1
  ry = math.random(0, 2) - 1
 end
 rectb(BOX_X + rx, BOX_Y + ry, BOX_W, BOX_H, 0)
 print("FPS " .. fps, 3 + rx, 2 + ry, 0)
 print("bunnies: " .. count, 120 + rx, 2 + ry, 0)

 frames = frames + 1
 local now = time()
 if now - since >= 1000 then
  fps = math.floor(frames * 1000 / (now - since) + 0.5)
  frames = 0
  since = now
 end
end

-- <TILES>
-- 000:22220002222200002222000022220ccc22220c0c22220cc02200ccc022000ccc
-- 001:200022220000222200002222ccc02222c0c022220cc022220ccc0022cccc0022
-- 016:22220cc022220c0022220cc022220ccc22220cc0222200002222002222220022
-- 017:00c0222200c0222200c02222ccc0222200c02222000022222200222222002222
-- </TILES>

-- <PALETTE>
-- 000:0000005d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7ffffff94b0c2566c86333c57
-- </PALETTE>
