-- ============================================================
-- Reserve Bar Mod for CTR Native
-- Name: Reserve Bar
-- Description: Displays a visual reserve bar on the HUD
--              showing current boost reserves, with color
--              coding for Sacred Fire and Ultra Sacred Fire.
--              Also demonstrates behavior modification by
--              optionally keeping reserves from draining.
-- ============================================================

-- PS1 framebuffer coordinate space: 512 x 216
-- Screen center: 256 (0x100) horizontal, 108 (0x6C) vertical

-- ============================================================
-- Configuration
-- ============================================================
local CONFIG =
{
    -- Bar position and size (PS1 OT coordinates)
    barX       = 0x0A,    -- Left edge (10)
    barY       = 0x68,    -- Top edge (104) - just below the existing HUD
    barWidth   = 0x3C,    -- Width (60 pixels)
    barHeight  = 0x06,    -- Height (6 pixels)

    -- Outline
    outlineW   = 0x3C,    -- Outline width (same as bar)
    outlineH   = 0x06,    -- Outline height (same as bar)

    -- Colors (R, G, B)
    colorNoReserve     = { 0x40, 0x40, 0x40 },  -- Gray (empty/no reserves)
    colorNormal        = { 0x00, 0xFF, 0x00 },   -- Green (normal reserves)
    colorSacredFire    = { 0xFF, 0xFF, 0x00 },   -- Yellow (Sacred Fire)
    colorUltraSacred   = { 0xFF, 0x40, 0x00 },   -- Orange (Ultra Sacred Fire)
    colorOutline       = { 0x00, 0x00, 0x00 },   -- Black (outline)
    colorBg            = { 0x20, 0x20, 0x20 },   -- Dark gray (background)

    -- Reserve display
    maxReserveDisplay  = 4800,   -- Max reserve value for full bar
    showText           = true,   -- Show "RESERVE" label
    labelFont          = 2,      -- 1=big, 2=small

    -- Behavior modification
    infiniteReserves   = false,  -- If true, reserves never drain
    reserveCap         = 0,      -- If > 0, cap reserves to this value (0 = no cap)
}

-- ============================================================
-- State
-- ============================================================
local lastReserve = 0
local smoothReserve = 0

-- ============================================================
-- Utility: Clamp a value between min and max
-- ============================================================
local function clamp(val, min, max)
    if val < min then return min end
    if val > max then return max end
    return val
end

-- ============================================================
-- Utility: Lerp (linear interpolation)
-- ============================================================
local function lerp(a, b, t)
    return a + (b - a) * t
end

-- ============================================================
-- Determine the reserve bar color based on fire level
-- ============================================================
local function getReserveColor(driver)
    if not driver.valid then
        return CONFIG.colorNoReserve
    end

    local fireSpeedCap = driver.fireSpeedCap
    local sacredFireSpeed = driver.const_SacredFireSpeed
    local singleTurboSpeed = driver.const_SingleTurboSpeed

    -- Ultra Sacred Fire: speed cap exceeds Sacred Fire threshold
    -- (this means they're on a super turbo pad)
    if fireSpeedCap > sacredFireSpeed then
        return CONFIG.colorUltraSacred
    end

    -- Sacred Fire: speed cap exceeds single turbo threshold
    if fireSpeedCap > singleTurboSpeed then
        return CONFIG.colorSacredFire
    end

    -- Normal reserves
    return CONFIG.colorNormal
end

-- ============================================================
-- Draw the reserve bar for a given player
-- ============================================================
local function drawReserveBar(playerIndex, posX, posY)
    local driver = mod.getDriver(playerIndex)

    if not driver.valid then
        return
    end

    -- If reserves are 0 and not on a turbo pad, don't draw
    if driver.reserves <= 0 and driver.turbo_outsideTimer <= 0 then
        -- Draw empty bar outline only
        mod.drawRect(posX, posY, CONFIG.barWidth, CONFIG.barHeight,
                     CONFIG.colorOutline[1], CONFIG.colorOutline[2], CONFIG.colorOutline[3])
        mod.drawRect(posX + 1, posY + 1, CONFIG.barWidth - 2, CONFIG.barHeight - 2,
                     CONFIG.colorBg[1], CONFIG.colorBg[2], CONFIG.colorBg[3])

        if CONFIG.showText then
            mod.drawText("RESERVE", posX + CONFIG.barWidth + 4, posY - 1, CONFIG.labelFont, 0)
        end
        return
    end

    -- Calculate fill amount
    local reserveVal = driver.reserves
    if reserveVal < 0 then reserveVal = 0 end

    local fillRatio = reserveVal / CONFIG.maxReserveDisplay
    fillRatio = clamp(fillRatio, 0, 1)

    local fillWidth = math.floor(fillRatio * (CONFIG.barWidth - 2) + 0.5)
    if fillWidth < 1 and reserveVal > 0 then fillWidth = 1 end

    -- Get color based on fire level
    local color = getReserveColor(driver)

    -- Draw outline (black border)
    mod.drawRect(posX, posY, CONFIG.barWidth, CONFIG.barHeight,
                 CONFIG.colorOutline[1], CONFIG.colorOutline[2], CONFIG.colorOutline[3])

    -- Draw background (dark gray, inside outline)
    mod.drawRect(posX + 1, posY + 1, CONFIG.barWidth - 2, CONFIG.barHeight - 2,
                 CONFIG.colorBg[1], CONFIG.colorBg[2], CONFIG.colorBg[3])

    -- Draw fill bar (colored, on top of background)
    if fillWidth > 0 then
        mod.drawRect(posX + 1, posY + 1, fillWidth, CONFIG.barHeight - 2,
                     color[1], color[2], color[3])
    end

    -- Draw label
    if CONFIG.showText then
        mod.drawText("RESERVE", posX + CONFIG.barWidth + 4, posY - 1, CONFIG.labelFont, 0)
    end
end

-- ============================================================
-- Hooks
-- ============================================================

-- Called once when the mod is initialized
mod.hook("onInit", function()
    mod.log("Reserve Bar mod initialized!")
    mod.log("Position: X=" .. CONFIG.barX .. " Y=" .. CONFIG.barY)
    mod.log("Size: " .. CONFIG.barWidth .. "x" .. CONFIG.barHeight)
    if CONFIG.infiniteReserves then
        mod.log("Infinite reserves: ENABLED")
    end
    if CONFIG.reserveCap > 0 then
        mod.log("Reserve cap: " .. CONFIG.reserveCap)
    end
end)

-- Called every frame during rendering — this is where we draw the bar
mod.hook("onRender", function()
    local numPlayers = mod.getNumPlayers()

    if numPlayers < 1 then
        return
    end

    -- For 1 player: draw below the existing slide meter position
    if numPlayers == 1 then
        drawReserveBar(0, CONFIG.barX, CONFIG.barY)
    else
        -- For multiplayer, draw smaller bars for each player
        local playerPositions =
        {
            { 0x08, 0x02 },  -- P1: top-left
            { 0x88, 0x02 },  -- P2: top-right
            { 0x08, 0x6C },  -- P3: bottom-left
            { 0x88, 0x6C },  -- P4: bottom-right
        }

        for i = 0, math.min(numPlayers - 1, 3) do
            local pos = playerPositions[i + 1]
            drawReserveBar(i, pos[1], pos[2])
        end
    end
end)

-- Called every frame during game logic — for state updates AND behavior modification
mod.hook("onUpdate", function()
    local driver = mod.getDriver(0)

    if driver.valid then
        lastReserve = driver.reserves
        smoothReserve = lerp(smoothReserve, lastReserve, 0.3)

        -- ============================================================
        -- BEHAVIOR MODIFICATION EXAMPLES
        -- These demonstrate how mods can CHANGE game behavior,
        -- not just observe it.
        -- ============================================================

        -- Example 1: Infinite reserves
        -- When the player has reserves, keep replenishing them
        -- so they never drain. This demonstrates mod.setDriverField().
        if CONFIG.infiniteReserves and lastReserve > 0 then
            mod.setDriverField(0, "reserves", 9600)
        end

        -- Example 2: Reserve cap
        -- Limit how many reserves a player can accumulate.
        -- Useful for competitive mods where you want to prevent
        -- hoarding reserves.
        if CONFIG.reserveCap > 0 and lastReserve > CONFIG.reserveCap then
            mod.setDriverField(0, "reserves", CONFIG.reserveCap)
        end

        -- Example 3: Advanced — direct memory write via GameTracker
        -- You can get the GameTracker pointer and read/write any
        -- offset. This gives full access to ALL game state.
        --
        -- local gGT = mod.getGameTracker()
        -- if gGT then
        --     -- Read gameMode1 (offset 0x000, s32)
        --     local mode = mod.readS32(gGT, 0x000)
        --     -- Read number of players (offset 0x343, u8)
        --     local numPlayers = mod.readU8(gGT, 0x343)
        --     mod.log("Mode=" .. mode .. " Players=" .. numPlayers)
        -- end
    else
        lastReserve = 0
        smoothReserve = 0
    end
end)
