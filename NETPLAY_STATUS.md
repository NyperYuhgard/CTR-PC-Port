# CTR PC Port — Netplay Implementation Status

## Project Overview

This document describes the netplay (online multiplayer) implementation for the
CTR PC Port project (`ctr_native`). The goal is two-player peer-to-peer racing
over UDP, with each player seeing their own full-screen camera (no split-screen).

**Build system:** CMake + MinGW Makefiles, 32-bit (`i686`), C99 (`-std=c99`).
**Platform:** Windows native (SDL/PsyCross), with `CTR_NATIVE` defines guarding
all native additions.

---

## Architecture

### Networking Model
- **Peer-to-peer UDP** — host/client model (no dedicated server).
- **Default port:** 14200.
- **Input replay** — each machine runs the full game simulation locally, receives
  the remote player's gamepad state every frame, and applies it to the correct
  driver. There is no rollback, no prediction, and no frame-number matching.
- **Packet types** (see `include/platform/native_netplay.h:10-20`):
  - `HELLO` (0x01) — handshake
  - `INPUT` (0x02) — gamepad state per frame
  - `PING` (0x03) / `PONG` (0x04) — latency measurement
  - `DISCONNECT` (0x05)
  - `START_RACE` (0x06) — host signals race preparation
  - `CHARACTER_SELECT` (0x07) — both directions
  - `TRACK_SELECT` (0x08) — host→client

### Input Mapping (both machines)

```
gamepad[0] = host's physical input
gamepad[1] = client's physical input
driver[0]  = host's character → reads from gamepad[0]
driver[1]  = client's character → reads from gamepad[1]
```

The input sync block is in `game/MAIN/MainMain.c:356-437`.

### Camera Mapping (after our fix)

**Host machine:**
```
cameraDC[0] → follows driver[0] (host) → writes to pushBuffer[0] → RENDERED full-screen
cameraDC[1] → follows driver[1] (client) → writes to pushBuffer[1] → NOT rendered
Host sees: driver[0]'s perspective ✓
```

**Client machine** (after camera swap):
```
cameraDC[0] → follows driver[1] (client) → writes to pushBuffer[0] → RENDERED full-screen
cameraDC[1] → follows driver[0] (host) → writes to pushBuffer[1] → NOT rendered
Client sees: driver[1]'s perspective ✓
```

### Flow: Lobby → Race

1. User joins/hosts → `main.c` calls `Netplay_Host()` or `Netplay_Connect()`
2. Lobby UI (`230_22_MM_Online_Menu.c`) handles character/track selection via
   `NETPLAY_PACKET_CHARACTER_SELECT` / `NETPLAY_PACKET_TRACK_SELECT`
3. `Online_StartRace()` (`230_22_MM_Online_Menu.c:62`) is called on both
   machines:
   - Sets `g_NetplayRacing = 1`
   - Sets `data.characterIDs[0]` = host char, `data.characterIDs[1]` = client char
   - Sets `gGT->numPlyrCurrGame = 2`, `gGT->numPlyrNextGame = 2`
   - Triggers track loading via `data.menuQueueLoadTrack`
4. During loading: `LOAD_44_TenStages.c` copies `numPlyrNextGame → numPlyrCurrGame`
5. `MainInit_06_Drivers.c` spawns 2 drivers (skips AI because `!g_NetplayRacing`)
6. `MainInit_07_FinalizeInit.c`:
   - Initializes 4 pushBuffers with 2P split-screen rects
   - Creates cameras for both drivers
   - **New code:** swaps `driverToFollow` for client, re-inits pushBuffer[0] with 1P rect
7. Main game loop (`MainMain.c`) begins, input sync runs every frame
8. **New code:** during `MainFrame_RenderFrame`, sets `numPlyrCurrGame = 1` so
   only 1 viewport is rendered full-screen, no split-screen lines

### Key Global Variables

| Variable | File | Purpose |
|----------|------|---------|
| `g_NetplayAutoJoin` | `native_netplay.c:130` | CLI flag to auto-connect |
| `g_NetplayRaceStarting` | `native_netplay.c:131` | Client received race-start signal |
| `g_NetplayRacing` | `native_netplay.c:132` | Currently in a netplay race |
| `g_NetplayHostCharacter` | `native_netplay.c:133` | -1 or host's char index |
| `g_NetplayClientCharacter` | `native_netplay.c:134` | -1 or client's char index |
| `g_NetplayTrackId` | `native_netplay.c:135` | Selected track LEV ID |
| `g_NetplayNumLaps` | `native_netplay.c:136` | Number of laps (default 3) |

---

## Changes Made

### 1. Periodic Ping (`platform/native_netplay.c`)

**Problem:** Ping/Pong infrastructure existed but was never triggered.

**Fix:** Added a 1-second periodic `NETPLAY_PACKET_PING` broadcast in
`Netplay_Poll()` (`native_netplay.c:1017`). Each ping carries a timestamp;
the peer echoes it back as `NETPLAY_PACKET_PONG`. RTT is calculated and stored
in `playerInfo[].pingMs`.

**File:** `platform/native_netplay.c:1017+` (inside `Netplay_Poll()`)

### 2. g_NetplayRacing Cleared on Menu Return

**Problem:** Returning to the title screen or disconnecting left
`g_NetplayRacing = 1`, causing the input sync block to override gamepad
states in menus (making menus unresponsive).

**Fix:** Set `g_NetplayRacing = 0` in:
- `MM_JumpTo_Title_FirstTime()` (`230_72_MM_JumpTo_Title_FirstTime.c:34`)
- `Netplay_Disconnect()` (`native_netplay.c:875`)

### 3. Character Selection Flow

**Problem:** Multiple issues:
- `g_NetplayHostCharacter` / `g_NetplayClientCharacter` initialized to `0`,
  which collided with Crash's character index.
- Host never sent its character choice to client.
- Client didn't wait for host's character before starting.

**Fixes:**
- Changed sentinel from `0` to `-1` for "not selected".
- Host now broadcasts `NETPLAY_PACKET_CHARACTER_SELECT` with its choice
  (`230_22_MM_Online_Menu.c:418-421`).
- Client handler stores host's character in `g_NetplayHostCharacter`
  (`native_netplay.c:612-622`).
- Host waits for `g_NetplayClientCharacter >= 0` before proceeding to track select.
- Client waits for both `g_NetplayRaceStarting && g_NetplayHostCharacter >= 0`
  before calling `Online_StartRace()`.

### 4. numPlyrNextGame Fix

**Problem:** `Online_StartRace()` set `gGT->numPlyrCurrGame = 2` but not
`numPlyrNextGame`. During track loading (`LOAD_44_TenStages.c:111`),
`numPlyrCurrGame` was overwritten with the stale `numPlyrNextGame` (=1),
causing only 1 driver to spawn.

**Fix:** Added `gGT->numPlyrNextGame = playerCount` in
`Online_StartRace()` (`230_22_MM_Online_Menu.c:71`).

### 5. No Split-Screen (Single Viewport per Machine)

**Problem:** With `numPlyrCurrGame = 2`, the renderer produces split-screen
(top/bottom halves), each showing a different driver's camera. Each player
should see their own character full-screen.

**Fix:** Two changes:

#### 5a. Camera Swap for Client (`MainInit_07_FinalizeInit.c:132-148`)

After camera initialization, when `g_NetplayRacing`:
- For `localId != 0` (client): swaps `cameraDC[0].driverToFollow` ↔
  `cameraDC[1].driverToFollow` so camera[0] follows the local (client) driver.
- Re-inits `pushBuffer[0]` with `PushBuffer_Init(&pushBuffer[0], 0, 1)` to get
  full-screen viewport rect (`0x200×0xD8`) instead of the 2P top-half rect
  (`0x200×0x6A`).

Includes `<platform/native_netplay.h>` for access to `Netplay_GetLocalPlayerId()`.

#### 5b. Override numPlyrCurrGame During Render (`MainMain.c:603-616`)

Before `MainFrame_RenderFrame()`:
- Saves `gGT->numPlyrCurrGame`
- Sets `gGT->numPlyrCurrGame = 1` when `g_NetplayRacing`
- After RenderFrame, restores original value

This causes all rendering functions to use the 1P code path:
- Single viewport (pushBuffer[0]) rendered full-screen
- Split-screen divider lines skipped
- Level geometry rendered via 1P path (`DrawLevelOvr1P`, `AnimateWater1P`, etc.)

---

## Known Bugs & Issues

### Critical

#### B1. Client Cannot Pause/Unpause

**Location:** Multiple places check `gamepad[0].buttonsTapped`, e.g.
`UI_44_RenderFrame_Racing.c:86`, pause logic in MainMain.c.

**Problem:** `gamepad[0]` = host's input on BOTH machines. On the client,
pressing START on the physical controller goes into `gamepad[1]`, but pause
logic checks `gamepad[0]` exclusively. The client can never pause.

**Fix needed:** Modify all gamepad[0]-specific checks to also check gamepad[1],
or add netplay-specific pause handling (e.g., sync pause state over network).
The simplest fix: check `gamepad[0] || gamepad[1]` for pause in the main loop,
and broadcast pause state so both machines pause together.

#### B2. HUD Draws for Both Drivers (Garbage for Second)

**Location:** `UI_44_RenderFrame_Racing.c`

**Problem:** The HUD loop at line 135 iterates over ALL player threads
(`gGT->threadBuckets[PLAYER].thread` linked list), NOT over `numPlyrCurrGame`.
With 2 drivers, both get HUD drawn. But `hudStructPtr` at line 54 is selected
based on `numPlyrCurrGame = 1`, providing only 1 set of HUD element positions.
The second driver's HUD is drawn using out-of-bounds position data (reading
beyond the 1P layout array).

**Impact:** Visual corruption — extra HUD elements at garbage screen positions.
Could potentially crash if garbage coordinates cause invalid GPU operations.

**Fix needed:** Either:
- (a) Skip the second player thread iteration when `g_NetplayRacing`, or
- (b) Change the loop to use `numPlyrCurrGame` instead of thread linked list
      iteration, or
- (c) Create a netplay-specific HUD path that only draws for the local player.

Option (b) is cleanest: change `do { ... } while (playerThread != 0)` to
`for (int i = 0; i < numPlyr; i++)` with early guard: on host, skip `i == 1`;
on client, skip `i == 0`.

#### B3. "Controller 2 Unplugged" Warning

**Location:** `MainFrame_08_RenderFrame.c:22` (`DrawUnpluggedMsg`)

**Problem:** `DrawUnpluggedMsg` checks `MainFrame_HaveAllPads(gGT->numPlyrNextGame)`.
Since `numPlyrNextGame = 2`, it expects 2 physical controllers. On both machines,
only 1 physical controller is plugged in (the local one).

**Fix needed:** In `DrawUnpluggedMsg`, skip the pad check when `g_NetplayRacing`
(or reduce expected pads to 1 for netplay).

### Medium

#### B4. No Frame-Number Matching in Input Sync

**Location:** `MainMain.c:356-437`, `native_netplay.c:986-998`

**Problem:** `Netplay_ReceiveInputs()` dequeues whatever is in the input queue
regardless of frame number. There's no check that the received input frame
matches the current local frame. Combined with different loading times
(machines start racing at different frame counts), this means:
- Host at frame N uses client's input from frame N-delta (old input)
- Client at frame M uses host's input from frame M-delta (old input)
- The delta is the difference in start times, typically 10-30 frames

**Fix needed:** Implement frame-number matching so inputs are applied to the
correct simulation frame. This requires either:
- Lockstep: both machines wait for each other at each frame
- Delay-based: buffer inputs for a fixed number of frames
- Rollback: predict and correct

The simplest short-term fix: add a frame number parameter to
`Netplay_ReceiveInputs` and skip inputs that don't match the current frame,
falling back to the last matched input or neutral state.

#### B5. Race Start Not Synchronized

**Location:** `Online_StartRace()` triggers track loading; the 3-2-1 countdown
starts independently on each machine when loading finishes.

**Problem:** Machine A might finish loading 500ms before Machine B. The
countdown starts at a different wall-clock time, and the frame counters diverge.
Coupled with B4, this causes input-to-frame mismatch.

**Fix needed:** Sync the race start:
- Both machines signal "ready" after loading completes
- Host broadcasts "GO" after both are ready
- Both machines start countdown on the same frame

#### B6. Vehicle Position / Physics Desync

**Location:** Entire simulation (race logic in overlays, physics in
`Veh*` files)

**Problem:** Using pure input replay, any difference in frame timing, physics
computation, or random number generation causes vehicle positions to diverge.
Over a 3-lap race (2-3 minutes), the divergence can be significant.

**Fix needed:** Periodic position sync packets or state checksum comparison.
At minimum, broadcast each driver's position/velocity every N frames and
apply corrections (with interpolation to avoid snapping).

#### B7. Items / Powerups / Crates Not Synced

**Problem:** Item crates (`pickup_types`), weapon pickups, wumpa fruit, and
crate destruction states are local to each machine. Player A picks up a crate
on Machine A, but Machine B still shows the crate as available. The RNG for
item assignment is also local, so Player A might get a missile while Player B
(seeing the same pickup) gives Player A a shield.

**Fix needed:** Either:
- Broadcast crate pickup events
- Use a shared RNG seed (but this requires deterministic execution)

#### B8. Race Finish / Results Not Synced

**Problem:** Finish line crossing, lap counting, and race results are computed
locally. Both machines may disagree on who finished first, especially if
positions have drifted (B6).

**Fix needed:** Sync finish events and reconcile results. The host should be
authoritative for race results, or both machines should agree via checksum.

### Low

#### B9. Camera Thread Uses cameraID for Some Logic

**Location:** `CAM_ThTick` (in overlay 226-229), various camera mode functions

**Problem:** After our `driverToFollow` swap on the client, `cameraDC[0]`
follows `driver[1]` but `cameraDC[0].cameraID` is still `0`. Some camera
functions may use `cameraID` (not `driverToFollow`) to index into arrays.
If `cameraID` is used to read `driver[cameraID]`, the camera would get the
wrong driver.

**Verification needed:** Search for uses of `cDC->cameraID` vs
`cDC->driverToFollow` in camera code (overlay 226-229). If cameraID is used
to look up drivers or pushBuffers, the swap approach may need adjustment.
A safer approach would be to swap the pushBuffer pointers instead of
driverToFollow.

#### B10. Disconnection During Race

**Problem:** If a player disconnects mid-race, there's no handling. The peer's
input stops arriving (gamepad becomes neutral/zero), their driver coasts to a
stop, and the race continues with a stationary car on track. No notification
or pause.

**Fix needed:** Handle disconnect events during race: show message, optionally
pause, and end the race gracefully.

#### B11. 60fps Interpolation Only Processes pushBuffer[0]

**Location:** `MainMain.c:526-563`

**Problem:** The 60fps interpolation code uses `gGT->numPlyrCurrGame` to
determine how many pushBuffers to save/interpolate. Since we set it to 1
before the interpolation block, only pushBuffer[0] gets interpolated.
pushBuffer[1] keeps stale camera data. This doesn't affect rendering (only
pushBuffer[0] is rendered), but if any code reads pushBuffer[1] position,
it gets interpolated or stale values.

**Impact:** Minimal, since pushBuffer[1] is not used for anything critical
during netplay racing.

#### B12. PushBuffer[1] Retains 2P Split-Screen Rect

**Location:** `MainInit_07_FinalizeInit.c`

**Problem:** We re-initialize `pushBuffer[0]` with 1P rect, but `pushBuffer[1]`
still has the 2P bottom-half rect from the original init. If anything reads
`pushBuffer[1].rect`, it gets y=0x6E, h=0x6A (bottom half).

**Impact:** Minimal for now. If a future feature reads pushBuffer[1].rect for
positioning, it'll be wrong.

---

## Task List

### Priority 1: Must Fix (Game-Breaking)

#### T1. Fix Pause for Client

**Files:** `game/MAIN/MainMain.c` (around line 580-592 in the main loop),
`game/UI/UI_44_RenderFrame_Racing.c:86`

**What to do:**
Add netplay-aware pause handling. When `g_NetplayRacing`:
- Check both `gamepad[0]` and `gamepad[1]` for START press
- Broadcast pause state via a new packet type
- Both machines pause/unpause together when either player presses START

**Simple alternative:**
Just check `gamepad[0]` or `gamepad[1]` for pause when `g_NetplayRacing`.
This lets either player pause locally but doesn't sync the pause state.
Better than nothing.

#### T2. Fix HUD for Single Viewport

**Files:** `game/UI/UI_44_RenderFrame_Racing.c`

**What to do:**
Change the player-thread loop (line 135-579) to iterate over
`numPlyrCurrGame` instead of following the thread linked list. When
`g_NetplayRacing && numPlyrCurrGame == 1`, only the local player's HUD
is drawn.

Specifically:
- Change `do { ... } while (playerThread != 0)` to
  `for (int i = 0; i < numPlyr; i++)`
- Use `gGT->drivers[i]` instead of `playerThread->object`
- On the host, only `i == 0` should draw (driver[0] = host)
- On the client (after camera swap), `i == 1` should draw (driver[1] = client)
- But since `numPlyrCurrGame = 1` during render, only `i == 0` iterates
- On client, `gGT->drivers[0]` is the host's driver, but camera shows client's view
- So we need: host → draw for driver[0], client → draw for driver[1]

**Better approach:**
After the camera swap on the client, swap which driver is at index 0 for HUD
purposes. Or use `cameraDC[0].driverToFollow` to determine which driver's HUD
to draw (but HUD code doesn't have easy access to cameraDC).

**Simplest fix:**
Temporarily (before HUD drawing) swap `gGT->drivers[0]` and `gGT->drivers[1]`
on the client machine, so `drivers[0]` = the local player. Then the existing
1P HUD code (which draws for `drivers[0]`) shows the correct stats.
Restore after HUD drawing.

#### T3. Suppress Controller Unplugged Warning

**Files:** `game/MAIN/MainFrame_08_RenderFrame.c` in `DrawUnpluggedMsg()` (~line 222)

**What to do:**
Add early return when `g_NetplayRacing`:
```c
if (g_NetplayRacing) return;
```
at the start of `DrawUnpluggedMsg`, or after the existing early-return checks.

Or, more precisely, change the pad check from `gGT->numPlyrNextGame` to `1`
when `g_NetplayRacing`.

### Priority 2: Important for Playability

#### T4. Implement Frame-Synced Input

**Files:** `platform/native_netplay.c` (Netplay_ReceiveInputs),
`game/MAIN/MainMain.c` (input sync block)

**What to do:**
Add frame-number matching. `Netplay_ReceiveInputs` should take a `currentFrame`
parameter and only return inputs whose `frameNum` matches (or is close to)
the current frame. Inputs for wrong frames are discarded or buffered.

This requires:
1. Both machines to agree on a starting frame (send it in START_RACE packet)
2. Each machine tracks its frame offset from the start
3. Inputs are tagged with the absolute frame number
4. On receive, inputs with frameNum != currentFrame are either buffered (if
   ahead) or discarded (if behind, use last known input as fallback)

#### T5. Sync Race Start

**Files:** `game/230/230_22_MM_Online_Menu.c` (Online_StartRace),
`platform/native_netplay.c`

**What to do:**
After both machines finish loading:
1. Each sends a "ready" packet to the other
2. Host counts down (e.g., 90 frames = 3 seconds) after receiving client's ready
3. Host broadcasts "GO" packet with start frame number
4. Both machines start the race countdown on the same frame

### Priority 3: Polish & Robustness

#### T6. Periodic Position Sync

**Files:** `platform/native_netplay.c`, `game/MAIN/MainMain.c`

**What to do:**
Every N frames (e.g., 30 = once per second), broadcast each driver's position,
velocity, and rotation. On receive, apply a correction if the difference
exceeds a threshold (with interpolation to avoid snapping).

New packet type: `NETPLAY_PACKET_POSITION_SYNC` (0x09).

#### T7. Item Crate Sync

**Files:** `platform/native_netplay.c`, crate pickup code (overlays)

**What to do:**
When a player picks up a crate, broadcast the crate index. The remote machine
marks that crate as collected. Items assigned by RNG need a shared seed or
the host to broadcast the item type awarded.

#### T8. Sync Race Finish / Results

**Files:** Race finish detection code, `platform/native_netplay.c`

**What to do:**
Broadcast finish-line crossing events. The host is authoritative for final
race positions. On disconnect, remaining player automatically wins.

#### T9. Handle Mid-Race Disconnect

**Files:** `platform/native_netplay.c`

**What to do:**
When `Netplay_HandleDisconnect` fires during `g_NetplayRacing`, show a
notification and end the race gracefully (or pause and offer return to menu).

### Priority 4: Verification & Cleanup

#### T10. Verify Camera Swap Robustness

**Files:** Camera overlay functions (226-229), `namespace_Camera.h`

**What to do:**
Check that all camera mode functions use `cDC->driverToFollow` to access
the driver, not `cDC->cameraID`. If any code does `gGT->drivers[cDC->cameraID]`,
the swap approach breaks and needs to be changed to swap pushBuffer pointers
instead of driverToFollow.

#### T11. Audio Localization

**Problem:** Engine sounds, item sounds, and other audio are positional based on
camera position. Both machines play audio for both drivers (since both drivers
exist in the simulation). The client hears host's engine as if it's nearby.

**Fix:** Filter audio sources based on whether they belong to the local player
or nearby players. This is low priority — the audio might be acceptable as-is.

#### T12. Improve Ping Display

**Location:** `230_22_MM_Online_Menu.c`

**Current:** Status shows "Connected" but no ping.
**Add:** Display pingMs for each peer in the lobby.

---

## Testing Instructions

1. **Build:**
   ```
   cd build
   cmake .. -G "MinGW Makefiles" -DCMAKE_C_FLAGS="-std=c99"
   mingw32-make -j4
   ```

2. **Run (two instances on same machine):**
   ```
   # Terminal 1 (host):
   ./ctr_native.exe --netplay host

   # Terminal 2 (client):
   ./ctr_native.exe --netplay connect 127.0.0.1
   ```

3. **Test flow:**
   - Both machines reach the ONLINE menu
   - Host presses START when "Players: 2" shown
   - Both pick characters (can be same or different)
   - Host picks track and laps, presses X
   - Race loads on both machines
   - Both cars should appear on track, each machine shows its own driver
     full-screen
   - Both drivers respond to their respective inputs
   - Split-screen divider line should NOT appear

4. **Known failure modes:**
   - Controller unplugged warning may appear (B3, need T3)
   - Client may not be able to pause (B1, need T1)
   - HUD may show duplicate/garbled elements (B2, need T2)
   - Over time, cars may drift apart in position (B6, need T6)

---

## File Reference Summary

| File | Purpose |
|------|---------|
| `include/platform/native_netplay.h` | Netplay API, structs, globals |
| `platform/native_netplay.c` | Core netcode (UDP, packets, peers) |
| `platform/native_platform.c` | Platform loop, calls `Netplay_Poll()` |
| `main.c` | Entry point, `--netplay` CLI args |
| `game/MAIN/MainMain.c` | Main game loop, input sync block |
| `game/MAIN/MainInit_06_Drivers.c` | Driver spawning (skips AI for netplay) |
| `game/MAIN/MainInit_07_FinalizeInit.c` | Camera/PushBuffer init, camera swap |
| `game/MAIN/MainFrame_08_RenderFrame.c` | Rendering, HUD, split-screen lines |
| `game/UI/UI_44_RenderFrame_Racing.c` | In-race HUD drawing |
| `game/230/230_22_MM_Online_Menu.c` | Online lobby menu UI/flow |
| `game/230/230_72_MM_JumpTo_Title_FirstTime.c` | Title screen init, clears g_NetplayRacing |
| `game/PushBuffer.c` | Viewport rect setup for 1P/2P/3P/4P |
| `include/namespace_Camera.h` | CameraDC struct definition |
| `include/namespace_Main.h` | GameTracker struct definition |
