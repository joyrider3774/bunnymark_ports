--#title "BunnyMark Lua"
--#version "1.0"
--#texture bunnytex "assets/bunny.png"

-- BunnyMark for Vircon32 in Lua, for the v32lua compiler (github.com/wedge1020/v32lua, native
-- Vircon32 API): the C version (../BunnyMark.c) line for line, so the two compare what the
-- language costs on the same console. A port of Timofffee's Playdate bunnymark, itself raylib's
-- textures_bunnymark by Ramon Santamaria.
--
-- Press A to add a bunny, B a hundred, X five hundred, Y a thousand, each once when the button is
-- let go; A + B takes them all away again. Every bunny moves and bounces off the box each frame;
-- the screen shows the frame rate, how much of a frame's CPU time the last frame took, the GPU
-- budget it asked for, and the count. See ../README.md.
--
-- The texture is the C version's, assets/bunny.png: the 16x16 bunny at 0,0 and a 4x4 black block
-- at 32,0 that is stretched into the box's lines. v32lua has no garbage collector, so the status
-- text is built only when a figure changes (the counts when a button changes them, the frame
-- rate and CPU once a second): a string built every frame would fill the RAM within minutes.

MAX_BUNNIES = 100000
CYCLES_PER_FRAME = 250000
GPU_PIXELS_PER_FRAME = 2073600  -- 640 x 360 x 9: the GPU's budget a frame

REGION_BUNNY = 0
REGION_BLACK = 1

-- the box the bunnies bounce in, as the Playdate's (5,40 390x195 on 400x240), on 640x360
BOX_X = 5
BOX_Y = 40
BOX_W = 630
BOX_H = 315
BUNNY_SIZE = 16

bx = {}
by = {}
bvx = {}
bvy = {}
bunniesCount = 0
pressed = false
resetHeld = false

fps = 0
cpuPercent = 0
statusText = ""
countText = ""

function instantiateBunnies(count)
    for i = 1, count do
        if bunniesCount < MAX_BUNNIES then
            bunniesCount = bunniesCount + 1
            bx[bunniesCount] = 320.0
            by[bunniesCount] = 180.0
            -- the Playdate's speeds halved, as the bunny is
            bvx[bunniesCount] = (math.random(0, 499) - 250) / 100.0
            bvy[bunniesCount] = (math.random(0, 499) - 250) / 100.0
        end
    end
end

-- A bunny turns round where it reaches the box's edge
function updateBunnies()
    local maxX = BOX_X + BOX_W - BUNNY_SIZE
    local maxY = BOX_Y + BOX_H - BUNNY_SIZE
    for i = 1, bunniesCount do
        local x = bx[i] + bvx[i]
        local y = by[i] + bvy[i]
        bx[i] = x
        by[i] = y
        if x > maxX or x < BOX_X then bvx[i] = -bvx[i] end
        if y > maxY or y < BOX_Y then bvy[i] = -bvy[i] end
    end
end

-- a line of the box: the 4x4 black block stretched
function drawBar(x, y, w, h)
    ioports.gpu.region = REGION_BLACK
    ioports.gpu.scaleX = w / 4.0
    ioports.gpu.scaleY = h / 4.0
    ioports.gpu.x = x
    ioports.gpu.y = y
    ioports.gpu.draw("zoom")
end

function drawRect(x, y, w, h)
    drawBar(x, y, w, 1)
    drawBar(x, y + h - 1, w, 1)
    drawBar(x, y, 1, h)
    drawBar(x + w - 1, y, 1, h)
end

-- the two lines of text, built again only when a figure in them changes
function updateTexts()
    -- the clear and the bunnies; the box and the text are a few thousand pixels more
    local gpu = math.floor((640 * 360 + bunniesCount * 16 * 16) / (GPU_PIXELS_PER_FRAME / 100))
    statusText = "FPS " .. fps .. "  CPU " .. cpuPercent .. "%  GPU " .. gpu .. "%"
    countText = "bunnies: " .. bunniesCount
end

function drawBunnies()
    ioports.gpu.clear()

    local rX = 0
    local rY = 0
    if pressed then
        rX = math.random(0, 3) - 2
        rY = math.random(0, 3) - 2
    end
    drawRect(BOX_X + rX, BOX_Y + rY, BOX_W, BOX_H)

    -- the BIOS font, drawn black
    ioports.gpu.multiply = 0xFF000000
    print(10 + rX, 10 + rY, statusText)
    print(400 + rX, 10 + rY, countText)
    ioports.gpu.multiply = 0xFFFFFFFF

    ioports.gpu.texture = bunnytex
    ioports.gpu.region = REGION_BUNNY
    for i = 1, bunniesCount do
        ioports.gpu.x = bx[i]
        ioports.gpu.y = by[i]
        ioports.gpu.draw()
    end
end

-- A gamepad button reads above 0 while it is held (the frames it has been) and below 0 once it is
-- up (-1 on the first frame after it is let go)
function checkButtons()
    local a = ioports.inp.A
    local b = ioports.inp.B
    local x = ioports.inp.X
    local y = ioports.inp.Y
    local before = bunniesCount

    if a > 0 and b > 0 then
        bunniesCount = 0
        resetHeld = true
    end
    -- a button adds its bunnies once, when it is let go; A + B was held at some point since A and
    -- B were last both up: their releases then end the reset and add nothing
    if not resetHeld then
        if a == -1 then instantiateBunnies(1) end
        if b == -1 then instantiateBunnies(100) end
    end
    if a <= 0 and b <= 0 then resetHeld = false end
    if x == -1 then instantiateBunnies(500) end
    if y == -1 then instantiateBunnies(1000) end

    -- the box shakes while a button is held, as on the Playdate
    pressed = a > 0 or b > 0 or x > 0 or y > 0
    if bunniesCount ~= before then updateTexts() end
end

function main()
    ioports.inp.gamepad = 0
    math.randomseed(ioports.tim.time)
    ioports.gpu.bgcolor = 0xFFFFFFFF

    ioports.gpu.texture = bunnytex
    ioports.gpu.region = REGION_BUNNY
    ioports.gpu.minX = 0
    ioports.gpu.minY = 0
    ioports.gpu.maxX = 15
    ioports.gpu.maxY = 15
    ioports.gpu.hotX = 0
    ioports.gpu.hotY = 0
    ioports.gpu.region = REGION_BLACK
    ioports.gpu.minX = 32
    ioports.gpu.minY = 0
    ioports.gpu.maxX = 35
    ioports.gpu.maxY = 3
    ioports.gpu.hotX = 32
    ioports.gpu.hotY = 0

    local frames = 0
    local secondStart = ioports.tim.frames
    updateTexts()

    while true do
        -- The cycle counter counts the cycles of the console's current frame and starts again at
        -- every frame, so work that runs past one frame would read low again. The frame counter
        -- says how many frames went by: whole frames plus the difference in cycles
        local startFrame = ioports.tim.frames
        local startCycles = ioports.tim.cycles

        checkButtons()
        updateBunnies()
        drawBunnies()

        -- the cycles this frame's work took, as a share of one frame's (past 100%: more than a
        -- frame, so the frame rate drops)
        local cycles = (ioports.tim.frames - startFrame) * CYCLES_PER_FRAME + ioports.tim.cycles - startCycles
        local cpu = math.floor(cycles * 100 / CYCLES_PER_FRAME)

        -- frames drawn in the last 60 of the console's frames
        frames = frames + 1
        local now = ioports.tim.frames
        if now - secondStart >= 60 then
            fps = math.floor(frames * 60 / (now - secondStart))
            cpuPercent = cpu
            frames = 0
            secondStart = now
            updateTexts()
        end

        system.wait()
    end
end
