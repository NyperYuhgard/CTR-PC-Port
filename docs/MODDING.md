# CTR Native — Modding Guide / Guía de Modding

> **English** | [Español](#ctr-native--guía-de-modding)

---

## Table of Contents / Índice

1. [Introduction](#introduction--introducción)
2. [Mod Structure / Estructura de un Mod](#mod-structure--estructura-de-un-mod)
3. [Lua API Reference / Referencia de la API Lua](#lua-api-reference--referencia-de-la-api-lua)
4. [Hook Types / Tipos de Hooks](#hook-types--tipos-de-hooks)
5. [File Overrides / Reemplazo de Archivos](#file-overrides--reemplazo-de-archivos)
6. [PS1 Coordinate System / Sistema de Coordenadas PS1](#ps1-coordinate-system--sistema-de-coordenadas-ps1)
7. [Examples / Ejemplos](#examples--ejemplos)
8. [Troubleshooting / Solución de Problemas](#troubleshooting--solución-de-problemas)

---

## Introduction / Introducción

**English:**

CTR Native supports Lua-based mods that can:

- Draw custom UI elements (text, rectangles) on screen
- Read and modify game memory (driver stats, game state, etc.)
- Hook into game events (init, update, render, input)
- Override game asset files (textures, models, levels, sounds, etc.)
- Create custom game logic and behavior modifications

Mods are written in Lua 5.4 and placed in the `mods/` directory next to the game executable.

**Español:**

CTR Native soporta mods basados en Lua que pueden:

- Dibujar elementos UI personalizados (texto, rectángulos) en pantalla
- Leer y modificar la memoria del juego (estadísticas del piloto, estado del juego, etc.)
- Interceptar eventos del juego (inicio, actualización, renderizado, input)
- Reemplazar archivos de assets del juego (texturas, modelos, niveles, sonidos, etc.)
- Crear lógica de juego personalizada y modificaciones de comportamiento

Los mods se escriben en Lua 5.4 y se colocan en el directorio `mods/` junto al ejecutable del juego.

---

## Mod Structure / Estructura de un Mod

**English:**

Each mod is a subdirectory inside `mods/`. The directory name becomes the mod's display name.

```
mods/
├── my_mod/
│   ├── main.lua          # Main script (required)
│   └── files/            # File overrides (optional)
│       └── lang/
│           └── en.lng
│            
├── hello_world/
│   └── main.lua
└── another_mod/
    ├── main.lua
    └── files/
        └── ...
```

- `main.lua` — Entry point, loaded automatically when the mod is enabled
- `files/` — Directory for asset overrides, mirrors the BIGFILE structure

**Español:**

Cada mod es un subdirectorio dentro de `mods/`. El nombre del directorio se convierte en el nombre visible del mod.

```
mods/
├── mi_mod/
│   ├── main.lua          # Script principal (requerido)
│   └── files/            # Reemplazo de archivos (opcional)
│       └── lang/
│           └── en.lng
│               
├── hello_world/
│   └── main.lua
└── otro_mod/
    ├── main.lua
    └── files/
        └── ...
```

- `main.lua` — Punto de entrada, se carga automáticamente cuando el mod está activado
- `files/` — Directorio para reemplazar assets, refleja la estructura de BIGFILE

---

## Lua API Reference / Referencia de la API Lua

**English:**

All mod functions are exposed through the global `mod` table. Here is the complete API:

**Español:**

Todas las funciones del mod están expuestas a través de la tabla global `mod`. Aquí está la API completa:

---

### `mod.log(message)`

**EN:** Print a message to the console. Prefixes with `[Mod]`.

**ES:** Imprime un mensaje en la consola. Prefijo `[Mod]`.

```lua
mod.log("Hello from my mod!")
-- Output: [Mod] Hello from my mod!
```

---

### `mod.hook(hookName, callback)`

**EN:** Register a callback function for a game event. Only ONE callback per hook type is supported (last registered wins).

**ES:** Registra una función callback para un evento del juego. Solo se admite UN callback por tipo de hook (el último registrado reemplaza al anterior).

```lua
mod.hook("onInit", function()
    mod.log("Mod initialized!")
end)
```

**Hook names available / Nombres de hooks disponibles:**

| Hook | When / Cuándo |
|------|---------------|
| `"onInit"` | After all mod scripts are loaded / Después de cargar todos los mods |
| `"onUpdate"` | Every frame during game logic / Cada frame durante la lógica del juego |
| `"onRender"` | Every frame before rendering / Cada frame antes del renderizado |
| `"onInput"` | Every frame after input processing / Cada frame después de procesar input |
| `"onTitleInit"` | When a new title/level is loaded / Cuando se carga un nuevo título/nivel |

---

### `mod.drawText(text, x, y, [fontType], [justify])`

**EN:** Queue text to be drawn on screen during the next render.

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `text` | string | — | Text to draw |
| `x` | int | — | X coordinate (PS1 space) |
| `y` | int | — | Y coordinate (PS1 space) |
| `fontType` | int | `2` | 1 = big font, 2 = small font |
| `justify` | int | `0` | 0 = left, 1 = center, 2 = right |

**ES:** Encola texto para dibujar en pantalla durante el próximo render.

| Parámetro | Tipo | Default | Descripción |
|-----------|------|---------|-------------|
| `text` | string | — | Texto a dibujar |
| `x` | int | — | Coordenada X (espacio PS1) |
| `y` | int | — | Coordenada Y (espacio PS1) |
| `fontType` | int | `2` | 1 = fuente grande, 2 = fuente pequeña |
| `justify` | int | `0` | 0 = izquierda, 1 = centro, 2 = derecha |

```lua
-- Draw centered "Hello World" with small font
mod.drawText("Hello World", 256, 108, 2, 1)

-- Draw left-aligned text with big font at top-left
mod.drawText("SCORE: 999", 10, 10, 1, 0)
```

---

### `mod.drawRect(x, y, w, h, r, g, b, [a])`

**EN:** Queue a filled rectangle to be drawn on screen.

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `x` | int | — | X coordinate (top-left) |
| `y` | int | — | Y coordinate (top-left) |
| `w` | int | — | Width |
| `h` | int | — | Height |
| `r` | int | — | Red (0–255) |
| `g` | int | — | Green (0–255) |
| `b` | int | — | Blue (0–255) |
| `a` | int | `255` | Alpha (0=transparent, 255=opaque) |

**ES:** Encoda un rectángulo relleno para dibujar en pantalla.

| Parámetro | Tipo | Default | Descripción |
|-----------|------|---------|-------------|
| `x` | int | — | Coordenada X (esquina superior izquierda) |
| `y` | int | — | Coordenada Y (esquina superior izquierda) |
| `w` | int | — | Ancho |
| `h` | int | — | Alto |
| `r` | int | — | Rojo (0–255) |
| `g` | int | — | Verde (0–255) |
| `b` | int | — | Azul (0–255) |
| `a` | int | `255` | Alfa (0=transparente, 255=opaco) |

```lua
-- Opaque red rectangle
mod.drawRect(100, 100, 50, 20, 255, 0, 0)

-- Semi-transparent green rectangle
mod.drawRect(100, 130, 50, 20, 0, 255, 0, 128)
```

---

### `mod.getDriver(playerIndex)`

**EN:** Get driver data for a player (0–7). Returns a table with these fields:

| Field | Type | Description |
|-------|------|-------------|
| `valid` | boolean | Whether the driver exists |
| `reserves` | int | Current boost reserves |
| `fireSpeedCap` | int | Current fire speed cap |
| `turbo_MeterRoomLeft` | int | Turbo meter room left |
| `turbo_outsideTimer` | int | Turbo outside timer |
| `numTurbos` | int | Number of turbos |
| `const_SacredFireSpeed` | int | Sacred Fire speed threshold |
| `const_SingleTurboSpeed` | int | Single Turbo speed threshold |
| `const_turboMaxRoom` | int | Turbo max room constant |
| `driverID` | int | Driver character ID |
| `actionsFlagSet` | int | Actions flag set |
| `speedApprox` | int | Approximate speed |

**ES:** Obtiene datos del piloto para un jugador (0–7). Devuelve una tabla con estos campos:

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `valid` | boolean | Si el piloto existe |
| `reserves` | int | Reservas de turbo actuales |
| `fireSpeedCap` | int | Límite de velocidad de fuego actual |
| `turbo_MeterRoomLeft` | int | Espacio restante del medidor de turbo |
| `turbo_outsideTimer` | int | Temporizador de turbo externo |
| `numTurbos` | int | Número de turbos |
| `const_SacredFireSpeed` | int | Umbral de velocidad de Sacred Fire |
| `const_SingleTurboSpeed` | int | Umbral de velocidad de Single Turbo |
| `const_turboMaxRoom` | int | Constante de espacio máximo de turbo |
| `driverID` | int | ID del personaje piloto |
| `actionsFlagSet` | int | Conjunto de banderas de acciones |
| `speedApprox` | int | Velocidad aproximada |

```lua
local driver = mod.getDriver(0)
if driver.valid then
    mod.log("Player 1 reserves: " .. driver.reserves)
    mod.log("Player 1 speed: " .. driver.speedApprox)
end
```

---

### `mod.setDriverField(playerIndex, fieldName, value)`

**EN:** Write a driver field by name. Returns `true` on success, `false` on failure.

**ES:** Escribe un campo del piloto por nombre. Devuelve `true` en éxito, `false` en error.

**Writable fields / Campos editables:**

| Field / Campo | Type / Tipo | Description / Descripción |
|---------------|-------------|---------------------------|
| `"reserves"` | int | Boost reserves / Reservas de turbo |
| `"fireSpeedCap"` | int | Fire speed cap / Límite de velocidad de fuego |
| `"turbo_MeterRoomLeft"` | int | Turbo meter room |
| `"turbo_outsideTimer"` | int | Turbo outside timer |
| `"numTurbos"` | int | Number of turbos / Número de turbos |
| `"kartState"` | int | Kart state / Estado del kart |
| `"actionsFlagSet"` | int | Actions flags / Banderas de acciones |
| `"speedApprox"` | int | Speed value / Velocidad |

```lua
-- Give player 1 infinite reserves
mod.setDriverField(0, "reserves", 9600)

-- Cap reserves at a specific value
if driver.reserves > 3000 then
    mod.setDriverField(0, "reserves", 3000)
end
```

---

### `mod.getNumPlayers()`

**EN:** Returns the number of active players.

**ES:** Devuelve el número de jugadores activos.

```lua
local count = mod.getNumPlayers()
mod.log("Players: " .. count)
```

---

### `mod.getGameMode()`

**EN:** Returns the current `gameMode1` bitfield value.

**ES:** Devuelve el valor del campo de bits `gameMode1` actual.

```lua
local mode = mod.getGameMode()
if mode & 0x1 ~= 0 then
    mod.log("Game is loading")
end
```

---

### `mod.getGameTracker()`

**EN:** Returns a lightuserdata pointer to the GameTracker struct. Use with `readS16`, `readU8`, `readS32`, `writeS16`, `writeU8`, `writeS32` for full memory access.

**ES:** Devuelve un puntero lightuserdata al struct GameTracker. Úsalo con `readS16`, `readU8`, `readS32`, `writeS16`, `writeU8`, `writeS32` para acceso completo a la memoria.

```lua
local gGT = mod.getGameTracker()
if gGT then
    -- Read gameMode1 at offset 0x000
    local mode = mod.readS32(gGT, 0x000)
    -- Read numPlyrCurrGame at offset 0x343
    local numPlayers = mod.readU8(gGT, 0x343)
    mod.log("Mode=" .. mode .. " Players=" .. numPlayers)
end
```

**Known GameTracker offsets / Offsets conocidos de GameTracker:**

| Offset | Type / Tipo | Field / Campo |
|--------|-------------|---------------|
| `0x000` | s32 | `gameMode1` |
| `0x343` | u8 | `numPlyrCurrGame` |
| `0x24EC` | Driver*[8] | `drivers[]` pointer array |

---

### Memory Read Functions / Funciones de Lectura de Memoria

```lua
mod.readS16(pointer, offset)   -- Read s16 from memory address + offset
mod.readU8(pointer, offset)    -- Read u8  from memory address + offset
mod.readS32(pointer, offset)   -- Read s32 from memory address + offset
```

**EN:** Read values from any memory address. `pointer` is a lightuserdata obtained from `getGameTracker()` or other sources. Returns the value read.

**ES:** Lee valores de cualquier dirección de memoria. `pointer` es un lightuserdata obtenido de `getGameTracker()` u otras fuentes. Devuelve el valor leído.

---

### Memory Write Functions / Funciones de Escritura de Memoria

```lua
mod.writeS16(pointer, offset, value)   -- Write s16 to memory address + offset
mod.writeU8(pointer, offset, value)    -- Write u8  to memory address + offset
mod.writeS32(pointer, offset, value)   -- Write s32 to memory address + offset
```

**EN:** Write values to any memory address. No return value.

**ES:** Escribe valores en cualquier dirección de memoria. Sin valor de retorno.

---

### `mod.getModPath()`

**EN:** Returns the filesystem path to the currently loading mod's directory. Useful for reading bundled files.

**ES:** Devuelve la ruta del sistema de archivos al directorio del mod que se está cargando. Útil para leer archivos incluidos.

```lua
local path = mod.getModPath()
mod.log("My mod is at: " .. path)
```

---

### `mod.readFile(relativePath)`

**EN:** Read a file from the mod's `files/` directory. Returns the file contents as a string, or `nil` on failure.

**ES:** Lee un archivo del directorio `files/` del mod. Devuelve el contenido como string, o `nil` en caso de error.

```lua
local data = mod.readFile("config.json")
if data then
    mod.log("Config: " .. data)
end
```

---

### `mod.writeFile(relativePath, data)`

**EN:** Write a file to the mod's `files/` directory. Returns `true` on success.

**ES:** Escribe un archivo en el directorio `files/` del mod. Devuelve `true` en éxito.

```lua
local ok = mod.writeFile("save.txt", "Hello from Lua!")
if ok then mod.log("Saved!") end
```

---

## Hook Types / Tipos de Hooks

**English:**

Hooks are called in this order each frame:

1. **`onInput`** — After gamepad input is processed. Good for reading button states.
2. **`onUpdate`** — During game logic update. Good for modifying game state (driver stats, etc.).
3. **`onRender`** — Before rendering. Call `mod.drawRect()` / `mod.drawText()` here to queue UI elements.
4. The draw queue is flushed automatically just before the GPU processes the ordering table.

During initialization:
- **`onInit`** — Called once after all mod scripts have been loaded.
- **`onTitleInit`** — Called when a new level/track/adventure area is loaded.

**Español:**

Los hooks se llaman en este orden cada frame:

1. **`onInput`** — Después de procesar el input de los gamepads. Bueno para leer estados de botones.
2. **`onUpdate`** — Durante la actualización de la lógica del juego. Bueno para modificar el estado del juego.
3. **`onRender`** — Antes del renderizado. Llama a `mod.drawRect()` / `mod.drawText()` aquí para encolar elementos UI.
4. La cola de dibujo se vacía automáticamente justo antes de que la GPU procese la tabla de ordenamiento.

Durante la inicialización:
- **`onInit`** — Se llama una vez después de cargar todos los scripts de los mods.
- **`onTitleInit`** — Se llama cuando se carga un nuevo nivel/pista/zona de aventura.

**Execution order example / Ejemplo de orden de ejecución:**

```lua
mod.hook("onInput", function()
    -- Called first each frame
end)

mod.hook("onUpdate", function()
    -- Called second each frame
    -- Driver data is cached fresh before this hook
    local d = mod.getDriver(0)
    if d.valid and d.reserves > 0 then
        mod.setDriverField(0, "reserves", 9600) -- infinite reserves
    end
end)

mod.hook("onRender", function()
    -- Called third each frame
    -- Queue drawing commands here
    mod.drawText("Hello", 256, 108, 2, 1)
end)
```

---

## File Overrides / Reemplazo de Archivos

**English:**

Mods can replace ANY game asset by placing files in their `files/` directory, mirroring the structure from `BIGFILE.TXT`. This works in all data modes (PACKED, UNPACKED, HYBRID).

**Lookup order / Orden de búsqueda:**

1. Each enabled mod's `files/` directory (checked in scan order)
2. The global `assets/BIGFILE/` unpacked folder
3. The `assets/` directory directly
4. The original BIGFILE.BIG archive (if using PACKED mode)

**How to override a file / Cómo reemplazar un archivo:**

```lua
-- Given a BIGFILE.TXT entry like:
--   levels/tracks/coco/1P/data.lev

-- Place the replacement at:
--   mods/my_mod/files/BIGFILE/levels/tracks/coco/1P/data.lev

-- OR (without BIGFILE/ prefix in the path):
--   mods/my_mod/files/levels/tracks/coco/1P/data.lev
```

The relative path from `BIGFILE.TXT` is used. If an entry says:

```
levels/tracks/coco/1P/data.lev
```

Then the mod override path would be:

```
mods/my_mod/files/levels/tracks/coco/1P/data.lev
```

**Language file override / Reemplazo de archivo de idioma:**

To override a language file, place it at:

```
mods/my_mod/files/lang/en.lng
```

The language system will pick up the override automatically when the mod is toggled on/off (via `NativeMods_ToggleMod()` which reloads the language file).

**Español:**

Los mods pueden reemplazar CUALQUIER asset del juego colocando archivos en su directorio `files/`, reflejando la estructura de `BIGFILE.TXT`. Esto funciona en todos los modos de datos (PACKED, UNPACKED, HYBRID).

**Orden de búsqueda:**

1. El directorio `files/` de cada mod activado (en orden de escaneo)
2. La carpeta global `assets/BIGFILE/` (unpacked)
3. El directorio `assets/` directamente
4. El archivo BIGFILE.BIG original (si se usa modo PACKED)

**Cómo reemplazar un archivo:**

```
Dada una entrada de BIGFILE.TXT como:
  levels/tracks/coco/1P/data.lev

Coloca el reemplazo en:
  mods/mi_mod/files/levels/tracks/coco/1P/data.lev

(Sin prefijo BIGFILE/ en la ruta)
```

La ruta relativa de `BIGFILE.TXT` es la que se usa. Si una entrada dice:

```
levels/tracks/coco/1P/data.lev
```

Entonces la ruta de override del mod sería:

```
mods/mi_mod/files/levels/tracks/coco/1P/data.lev
```

**Reemplazo de archivo de idioma:**

Para reemplazar un archivo de idioma, colócalo en:

```
mods/mi_mod/files/lang/en.lng
```

El sistema de idioma detectará el reemplazo automáticamente al activar/desactivar el mod.

---

## PS1 Coordinate System / Sistema de Coordenadas PS1

**English:**

The PS1 framebuffer resolution used by CTR is **512 × 216** pixels. All drawing coordinates use this space and are scaled to the window by the native renderer.

```lua
-- Screen boundaries / Límites de la pantalla
-- X: 0 to 511 (left to right)
-- Y: 0 to 215 (top to bottom)

-- Center of screen / Centro de la pantalla
-- X = 256 (0x100)
-- Y = 108 (0x6C)
```

**Useful positions / Posiciones útiles:**

| Position / Posición | X | Y |
|---------------------|---|---|
| Top-left / Superior izquierda | 0 | 0 |
| Top-center / Superior centro | 256 | 0 |
| Top-right / Superior derecha | 511 | 0 |
| Center / Centro | 256 | 108 |
| Bottom-left / Inferior izquierda | 0 | 215 |
| Bottom-center / Inferior centro | 256 | 215 |
| Bottom-right / Inferior derecha | 511 | 215 |

**Español:**

La resolución del framebuffer de PS1 usada por CTR es de **512 × 216** píxeles. Todas las coordenadas de dibujo usan este espacio y son escaladas a la ventana por el renderizador nativo.

```lua
-- Límites de la pantalla
-- X: 0 a 511 (izquierda a derecha)
-- Y: 0 a 215 (arriba a abajo)

-- Centro de la pantalla
-- X = 256 (0x100)
-- Y = 108 (0x6C)
```

---

## Examples / Ejemplos

### Example 1: Hello World

**EN:** A minimal mod that displays "Hello World" centered on screen.

**ES:** Un mod mínimo que muestra "Hello World" centrado en pantalla.

```lua
-- hello_world/main.lua

mod.hook("onInit", function()
    mod.log("Hello World mod loaded!")
end)

mod.hook("onRender", function()
    mod.drawText("Hello World", 256, 108, 2, 1)
end)
```

### Example 2: Player HUD Info

**EN:** Display player 1's turbo reserves and speed on screen.

**ES:** Muestra las reservas de turbo y velocidad del jugador 1 en pantalla.

```lua
mod.hook("onRender", function()
    local d = mod.getDriver(0)
    if d.valid then
        mod.drawText("Reserves: " .. d.reserves, 10, 10, 2, 0)
        mod.drawText("Speed: " .. d.speedApprox, 10, 22, 2, 0)
    end
end)
```

### Example 3: Infinite Reserves Toggle

**EN:** Keep reserves maxed out while the mod is active.

**ES:** Mantiene las reservas al máximo mientras el mod está activo.

```lua
mod.hook("onUpdate", function()
    local d = mod.getDriver(0)
    if d.valid and d.reserves > 0 then
        mod.setDriverField(0, "reserves", 9600)
    end
end)
```

### Example 4: Draw a Speed Bar

**EN:** Draw a colored bar showing the player's speed.

**ES:** Dibuja una barra de colores mostrando la velocidad del jugador.

```lua
local BAR_X = 200
local BAR_Y = 10
local BAR_W = 100
local BAR_H = 8

mod.hook("onRender", function()
    local d = mod.getDriver(0)
    if not d.valid then return end

    -- Background
    mod.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, 32, 32, 32)

    -- Speed fill (clamped to bar width)
    local fill = math.min(d.speedApprox / 20000 * BAR_W, BAR_W)
    if fill > 0 then
        local r = 0
        local g = 255
        if d.fireSpeedCap > d.const_SingleTurboSpeed then
            r, g = 255, 255  -- Yellow = Sacred Fire
        end
        if d.fireSpeedCap > d.const_SacredFireSpeed then
            r, g = 255, 64   -- Orange = Ultra Sacred Fire
        end
        mod.drawRect(BAR_X, BAR_Y, math.floor(fill), BAR_H, r, g, 0)
    end
end)
```

### Example 5: File Override + Custom Level

**EN:** Replace a track's data.lev with a custom one.

**ES:** Reemplaza el data.lev de una pista con uno personalizado.

```
mods/my_custom_track/
├── main.lua
└── files/
    └── levels/
        └── tracks/
            └── coco/
                └── 1P/
                    ├── data.lev     -- Custom level data
                    └── data.vrm     -- Custom textures (optional)
```

```lua
-- main.lua
mod.hook("onInit", function()
    mod.log("Custom Track mod loaded!")
end)
```

### Example 6: Raw Memory Access via GameTracker

**EN:** Read and write arbitrary game memory using the GameTracker pointer.

**ES:** Lee y escribe memoria arbitraria del juego usando el puntero GameTracker.

```lua
mod.hook("onUpdate", function()
    local gGT = mod.getGameTracker()
    if not gGT then return end

    -- Read game mode
    local mode = mod.readS32(gGT, 0x000)

    -- Read number of players
    local numPlyr = mod.readU8(gGT, 0x343)

    -- Read driver pointer array (offset 0x24EC)
    -- Each entry is 4 bytes on 32-bit
    local driver0ptr = mod.readS32(gGT, 0x24EC)

    -- Write: set gameMode1 bit 0 (loading flag)
    mod.writeS32(gGT, 0x000, mode | 0x1)

    mod.log("Mode=" .. mode .. " Players=" .. numPlyr)
end)
```

---

## Troubleshooting / Solución de Problemas

### English

**Mod not showing in the list:**
- Make sure the mod directory is inside `mods/` next to the game executable
- Verify `main.lua` exists and is readable
- Check the console output for `[Mods]` error messages

**Draw calls not appearing on screen:**
- Make sure `mod.drawText()` / `mod.drawRect()` are called inside an `onRender` hook
- Verify coordinates are within the PS1 framebuffer (512×216)
- Check that the draw queue isn't overflowing (max 256 commands per frame)

**Lua errors:**
- Errors are caught and logged with `[Mods] Lua error in <mod>: <message>`
- Check the console/log output for details

**File overrides not working:**
- Verify the file path matches the path in `BIGFILE.TXT` exactly
- Check that the mod is enabled in the state file
- Ensure the file is in `mods/<name>/files/<path>`, not in `mods/<name>/<path>`

### Español

**El mod no aparece en la lista:**
- Asegúrate de que el directorio del mod esté dentro de `mods/` junto al ejecutable
- Verifica que `main.lua` exista y sea legible
- Revisa la salida de la consola en busca de mensajes de error `[Mods]`

**Las llamadas de dibujo no aparecen en pantalla:**
- Asegúrate de que `mod.drawText()` / `mod.drawRect()` se llamen dentro de un hook `onRender`
- Verifica que las coordenadas estén dentro del framebuffer PS1 (512×216)
- Revisa que la cola de dibujo no se desborde (máximo 256 comandos por frame)

**Errores de Lua:**
- Los errores se capturan y registran con `[Mods] Lua error in <mod>: <message>`
- Revisa la salida de la consola para más detalles

**Los reemplazos de archivos no funcionan:**
- Verifica que la ruta del archivo coincida exactamente con la ruta en `BIGFILE.TXT`
- Revisa que el mod esté activado en el archivo de estado
- Asegúrate de que el archivo esté en `mods/<nombre>/files/<ruta>`, no en `mods/<nombre>/<ruta>`

---

## Tips & Best Practices / Consejos y Buenas Prácticas

### English

- **Performance:** Keep `onUpdate` and `onRender` hooks lightweight. Heavy Lua computation can slow down the game.
- **Drawing:** Queue all draw commands inside `onRender`. Draw commands are flushed automatically before GPU submission.
- **State persistence:** Mod enabled/disabled state is saved to `mods_state.cfg` automatically on game exit.
- **Memory safety:** Use `setDriverField()` instead of raw memory writes when possible — it validates the field name and handles alignment.
- **Debugging:** Use `mod.log()` liberally to debug your mod. Output appears in the console with `[Mod]` prefix.
- **File paths:** Always use forward slashes (`/`) in file paths, even on Windows.

### Español

- **Rendimiento:** Mantén los hooks `onUpdate` y `onRender` ligeros. Cómputos pesados en Lua pueden ralentizar el juego.
- **Dibujo:** Encola todos los comandos de dibujo dentro de `onRender`. Los comandos se vacían automáticamente antes del envío a la GPU.
- **Persistencia de estado:** El estado activado/desactivado de los mods se guarda en `mods_state.cfg` automáticamente al salir del juego.
- **Seguridad de memoria:** Usa `setDriverField()` en lugar de escrituras de memoria directas cuando sea posible — valida el nombre del campo y maneja la alineación.
- **Depuración:** Usa `mod.log()` liberalmente para depurar tu mod. La salida aparece en la consola con el prefijo `[Mod]`.
- **Rutas de archivo:** Usa siempre barras diagonales (`/`) en las rutas de archivo, incluso en Windows.
