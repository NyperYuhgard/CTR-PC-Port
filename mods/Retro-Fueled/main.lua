-- Retro-Fueled for CTR Native PC Port
-- Ported from PS1 SDK mod by Nyper
-- Enables USF stacking, U-Turn reserve preservation, and blue fire on all pads

local B = mod.BUTTONS

local TURBO_PAD = 0x04
local ALLOW_ACCEL_KEY = 0x08

local USF_FLOOR = 0x4800
local USF_CAP   = 0x6000
local FIRE_STEP = 8
local RESERVE_FLOOR = 150

local U_TURN_BUTTONS = B.SQUARE | B.DOWN | B.CROSS

local prev  = {}
local wasRacing = false

local function initState(n)
    n = n or 8
    for i = 0, n - 1 do
        prev[i] = { reserves = 0, uTurn = false }
    end
end

local function isRacing()
    return mod.getGameMode() & 0x02 ~= 0
end

mod.hook("onInit", function()
    mod.log("Retro-Fueled loaded!")
    initState()
end)

mod.hook("onInput", function()
    if not isRacing() then return end

    local n = mod.getNumPlayers()
    for i = 0, n - 1 do
        local pad = mod.getGamepad(i)
        if pad then
            prev[i].uTurn = (pad.held & U_TURN_BUTTONS) == U_TURN_BUTTONS
        end
    end
end)

mod.hook("onFirePre", function()
    if not isRacing() then return end

    local ctx = mod.getHookContext()
    local idx = ctx.driverIndex
    if idx < 0 then return end

    local fireType = ctx.args[1]

    -- Only bypass accel prevention for turbo pads
    if (fireType & TURBO_PAD) ~= 0 then
        local d = mod.getDriver(idx)
        if d and d.valid and (d.actionsFlagSet & ALLOW_ACCEL_KEY) ~= 0 then
            mod.setDriverField(idx, "actionsFlagSet", d.actionsFlagSet & ~ALLOW_ACCEL_KEY)
        end
    end
end)

mod.hook("onUpdate", function()
    local racing = isRacing()
    if not racing then
        wasRacing = false
        return
    end
    if not wasRacing then
        wasRacing = true
        initState(mod.getNumPlayers())
    end
    if mod.getGameMode() & 0x10 ~= 0 then return end

    local n = mod.getNumPlayers()
    for i = 0, n - 1 do
        local d = mod.getDriver(i)
        if d and d.valid and prev[i].uTurn then
            mod.setDriverField(i, "reserves", RESERVE_FLOOR)
            mod.setDriverField(i, "fireSpeedCap", USF_CAP)
        end
    end
end)

mod.hook("onRender", function()
    if not wasRacing then return end
    if mod.getGameMode() & 0x10 ~= 0 then return end

    local n = mod.getNumPlayers()
    for i = 0, n - 1 do
        local d = mod.getDriver(i)
        if d and d.valid and d.reserves > 0 then
            if d.fireSpeedCap < USF_FLOOR then
                mod.setDriverField(i, "fireSpeedCap", USF_FLOOR)
            end

            local cap = d.fireSpeedCap + FIRE_STEP
            if cap > USF_CAP then cap = USF_CAP end
            mod.setDriverField(i, "fireSpeedCap", cap)
        end
    end
end)
