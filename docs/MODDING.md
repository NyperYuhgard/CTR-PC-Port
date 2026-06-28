# CTR Native — Modding Guide / Guía de Modding

> **English** | [Español](#ctr-native--guía-de-modding-1)

---

## Table of Contents / Índice

1. [Introduction / Introducción](#introduction--introducción)
2. [Getting Started / Primeros Pasos](#getting-started--primeros-pasos)
3. [Mod Structure / Estructura de un Mod](#mod-structure--estructura-de-un-mod)
4. [Lua API Reference / Referencia de la API Lua](#lua-api-reference--referencia-de-la-api-lua)
    - [Logging & Utilities](#logging--utilities)
    - [File I/O](#file-io)
    - [Hooks (Events)](#hooks-events)
    - [Driver Data (Read Only)](#driver-data-read-only)
    - [Driver Data (Write)](#driver-data-write)
    - [Game State Access](#game-state-access)
    - [Memory Read/Write](#memory-readwrite)
    - [Drawing](#drawing)
5. [Hook Execution Order / Orden de Ejecución de Hooks](#hook-execution-order--orden-de-ejecución-de-hooks)
6. [File Override System / Sistema de Reemplazo de Archivos](#file-override-system--sistema-de-reemplazo-de-archivos)
7. [PS1 Coordinate System / Sistema de Coordenadas PS1](#ps1-coordinate-system--sistema-de-coordenadas-ps1)
8. [Internal Draw Queue / Cola de Dibujo Interna](#internal-draw-queue--cola-de-dibujo-interna)
9. [Mod Persistence / Persistencia de Mods](#mod-persistence--persistencia-de-mods)
10. [Available Lua Libraries / Librerías Lua Disponibles](#available-lua-libraries--librerías-lua-disponibles)
11. [Examples / Ejemplos](#examples--ejemplos)
12. [Troubleshooting / Solución de Problemas](#troubleshooting--solución-de-problemas)
13. [Tips & Best Practices / Consejos y Buenas Prácticas](#tips--best-practices--consejos-y-buenas-prácticas)

---

## Introduction / Introducción

**English:**

CTR Native supports Lua 5.4 mods that can:

- Draw custom UI elements (text, filled rectangles) on screen
- Read and modify game memory (driver stats, game state, etc.)
- Hook into game events (init, update, render, input)
- Override game asset files (textures, models, levels, sounds, etc.)
- Create custom game logic and behavior modifications
- Persist and read mod-specific data files

Mods are written in Lua 5.4 and placed in the `mods/` directory next to the game executable. All mods share a single Lua VM — be careful with global variable names to avoid conflicts between mods.

**Español:**

CTR Native soporta mods basados en Lua 5.4 que pueden:

- Dibujar elementos UI personalizados (texto, rectángulos rellenos) en pantalla
- Leer y modificar la memoria del juego (estadísticas del piloto, estado del juego, etc.)
- Interceptar eventos del juego (inicio, actualización, renderizado, input)
- Reemplazar archivos de assets del juego (texturas, modelos, niveles, sonidos, etc.)
- Crear lógica de juego personalizada y modificaciones de comportamiento
- Persistir y leer archivos de datos propios del mod

Los mods se escriben en Lua 5.4 y se colocan en el directorio `mods/` junto al ejecutable del juego. Todos los mods comparten una sola máquina virtual de Lua — ten cuidado con los nombres de variables globales para evitar conflictos entre mods.

---

## Getting Started / Primeros Pasos

**English:**

1. Create a new folder inside the `mods/` directory (e.g., `mods/my_first_mod/`)
2. Create a `main.lua` file inside it
3. Write your Lua code using the `mod` API functions
4. Launch the game — your mod will be detected automatically
5. Open the **MODS** menu (Options → Mods) to toggle mods on/off

The directory name becomes the mod's display name in the in-game menu.

**Español:**

1. Crea una nueva carpeta dentro del directorio `mods/` (ej: `mods/mi_primer_mod/`)
2. Crea un archivo `main.lua` dentro de ella
3. Escribe tu código Lua usando las funciones de la API `mod`
4. Inicia el juego — tu mod se detectará automáticamente
5. Abre el menú **MODS** (Options → Mods) para activar/desactivar mods

El nombre del directorio se convierte en el nombre visible del mod en el menú del juego.

---

## Mod Structure / Estructura de un Mod

**English:**

Each mod is a subdirectory inside `mods/`. The directory name IS the mod's display name.

```
mods/
├── my_mod/
│   ├── main.lua          # Main script (REQUIRED)
│   └── files/            # File overrides (optional)
│       ├── lang/
│       │   └── en.lng
│       ├── levels/
│       │   └── tracks/
│       │       └── coco/
│       │           └── 1P/
│       │               └── data.lev
│       └── config.txt
│
├── hello_world/
│   └── main.lua
│
└── another_mod/
    ├── main.lua
    └── files/
        └── ...
```

| File / Folder | Required | Description |
|---------------|----------|-------------|
| `main.lua` | **Yes** | Entry point, loaded when the mod is enabled |
| `files/` | No | Directory for asset overrides, mirrors BIGFILE structure |
| Other `.lua` files | No | Use `require` or `dofile` to load additional scripts from `main.lua` |

**Mod limits:**
- Maximum **64 mods** can be registered simultaneously
- Maximum **256 draw commands** per frame across all mods
- Only **1 callback per hook type** per mod (last registered wins)

**Español:**

Cada mod es un subdirectorio dentro de `mods/`. El nombre del directorio ES el nombre visible del mod.

```
mods/
├── mi_mod/
│   ├── main.lua          # Script principal (REQUERIDO)
│   └── files/            # Reemplazo de archivos (opcional)
│       ├── lang/
│       │   └── es.lng
│       ├── levels/
│       │   └── tracks/
│       │       └── coco/
│       │           └── 1P/
│       │               └── data.lev
│       └── config.txt
│
├── hello_world/
│   └── main.lua
│
└── otro_mod/
    ├── main.lua
    └── files/
        └── ...
```

| Archivo / Carpeta | Requerido | Descripción |
|-------------------|-----------|-------------|
| `main.lua` | **Sí** | Punto de entrada, se carga al activar el mod |
| `files/` | No | Directorio para reemplazar assets, refleja la estructura de BIGFILE |
| Otros `.lua` | No | Usa `require` o `dofile` para cargar scripts adicionales desde `main.lua` |

**Límites de mods:**
- Máximo **64 mods** pueden registrarse simultáneamente
- Máximo **256 comandos de dibujo** por frame entre todos los mods
- Solo **1 callback por tipo de hook** por mod (el último registrado reemplaza)

---

## Lua API Reference / Referencia de la API Lua

**English:**

All mod functions are exposed through the global `mod` table. The Lua standard libraries (`math`, `string`, `table`, `os`, `io`, etc.) are also available.

> **Note:** File I/O functions (`io.open`, `os.execute`) are restricted to the sandbox. Use `mod.readFile()` and `mod.writeFile()` for safe file access within your mod's `files/` directory.

**Español:**

Todas las funciones del mod están expuestas a través de la tabla global `mod`. Las librerías estándar de Lua (`math`, `string`, `table`, `os`, `io`, etc.) también están disponibles.

> **Nota:** Las funciones de E/S de archivos (`io.open`, `os.execute`) están restringidas a la caja de arena. Usa `mod.readFile()` y `mod.writeFile()` para acceso seguro a archivos dentro del directorio `files/` de tu mod.

---

### Logging & Utilities

---

#### `mod.log(message)`

**EN:** Print a message to the console. Prefixes with `[Mod]`. Useful for debugging.

| Param | Type | Description |
|-------|------|-------------|
| `message` | string | The text to print |

**Returns:** nothing

**ES:** Imprime un mensaje en la consola. Prefijo `[Mod]`. Útil para depuración.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `message` | string | El texto a imprimir |

**Retorna:** nada

```lua
mod.log("Hello from my mod!")
-- Output: [Mod] Hello from my mod!
```

---

#### `mod.getModPath()`

**EN:** Returns the absolute filesystem path to the currently loading mod's directory. Useful for reading bundled files with Lua's standard `io` library.

**Returns:** string — the mod directory path, or `nil` if called outside a mod loading context

**ES:** Devuelve la ruta absoluta del sistema de archivos al directorio del mod que se está cargando. Útil para leer archivos incluidos con la librería estándar `io` de Lua.

**Retorna:** string — la ruta del directorio del mod, o `nil` si se llama fuera del contexto de carga de un mod

```lua
local path = mod.getModPath()
mod.log("My mod is at: " .. path)
-- Output: [Mod] My mod is at: /path/to/game/mods/my_mod/
```

---

### File I/O

---

#### `mod.readFile(relativePath)`

**EN:** Read a file from the mod's `files/` directory. The path is relative to your mod's `files/` folder.

| Param | Type | Description |
|-------|------|-------------|
| `relativePath` | string | Path relative to `files/` (use `/` separators) |

**Returns:** string — full file contents on success, or `nil` on failure (file not found, read error)

**ES:** Lee un archivo del directorio `files/` del mod. La ruta es relativa a la carpeta `files/` de tu mod.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `relativePath` | string | Ruta relativa a `files/` (usa separadores `/`) |

**Retorna:** string — contenido completo del archivo en éxito, o `nil` en error (archivo no encontrado, error de lectura)

```lua
local data = mod.readFile("config.json")
if data then
    mod.log("Config loaded: " .. data)
else
    mod.log("Could not read config.json")
end

-- Read from subdirectories:
local img = mod.readFile("images/title.png")
```

---

#### `mod.writeFile(relativePath, data)`

**EN:** Write data to a file inside the mod's `files/` directory. Creates subdirectories as needed.

| Param | Type | Description |
|-------|------|-------------|
| `relativePath` | string | Path relative to `files/` (use `/` separators) |
| `data` | string | Data to write |

**Returns:** boolean — `true` on success, `false` on failure

**ES:** Escribe datos en un archivo dentro del directorio `files/` del mod. Crea subdirectorios si es necesario.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `relativePath` | string | Ruta relativa a `files/` (usa separadores `/`) |
| `data` | string | Datos a escribir |

**Retorna:** boolean — `true` en éxito, `false` en error

```lua
-- Save player progress
local ok = mod.writeFile("save.txt", "Level 3 completed!")
if ok then
    mod.log("Progress saved!")
end

-- Write JSON-style data
local scoreData = "highscore=" .. highscore
mod.writeFile("scores.dat", scoreData)
```

---

### Hooks (Events)

---

#### `mod.hook(hookName, callback)`

**EN:** Register a callback function for a game event. Only ONE callback per hook type is supported per mod (the last registered replaces any previous one for that hook).

| Param | Type | Description |
|-------|------|-------------|
| `hookName` | string | One of: `"onInit"`, `"onUpdate"`, `"onRender"`, `"onInput"`, `"onTitleInit"` |
| `callback` | function | Function to call when the hook fires |

**Returns:** nothing

**ES:** Registra una función callback para un evento del juego. Solo se admite UN callback por tipo de hook por mod (el último registrado reemplaza al anterior para ese hook).

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `hookName` | string | Uno de: `"onInit"`, `"onUpdate"`, `"onRender"`, `"onInput"`, `"onTitleInit"` |
| `callback` | function | Función a llamar cuando se activa el hook |

**Retorna:** nada

```lua
mod.hook("onInit", function()
    mod.log("Mod initialized!")
end)
```

**Available hooks / Hooks disponibles:**

| Hook Name | Called When / Cuándo se llama | Best For / Ideal para |
|-----------|-------------------------------|----------------------|
| `"onInit"` | Once after ALL mod scripts are loaded | Initialization, loading config files |
| `"onUpdate"` | Every frame during game logic update | Modifying game state, driver stats |
| `"onRender"` | Every frame before rendering | Drawing UI elements (text, rects) |
| `"onInput"` | Every frame after input processing | Reading button states |
| `"onTitleInit"` | When a new level/track/adventure area loads | Per-track setup, resetting state |

> **Note:** Hooks receive no arguments. Use `mod.getDriver()`, `mod.getGameTracker()`, etc. inside callbacks to read game state.

> **Nota:** Los hooks no reciben argumentos. Usa `mod.getDriver()`, `mod.getGameTracker()`, etc. dentro de los callbacks para leer el estado del juego.

---

### Driver Data (Read Only)

---

#### `mod.getDriver(playerIndex)`

**EN:** Get a table of driver data for a player. The data is cached before hooks run, so it's consistent throughout a single frame.

| Param | Type | Description |
|-------|------|-------------|
| `playerIndex` | int | Player slot (0–7) |

**Returns:** table — driver data with the fields below, or a table with `valid = false` if the slot is empty.

**ES:** Obtiene una tabla con los datos del piloto para un jugador. Los datos se cachean antes de ejecutar los hooks, por lo que son consistentes durante todo el frame.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `playerIndex` | int | Espacio del jugador (0–7) |

**Retorna:** table — datos del piloto con los campos siguientes, o una tabla con `valid = false` si el espacio está vacío.

**Returned fields / Campos retornados:**

| Field | Type | Read/Write | Description / Descripción |
|-------|------|------------|--------------------------|
| `valid` | boolean | R | Whether the driver exists / Si el piloto existe |
| `reserves` | int (s16) | RW | Current boost reserves (0–9600) / Reservas de turbo actuales |
| `fireSpeedCap` | int (s16) | RW | Current fire speed cap / Límite de velocidad de fuego actual |
| `turbo_MeterRoomLeft` | int (s16) | RW | Turbo meter room left / Espacio restante del medidor de turbo |
| `turbo_outsideTimer` | int (s16) | RW | Turbo outside timer / Temporizador de turbo externo |
| `numTurbos` | int (u8) | RW | Number of turbos collected / Número de turbos recolectados |
| `const_SacredFireSpeed` | int (s16) | R | Sacred Fire speed threshold (read-only constant) / Umbral de velocidad de Sacred Fire |
| `const_SingleTurboSpeed` | int (s16) | R | Single Turbo speed threshold (read-only constant) / Umbral de velocidad de Single Turbo |
| `const_turboMaxRoom` | int (u8) | R | Turbo max room constant (read-only) / Constante de espacio máximo de turbo |
| `driverID` | int (u8) | R | Driver character ID (0 = Crash, 1 = Cortex, etc.) / ID del personaje piloto |
| `actionsFlagSet` | int (s16) | RW | Actions flag bitfield / Conjunto de banderas de acciones |
| `speedApprox` | int (s16) | RW | Approximate speed value / Velocidad aproximada |

> `(s16)` = signed 16-bit integer (range: -32768 to 32767) / entero con signo de 16 bits
> `(u8)` = unsigned 8-bit integer (range: 0 to 255) / entero sin signo de 8 bits

```lua
local d = mod.getDriver(0)
if d.valid then
    mod.log("Player 1 reserves: " .. d.reserves)
    mod.log("Player 1 speed: " .. d.speedApprox)
    mod.log("Driver ID: " .. d.driverID)
    
    -- Compare against speed thresholds
    if d.speedApprox > d.const_SacredFireSpeed then
        mod.log("Ultra Sacred Fire active!")
    elseif d.speedApprox > d.const_SingleTurboSpeed then
        mod.log("Sacred Fire active!")
    end
end
```

---

### Driver Data (Write)

---

#### `mod.setDriverField(playerIndex, fieldName, value)`

**EN:** Write a driver field by name. Safer than raw memory writes — validates the field name and handles data alignment automatically.

| Param | Type | Description |
|-------|------|-------------|
| `playerIndex` | int | Player slot (0–7) |
| `fieldName` | string | Field name (see table below) |
| `value` | int | Value to write |

**Returns:** boolean — `true` on success, `false` if the player slot is empty

**ES:** Escribe un campo del piloto por nombre. Más seguro que escribir memoria directamente — valida el nombre del campo y maneja la alineación de datos automáticamente.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `playerIndex` | int | Espacio del jugador (0–7) |
| `fieldName` | string | Nombre del campo (ver tabla abajo) |
| `value` | int | Valor a escribir |

**Retorna:** boolean — `true` en éxito, `false` si el espacio del jugador está vacío

**Writable fields / Campos editables:**

| Field Name | Type | Range | Description / Descripción |
|------------|------|-------|--------------------------|
| `"reserves"` | s16 | 0–9600 | Boost reserves / Reservas de turbo |
| `"fireSpeedCap"` | s16 | varies | Fire speed cap / Límite de velocidad de fuego |
| `"turbo_MeterRoomLeft"` | s16 | varies | Turbo meter room / Espacio del medidor de turbo |
| `"turbo_outsideTimer"` | s16 | varies | Turbo outside timer / Temporizador de turbo externo |
| `"numTurbos"` | u8 | 0–10 | Number of turbos / Número de turbos |
| `"kartState"` | u8 | 0–255 | Kart state (bitfield) / Estado del kart (campo de bits) |
| `"actionsFlagSet"` | s16 | varies | Actions flag bitfield / Banderas de acciones |
| `"speedApprox"` | s16 | varies | Speed value / Velocidad |

On error (invalid field name), a Lua error is raised with the list of valid field names.

En error (nombre de campo inválido), se lanza un error de Lua con la lista de campos válidos.

```lua
-- Give player 1 infinite reserves
mod.setDriverField(0, "reserves", 9600)

-- Set speed to a specific value
mod.setDriverField(0, "speedApprox", 15000)

-- OnUpdate: cap reserves to prevent abuse
mod.hook("onUpdate", function()
    local d = mod.getDriver(0)
    if d.valid and d.reserves > 3000 then
        mod.setDriverField(0, "reserves", 3000)
    end
end)
```

---

### Game State Access

---

#### `mod.getNumPlayers()`

**EN:** Returns the number of active players in the current game session. Includes all human-controlled players.

**Returns:** int — number of active players (0–8)

**ES:** Devuelve el número de jugadores activos en la sesión actual. Incluye todos los jugadores controlados por humanos.

**Retorna:** int — número de jugadores activos (0–8)

```lua
local count = mod.getNumPlayers()
mod.log("Players in this game: " .. count)

-- Iterate all players
for i = 0, count - 1 do
    local d = mod.getDriver(i)
    if d.valid then
        mod.log("Player " .. (i+1) .. " has " .. d.reserves .. " reserves")
    end
end
```

---

#### `mod.getGameMode()`

**EN:** Returns the current `gameMode1` bitfield value. This encodes the current game state (loading, racing, menu, etc.) as a set of bit flags.

**Returns:** int — `gameMode1` bitfield

**ES:** Devuelve el valor del campo de bits `gameMode1` actual. Esto codifica el estado actual del juego (cargando, corriendo, menú, etc.) como un conjunto de banderas de bits.

**Retorna:** int — campo de bits `gameMode1`

**Common bit values / Valores de bits comunes:**

| Bit | Hex | Meaning / Significado |
|-----|-----|----------------------|
| 0 | `0x01` | Loading / Cargando |
| 1 | `0x02` | Racing / Corriendo |
| 4 | `0x10` | Paused / Pausado |
| 5 | `0x20` | Race over / Carrera terminada |
| 8 | `0x100` | Menu / Menú |
| 16 | `0x10000` | Demo mode / Modo demo |

```lua
local mode = mod.getGameMode()
if mode & 0x01 ~= 0 then
    mod.log("Game is loading")
end
if mode & 0x02 ~= 0 then
    mod.log("Race is active!")
end
if mode & 0x10 ~= 0 then
    mod.log("Game is paused")
end
```

---

#### `mod.getGameTracker()`

**EN:** Returns a lightuserdata pointer to the global GameTracker struct (`sdata->gGT`). Use with `readS16`, `readU8`, `readS32`, `writeS16`, `writeU8`, `writeS32` for full raw memory access beyond the driver struct.

**Returns:** lightuserdata — pointer to GameTracker, or `nil` if unavailable

**ES:** Devuelve un puntero lightuserdata al struct global GameTracker (`sdata->gGT`). Úsalo con `readS16`, `readU8`, `readS32`, `writeS16`, `writeU8`, `writeS32` para acceso completo a memoria sin procesar más allá del struct del piloto.

**Retorna:** lightuserdata — puntero a GameTracker, o `nil` si no está disponible

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

**Known GameTracker offsets (CTR NTSC-U 926) / Offsets conocidos de GameTracker:**

| Offset | Type | Field | Description / Descripción |
|--------|------|-------|--------------------------|
| `0x000` | s32 | `gameMode1` | Game state bitfield |
| `0x004` | s32 | `gameMode2` | Secondary game state |
| `0x034` | s32 | `levelID` | Current level ID |
| `0x04C` | s32 | `timer` | Frame timer (increments each frame) |
| `0x054` | s32 | `framesInThisLEV` | Frames spent in current level |
| `0x058` | s32 | `elapsedTimeMS` | Elapsed time in milliseconds |
| `0x0AC` | s32 | `boolDemoMode` | Demo mode flag |
| `0x0B0` | s32 | `mainGameState` | Main game state enum |
| `0x343` | u8 | `numPlyrCurrGame` | Number of active players |
| `0x24EC` | s32[8] | `drivers[]` | Pointer array to driver structs (8 entries, 4 bytes each) |

> **Warning:** Offsets may differ between game versions (NTSC-U, PAL, NTSC-J). The values above are for NTSC-U 926.

> **Advertencia:** Los offsets pueden diferir entre versiones del juego (NTSC-U, PAL, NTSC-J). Los valores anteriores son para NTSC-U 926.

---

### Memory Read/Write

---

**EN:** These functions provide raw memory access for advanced modding. Combine with `mod.getGameTracker()` or any other obtained pointer.

**ES:** Estas funciones proporcionan acceso a memoria sin procesar para modding avanzado. Combínalas con `mod.getGameTracker()` o cualquier otro puntero obtenido.

---

#### `mod.readS16(pointer, offset)`

**EN:** Read a signed 16-bit integer from `pointer + offset`.

| Param | Type | Description |
|-------|------|-------------|
| `pointer` | lightuserdata | Base memory address |
| `offset` | int | Byte offset from the base address |

**Returns:** int — the signed 16-bit value read

**ES:** Lee un entero con signo de 16 bits desde `puntero + offset`.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `pointer` | lightuserdata | Dirección de memoria base |
| `offset` | int | Desplazamiento en bytes desde la dirección base |

**Retorna:** int — el valor de 16 bits con signo leído

---

#### `mod.readU8(pointer, offset)`

**EN:** Read an unsigned 8-bit integer from `pointer + offset`.

| Param | Type | Description |
|-------|------|-------------|
| `pointer` | lightuserdata | Base memory address |
| `offset` | int | Byte offset from the base address |

**Returns:** int — the unsigned 8-bit value read (0–255)

**ES:** Lee un entero sin signo de 8 bits desde `puntero + offset`.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `pointer` | lightuserdata | Dirección de memoria base |
| `offset` | int | Desplazamiento en bytes desde la dirección base |

**Retorna:** int — el valor de 8 bits sin signo leído (0–255)

---

#### `mod.readS32(pointer, offset)`

**EN:** Read a signed 32-bit integer from `pointer + offset`.

| Param | Type | Description |
|-------|------|-------------|
| `pointer` | lightuserdata | Base memory address |
| `offset` | int | Byte offset from the base address |

**Returns:** int — the signed 32-bit value read

**ES:** Lee un entero con signo de 32 bits desde `puntero + offset`.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `pointer` | lightuserdata | Dirección de memoria base |
| `offset` | int | Desplazamiento en bytes desde la dirección base |

**Retorna:** int — el valor de 32 bits con signo leído

---

#### `mod.writeS16(pointer, offset, value)`

**EN:** Write a signed 16-bit integer to `pointer + offset`.

| Param | Type | Description |
|-------|------|-------------|
| `pointer` | lightuserdata | Base memory address |
| `offset` | int | Byte offset from the base address |
| `value` | int | Value to write (clamped to s16 range) |

**Returns:** nothing

**ES:** Escribe un entero con signo de 16 bits en `puntero + offset`.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `pointer` | lightuserdata | Dirección de memoria base |
| `offset` | int | Desplazamiento en bytes desde la dirección base |
| `value` | int | Valor a escribir (limitado al rango s16) |

**Retorna:** nada

---

#### `mod.writeU8(pointer, offset, value)`

**EN:** Write an unsigned 8-bit integer to `pointer + offset`.

| Param | Type | Description |
|-------|------|-------------|
| `pointer` | lightuserdata | Base memory address |
| `offset` | int | Byte offset from the base address |
| `value` | int | Value to write (clamped to u8 range) |

**Returns:** nothing

**ES:** Escribe un entero sin signo de 8 bits en `puntero + offset`.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `pointer` | lightuserdata | Dirección de memoria base |
| `offset` | int | Desplazamiento en bytes desde la dirección base |
| `value` | int | Valor a escribir (limitado al rango u8) |

**Retorna:** nada

---

#### `mod.writeS32(pointer, offset, value)`

**EN:** Write a signed 32-bit integer to `pointer + offset`.

| Param | Type | Description |
|-------|------|-------------|
| `pointer` | lightuserdata | Base memory address |
| `offset` | int | Byte offset from the base address |
| `value` | int | Value to write (clamped to s32 range) |

**Returns:** nothing

**ES:** Escribe un entero con signo de 32 bits en `puntero + offset`.

| Parámetro | Tipo | Descripción |
|-----------|------|-------------|
| `pointer` | lightuserdata | Dirección de memoria base |
| `offset` | int | Desplazamiento en bytes desde la dirección base |
| `value` | int | Valor a escribir (limitado al rango s32) |

**Retorna:** nada

```lua
-- Example: Reading driver pointer and modifying a field via raw memory
local gGT = mod.getGameTracker()
if gGT then
    -- Read pointer to driver 0 (offset 0x24EC in GameTracker)
    local driver0ptr = mod.readS32(gGT, 0x24EC)
    if driver0ptr ~= 0 then
        -- Read driver's reserves (offset 0x000 in driver struct)
        local reserves = mod.readS16(driver0ptr, 0x000)
        mod.log("Driver 0 reserves (raw): " .. reserves)
        
        -- Write new speed value (offset 0x006 in driver struct)
        mod.writeS16(driver0ptr, 0x006, 20000)
    end
end
```

---

### Drawing

---

#### `mod.drawRect(x, y, w, h, r, g, b, [a])`

**EN:** Queue a filled rectangle to be drawn on screen. The command is executed during the render flush, not immediately.

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `x` | int | — | X coordinate (top-left, PS1 512x216 space) |
| `y` | int | — | Y coordinate (top-left, PS1 512x216 space) |
| `w` | int | — | Width in pixels |
| `h` | int | — | Height in pixels |
| `r` | int | — | Red color component (0–255) |
| `g` | int | — | Green color component (0–255) |
| `b` | int | — | Blue color component (0–255) |
| `a` | int | `255` | Alpha (0 = transparent, 255 = opaque) |

**Returns:** nothing

**Notes:**
- Coordinates use the PS1 framebuffer space (512×216)
- The maximum draw queue size is 256 commands per frame
- If the queue overflows, the command is silently dropped and a warning is logged

**ES:** Encola un rectángulo relleno para dibujar en pantalla. El comando se ejecuta durante el vaciado de renderizado, no inmediatamente.

| Parámetro | Tipo | Default | Descripción |
|-----------|------|---------|-------------|
| `x` | int | — | Coordenada X (esquina superior izquierda, espacio PS1 512x216) |
| `y` | int | — | Coordenada Y (esquina superior izquierda, espacio PS1 512x216) |
| `w` | int | — | Ancho en píxeles |
| `h` | int | — | Alto en píxeles |
| `r` | int | — | Componente rojo (0–255) |
| `g` | int | — | Componente verde (0–255) |
| `b` | int | — | Componente azul (0–255) |
| `a` | int | `255` | Alfa (0 = transparente, 255 = opaco) |

**Retorna:** nada

**Notas:**
- Las coordenadas usan el espacio del framebuffer PS1 (512×216)
- El tamaño máximo de la cola de dibujo es 256 comandos por frame
- Si la cola se desborda, el comando se descarta silenciosamente y se registra una advertencia

```lua
-- Opaque red rectangle (top-left corner)
mod.drawRect(10, 10, 100, 20, 255, 0, 0)

-- Semi-transparent green rectangle (center screen)
mod.drawRect(200, 90, 112, 36, 0, 255, 0, 128)

-- Black background bar (like a HUD element)
mod.drawRect(0, 200, 512, 16, 0, 0, 0, 200)
```

---

#### `mod.drawText(text, x, y, [fontType], [justify])`

**EN:** Queue text to be drawn on screen during the next render.

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `text` | string | — | Text to draw (max 127 characters) |
| `x` | int | — | X coordinate (PS1 512x216 space) |
| `y` | int | — | Y coordinate (PS1 512x216 space) |
| `fontType` | int | `2` | 1 = big font (tall), 2 = small font (narrow) |
| `justify` | int | `0` | 0 = left-aligned, 1 = centered, 2 = right-aligned |

**Returns:** nothing

**ES:** Encola texto para dibujar en pantalla durante el próximo render.

| Parámetro | Tipo | Default | Descripción |
|-----------|------|---------|-------------|
| `text` | string | — | Texto a dibujar (máx. 127 caracteres) |
| `x` | int | — | Coordenada X (espacio PS1 512x216) |
| `y` | int | — | Coordenada Y (espacio PS1 512x216) |
| `fontType` | int | `2` | 1 = fuente grande (alta), 2 = fuente pequeña (angosta) |
| `justify` | int | `0` | 0 = alineado a la izquierda, 1 = centrado, 2 = alineado a la derecha |

**Retorna:** nada

```lua
-- Draw centered "Hello World" with small font
mod.drawText("Hello World", 256, 108, 2, 1)

-- Draw left-aligned text with big font at top-left
mod.drawText("SCORE: 999", 10, 10, 1, 0)

-- Draw right-aligned text with small font at top-right
mod.drawText("LAP 3/3", 502, 10, 2, 2)

-- Display driver info
local d = mod.getDriver(0)
if d.valid then
    mod.drawText("Speed: " .. d.speedApprox, 10, 200, 2, 0)
    mod.drawText("Reserves: " .. d.reserves, 10, 210, 2, 0)
end
```

---

## Hook Execution Order / Orden de Ejecución de Hooks

**English:**

Hooks are called in this specific order each frame:

1. **`onInput`** — After `GAMEPAD_ProcessAnyoneVars()` processes gamepad input. Best for reading button states or modifying input behavior.
2. **`onUpdate`** — During game logic update. Driver data is cached fresh before this hook fires. Best for modifying game state, driver stats, etc.
3. **`onRender`** — Before rendering and GPU submission. Call `mod.drawRect()` / `mod.drawText()` here to queue UI elements. The draw queue is flushed automatically after this hook returns, just before the GPU processes the ordering table.

**Initialization hooks:**

- **`onInit`** — Called once after all enabled mods' `main.lua` scripts have been loaded. Best for initialization, loading config files, setting up state.
- **`onTitleInit`** — Called when a new level, track, or adventure area is loaded. Best for per-track setup, resetting state between tracks.

**Per-frame flow:**

```
Frame Start
  ├── Process input
  ├── onInput hook
  ├── Cache game state (driver pointers, numPlayers, gameMode)
  ├── onUpdate hook
  ├── Game logic update
  ├── onRender hook
  ├── Flush draw queue (draw all queued rects/text)
  └── GPU render
```

**Español:**

Los hooks se llaman en este orden específico cada frame:

1. **`onInput`** — Después de que `GAMEPAD_ProcessAnyoneVars()` procesa el input de los gamepads. Ideal para leer estados de botones o modificar el comportamiento del input.
2. **`onUpdate`** — Durante la actualización de la lógica del juego. Los datos del piloto se cachean frescos antes de que este hook se dispare. Ideal para modificar el estado del juego, estadísticas del piloto, etc.
3. **`onRender`** — Antes del renderizado y envío a la GPU. Llama a `mod.drawRect()` / `mod.drawText()` aquí para encolar elementos UI. La cola de dibujo se vacía automáticamente después de que este hook retorna, justo antes de que la GPU procese la tabla de ordenamiento.

**Hooks de inicialización:**

- **`onInit`** — Se llama una vez después de que todos los scripts `main.lua` de los mods activados se hayan cargado. Ideal para inicialización, cargar archivos de configuración, configurar estado.
- **`onTitleInit`** — Se llama cuando se carga un nuevo nivel, pista o zona de aventura. Ideal para configuración por pista, reiniciar estado entre pistas.

**Flujo por frame:**

```
Inicio del Frame
  ├── Procesar input
  ├── Hook onInput
  ├── Cachear estado del juego (punteros de piloto, numPlayers, gameMode)
  ├── Hook onUpdate
  ├── Actualización de lógica del juego
  ├── Hook onRender
  ├── Vaciar cola de dibujo (dibujar todos los rects/texto encolados)
  └── Renderizado GPU
```

```lua
-- Complete example showing all hooks in order:
mod.hook("onInit", function()
    mod.log("Mod loaded!")
    -- Load config
    local cfg = mod.readFile("config.txt")
end)

mod.hook("onTitleInit", function()
    mod.log("New track loaded!")
    -- Reset per-track state
end)

mod.hook("onInput", function()
    -- Check input (gamepad state read via memory)
end)

mod.hook("onUpdate", function()
    -- Modify game state
    local d = mod.getDriver(0)
    if d.valid and d.reserves < 100 then
        mod.setDriverField(0, "reserves", 9600)
    end
end)

mod.hook("onRender", function()
    -- Draw UI
    local d = mod.getDriver(0)
    if d.valid then
        mod.drawText("Reserves: " .. d.reserves, 10, 10, 2, 0)
    end
end)
```

---

## File Override System / Sistema de Reemplazo de Archivos

**English:**

Mods can replace ANY game asset by placing files in their `files/` directory, mirroring the path structure from `BIGFILE.TXT`. This works in all data modes: PACKED, UNPACKED, and HYBRID.

**Lookup order (first match wins):**

1. Each enabled mod's `files/` directory (checked in scan order — the order they appear in the mods menu)
2. The global `assets/BIGFILE/` unpacked folder
3. The `assets/` directory directly
4. The original `BIGFILE.BIG` archive (if using PACKED mode)

**How to override a file:**

Given a `BIGFILE.TXT` entry like:
```
levels/tracks/coco/1P/data.lev
```

Place the replacement at:
```
mods/my_mod/files/levels/tracks/coco/1P/data.lev
```

**Language file override:**

To override a language file, place it at:
```
mods/my_mod/files/lang/en.lng
```

When a mod is toggled on/off, the language system automatically reloads, picking up the override.

**Multiple mods overriding the same file:**

The mod that appears first in the mods menu (earliest scan order) wins. Mods are scanned in filesystem order (directory creation order).

**Español:**

Los mods pueden reemplazar CUALQUIER asset del juego colocando archivos en su directorio `files/`, reflejando la estructura de rutas de `BIGFILE.TXT`. Esto funciona en todos los modos de datos: PACKED, UNPACKED y HYBRID.

**Orden de búsqueda (la primera coincidencia gana):**

1. El directorio `files/` de cada mod activado (en orden de escaneo — el orden en que aparecen en el menú de mods)
2. La carpeta global `assets/BIGFILE/` (unpacked)
3. El directorio `assets/` directamente
4. El archivo `BIGFILE.BIG` original (si se usa modo PACKED)

**Cómo reemplazar un archivo:**

Dada una entrada de `BIGFILE.TXT` como:
```
levels/tracks/coco/1P/data.lev
```

Coloca el reemplazo en:
```
mods/mi_mod/files/levels/tracks/coco/1P/data.lev
```

**Reemplazo de archivo de idioma:**

Para reemplazar un archivo de idioma, colócalo en:
```
mods/mi_mod/files/lang/es.lng
```

Cuando se activa/desactiva un mod, el sistema de idioma se recarga automáticamente, detectando el reemplazo.

**Múltiples mods reemplazando el mismo archivo:**

El mod que aparece primero en el menú de mods (orden de escaneo más temprano) gana. Los mods se escanean en orden del sistema de archivos (orden de creación del directorio).

---

## PS1 Coordinate System / Sistema de Coordenadas PS1

**English:**

The PS1 framebuffer resolution used by CTR is **512 × 216** pixels. All drawing coordinates use this space and are automatically scaled to the host window by the native renderer.

```
Screen boundaries:
  X: 0 to 511 (left to right)
  Y: 0 to 215 (top to bottom)

Center of screen:
  X = 256 (0x100)
  Y = 108 (0x6C)
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

**Font sizes / Tamaños de fuente:**

| Font type | Approx char width | Approx char height | Best for |
|-----------|-------------------|--------------------|----------|
| Big (1) | ~12 px | ~14 px | Titles, headings |
| Small (2) | ~7 px | ~9 px | HUD, stats, data |

**Español:**

La resolución del framebuffer de PS1 usada por CTR es de **512 × 216** píxeles. Todas las coordenadas de dibujo usan este espacio y son escaladas automáticamente a la ventana por el renderizador nativo.

```
Límites de la pantalla:
  X: 0 a 511 (izquierda a derecha)
  Y: 0 a 215 (arriba a abajo)

Centro de la pantalla:
  X = 256 (0x100)
  Y = 108 (0x6C)
```

**Tamaños de fuente:**

| Tipo de fuente | Ancho aprox | Alto aprox | Ideal para |
|----------------|-------------|------------|------------|
| Grande (1) | ~12 px | ~14 px | Títulos, encabezados |
| Pequeña (2) | ~7 px | ~9 px | HUD, estadísticas, datos |

---

## Internal Draw Queue / Cola de Dibujo Interna

**English:**

Drawing commands from all mods are collected in a shared queue and flushed once per frame, after the `onRender` hook completes.

- **Maximum queue size:** 256 commands per frame (shared across all mods)
- **Flush timing:** After `onRender` hook, before GPU processes the ordering table
- **Coordinate system:** PS1 framebuffer space (512×216)
- **Z-ordering:** Draw commands are placed into the UI ordering table, rendering above the game world

When the queue overflows, a warning is printed to the console: `[Mods] Draw queue overflow, ignoring drawRect/drawText`.

**Español:**

Los comandos de dibujo de todos los mods se recolectan en una cola compartida y se vacían una vez por frame, después de que el hook `onRender` se completa.

- **Tamaño máximo de cola:** 256 comandos por frame (compartido entre todos los mods)
- **Momento de vaciado:** Después del hook `onRender`, antes de que la GPU procese la tabla de ordenamiento
- **Sistema de coordenadas:** Espacio del framebuffer PS1 (512×216)
- **Orden Z:** Los comandos de dibujo se colocan en la tabla de ordenamiento de UI, renderizando sobre el mundo del juego

Cuando la cola se desborda, se imprime una advertencia en la consola: `[Mods] Draw queue overflow, ignoring drawRect/drawText`.

---

## Mod Persistence / Persistencia de Mods

**English:**

**State persistence:** Mod enabled/disabled state is automatically saved to `mods_state.cfg` (in the game directory) on:
- Game exit (via `atexit` handler)
- Each manual toggle from the in-game Mods menu

**Format of `mods_state.cfg`:**
```
my_mod=1
hello_world=1
another_mod=0
```

- `1` = mod is enabled
- `0` = mod is disabled

**Data persistence:** Use `mod.writeFile()` and `mod.readFile()` to persist mod-specific data. Recommended formats: JSON (manual string building), Lua tables (via `string.dump`), or simple text/config formats.

**Español:**

**Persistencia de estado:** El estado activado/desactivado de los mods se guarda automáticamente en `mods_state.cfg` (en el directorio del juego) al:
- Salir del juego (mediante el manejador `atexit`)
- Cada cambio manual desde el menú de Mods del juego

**Formato de `mods_state.cfg`:**
```
mi_mod=1
hello_world=1
otro_mod=0
```

- `1` = mod activado
- `0` = mod desactivado

**Persistencia de datos:** Usa `mod.writeFile()` y `mod.readFile()` para persistir datos específicos del mod. Formatos recomendados: JSON (construcción manual de strings), tablas de Lua (vía `string.dump`), o formatos simples de texto/config.

---

## Available Lua Libraries / Librerías Lua Disponibles

**English:**

The full Lua 5.4 standard library is available to all mods:

| Library | Functions | Notes |
|---------|-----------|-------|
| `math` | `abs`, `floor`, `ceil`, `min`, `max`, `sin`, `cos`, `sqrt`, `random`, `randomseed`, etc. | Useful for calculations, procedural generation |
| `string` | `sub`, `gsub`, `find`, `match`, `format`, `upper`, `lower`, `byte`, `char`, etc. | String manipulation, text formatting |
| `table` | `insert`, `remove`, `sort`, `concat`, `unpack` | Data structures |
| `os` | `time`, `date`, `difftime` | Time functions only (no `os.execute`) |
| `io` | `open`, `read`, `write`, `close` | File I/O (sandboxed to mod directory) |
| `utf8` | `char`, `code`, `len`, `sub` | UTF-8 string handling |

> **Security note:** The `os.execute`, `os.rename`, `os.remove`, `io.popen`, and `loadlib` functions are restricted. Use `mod.readFile`/`mod.writeFile` for sandboxed file access.

**Español:**

La librería estándar completa de Lua 5.4 está disponible para todos los mods:

| Librería | Funciones | Notas |
|----------|-----------|-------|
| `math` | `abs`, `floor`, `ceil`, `min`, `max`, `sin`, `cos`, `sqrt`, `random`, `randomseed`, etc. | Útil para cálculos, generación procedural |
| `string` | `sub`, `gsub`, `find`, `match`, `format`, `upper`, `lower`, `byte`, `char`, etc. | Manipulación de strings, formato de texto |
| `table` | `insert`, `remove`, `sort`, `concat`, `unpack` | Estructuras de datos |
| `os` | `time`, `date`, `difftime` | Solo funciones de tiempo (sin `os.execute`) |
| `io` | `open`, `read`, `write`, `close` | E/S de archivos (limitada al directorio del mod) |
| `utf8` | `char`, `code`, `len`, `sub` | Manejo de strings UTF-8 |

> **Nota de seguridad:** Las funciones `os.execute`, `os.rename`, `os.remove`, `io.popen` y `loadlib` están restringidas. Usa `mod.readFile`/`mod.writeFile` para acceso a archivos dentro de la caja de arena.

---

## Examples / Ejemplos

---

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

---

### Example 2: Player HUD Info

**EN:** Display player 1's turbo reserves, speed, and fire status on screen.

**ES:** Muestra las reservas de turbo, velocidad y estado de fuego del jugador 1 en pantalla.

```lua
mod.hook("onRender", function()
    local d = mod.getDriver(0)
    if d.valid then
        -- Show reserves and speed
        mod.drawText("Reserves: " .. d.reserves, 10, 10, 2, 0)
        mod.drawText("Speed: " .. d.speedApprox, 10, 22, 2, 0)
        
        -- Show fire status
        local fireStatus = "None"
        if d.fireSpeedCap > d.const_SacredFireSpeed then
            fireStatus = "Ultra Sacred Fire!"
        elseif d.fireSpeedCap > d.const_SingleTurboSpeed then
            fireStatus = "Sacred Fire!"
        end
        mod.drawText(fireStatus, 10, 34, 1, 0)
    end
end)
```

---

### Example 3: Infinite Reserves with Toggle

**EN:** Keep reserves maxed out while the mod is active. Reads a config file to determine the reserve value.

**ES:** Mantiene las reservas al máximo mientras el mod está activo. Lee un archivo de configuración para determinar el valor de las reservas.

```lua
-- Load reserve target from config, default to 9600
local targetReserves = 9600

mod.hook("onInit", function()
    local cfg = mod.readFile("config.txt")
    if cfg then
        targetReserves = tonumber(cfg) or 9600
    end
    mod.log("Target reserves set to: " .. targetReserves)
end)

mod.hook("onUpdate", function()
    local numPlyr = mod.getNumPlayers()
    for i = 0, numPlyr - 1 do
        local d = mod.getDriver(i)
        if d.valid and d.reserves > 0 then
            mod.setDriverField(i, "reserves", targetReserves)
        end
    end
end)
```

```
-- mods/infinite_reserves/files/config.txt
9600
```

---

### Example 4: Speed Bar with Colors

**EN:** Draw a colored bar showing the player's speed, changing color based on fire type.

**ES:** Dibuja una barra de colores mostrando la velocidad del jugador, cambiando de color según el tipo de fuego.

```lua
local BAR_X = 200
local BAR_Y = 10
local BAR_W = 100
local BAR_H = 8

mod.hook("onRender", function()
    local d = mod.getDriver(0)
    if not d.valid then return end

    -- Background (dark gray)
    mod.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, 32, 32, 32)

    -- Speed fill (clamped to bar width)
    local fill = math.min(d.speedApprox / 20000 * BAR_W, BAR_W)
    if fill > 0 then
        local r, g = 0, 255

        -- Green = base speed
        -- Yellow = Sacred Fire
        -- Orange = Ultra Sacred Fire
        if d.fireSpeedCap > d.const_SacredFireSpeed then
            r, g = 255, 64   -- Orange
        elseif d.fireSpeedCap > d.const_SingleTurboSpeed then
            r, g = 255, 255  -- Yellow
        end

        mod.drawRect(BAR_X, BAR_Y, math.floor(fill), BAR_H, r, g, 0, 200)
    end
end)
```

---

### Example 5: File Override — Custom Track

**EN:** Replace a track's level data with a custom one via file override.

**ES:** Reemplaza los datos de nivel de una pista con uno personalizado mediante reemplazo de archivos.

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

mod.hook("onTitleInit", function()
    mod.log("Track loaded — custom data should be active")
end)
```

---

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
    -- Each entry is 4 bytes (32-bit pointer)
    local driver0ptr = mod.readS32(gGT, 0x24EC)

    if driver0ptr and driver0ptr ~= 0 then
        -- Read and modify driver's reserves at offset 0x000
        local reserves = mod.readS16(driver0ptr, 0x000)
        mod.writeS16(driver0ptr, 0x000, reserves + 100)
    end

    mod.log("Mode=0x" .. string.format("%X", mode) .. " Players=" .. numPlyr)
end)
```

---

### Example 7: Multi-Player Modifier

**EN:** Apply an effect to all players in the game.

**ES:** Aplica un efecto a todos los jugadores en la partida.

```lua
mod.hook("onUpdate", function()
    local numPlyr = mod.getNumPlayers()
    
    for i = 0, numPlyr - 1 do
        local d = mod.getDriver(i)
        if d.valid then
            -- Give all players a speed boost
            local newSpeed = d.speedApprox + 500
            mod.setDriverField(i, "speedApprox", newSpeed)
        end
    end
end)

mod.hook("onRender", function()
    local numPlyr = mod.getNumPlayers()
    
    -- Display info for all players
    local y = 10
    for i = 0, numPlyr - 1 do
        local d = mod.getDriver(i)
        if d.valid then
            mod.drawText("P" .. (i+1) .. ": " .. d.speedApprox, 400, y, 2, 0)
            y = y + 12
        end
    end
end)
```

---

### Example 8: Simple Lap Counter Overlay

**EN:** Display a custom lap counter overlay using `drawText` and `drawRect`.

**ES:** Muestra un contador de vueltas personalizado usando `drawText` y `drawRect`.

```lua
mod.hook("onRender", function()
    -- Draw a dark background bar at the top
    mod.drawRect(0, 0, 512, 20, 0, 0, 0, 180)
    
    -- Draw player lap info
    local numPlyr = mod.getNumPlayers()
    local x = 10
    for i = 0, numPlyr - 1 do
        local d = mod.getDriver(i)
        if d.valid then
            -- Color based on driver (just for visual variety)
            local colors = {
                {255, 200, 0},   -- P1: Gold
                {0, 150, 255},   -- P2: Blue
                {255, 50, 50},   -- P3: Red
                {0, 255, 100},   -- P4: Green
            }
            local c = colors[(i % #colors) + 1]
            
            mod.drawText("P" .. (i+1), x, 3, 2, 0)
            x = x + 60
        end
    end
end)
```

---

## Troubleshooting / Solución de Problemas

### English

**Mod not showing in the mods list:**
- Make sure the mod directory is inside `mods/` next to the game executable
- Verify `main.lua` exists and is readable
- Check the console output for `[Mods]` error messages
- Maximum 64 mods can be registered

**Draw calls not appearing on screen:**
- Make sure `mod.drawText()` / `mod.drawRect()` are called inside an `onRender` hook (not `onUpdate` or `onInit`)
- Verify coordinates are within the PS1 framebuffer (512×216)
- Check that the draw queue isn't overflowing (max 256 commands per frame across all mods)
- If using alpha values, ensure they're high enough to be visible (a > 0)

**Lua errors:**
- Errors are caught and logged with `[Mods] Lua error in <mod>: <message>`
- Check the console/log output for details including the Lua stack trace
- Common issues: nil value errors (forgot to check `d.valid`), type errors (passed string instead of number)

**File overrides not working:**
- Verify the file path matches the path in `BIGFILE.TXT` exactly (case-sensitive on some platforms)
- Check that the mod is enabled in the mods menu
- Ensure the file is in `mods/<name>/files/<path>`, not directly in `mods/<name>/<path>`
- Language file overrides require toggling the mod (or restarting) to take effect

**Mod conflicts:**
- Multiple mods hooking the same event: only the last hook registered per mod is used
- Multiple mods overriding the same file: the earliest mod in scan order wins
- Global variable collisions: all mods share a single Lua VM — use `local` variables

**`mod.getDriver()` returns `valid = false`:**
- The player slot might be empty (e.g., player index > number of active players)
- Check with `mod.getNumPlayers()` first
- The game might be in a menu or loading screen

### Español

**El mod no aparece en la lista de mods:**
- Asegúrate de que el directorio del mod esté dentro de `mods/` junto al ejecutable
- Verifica que `main.lua` exista y sea legible
- Revisa la salida de la consola en busca de mensajes de error `[Mods]`
- Máximo 64 mods pueden registrarse

**Las llamadas de dibujo no aparecen en pantalla:**
- Asegúrate de que `mod.drawText()` / `mod.drawRect()` se llamen dentro de un hook `onRender` (no `onUpdate` o `onInit`)
- Verifica que las coordenadas estén dentro del framebuffer PS1 (512×216)
- Revisa que la cola de dibujo no se desborde (máx. 256 comandos por frame entre todos los mods)
- Si usas valores alfa, asegúrate de que sean suficientemente altos para ser visibles (a > 0)

**Errores de Lua:**
- Los errores se capturan y registran con `[Mods] Lua error in <mod>: <message>`
- Revisa la salida de la consola para más detalles, incluyendo el stack trace de Lua
- Problemas comunes: errores de valor nil (olvidaste verificar `d.valid`), errores de tipo (pasaste string en lugar de número)

**Los reemplazos de archivos no funcionan:**
- Verifica que la ruta del archivo coincida exactamente con la ruta en `BIGFILE.TXT` (sensible a mayúsculas en algunas plataformas)
- Revisa que el mod esté activado en el menú de mods
- Asegúrate de que el archivo esté en `mods/<nombre>/files/<ruta>`, no directamente en `mods/<nombre>/<ruta>`
- Los reemplazos de archivos de idioma requieren activar/desactivar el mod (o reiniciar) para que surtan efecto

**Conflictos entre mods:**
- Múltiples mods usando el mismo hook: solo se usa el último hook registrado por mod
- Múltiples mods reemplazando el mismo archivo: el mod más temprano en orden de escaneo gana
- Colisiones de variables globales: todos los mods comparten una sola VM de Lua — usa variables `local`

**`mod.getDriver()` devuelve `valid = false`:**
- El espacio del jugador podría estar vacío (ej: índice de jugador > número de jugadores activos)
- Verifica con `mod.getNumPlayers()` primero
- El juego podría estar en un menú o pantalla de carga

---

## Tips & Best Practices / Consejos y Buenas Prácticas

### English

- **Performance:** Keep `onUpdate` and `onRender` hooks lightweight. Heavy Lua computation can slow down the game. Avoid expensive operations (file I/O, large loops) inside per-frame hooks.
- **Drawing:** Queue all draw commands inside `onRender`. Draw commands are flushed automatically before GPU submission.
- **Local variables:** Always use `local` for variables to avoid polluting the global namespace (all mods share one Lua VM).
- **State persistence:** Mod enabled/disabled state is saved automatically. Use `mod.writeFile()`/`mod.readFile()` for custom data persistence.
- **Memory safety:** Use `setDriverField()` instead of raw memory writes when possible — it validates the field name and handles alignment.
- **Debugging:** Use `mod.log()` liberally to debug your mod. Output appears in the console with `[Mod]` prefix.
- **File paths:** Always use forward slashes (`/`) in file paths, even on Windows.
- **Check `valid`:** Always check `d.valid` before accessing other driver fields to avoid crashes.
- **Modular code:** Use `require` and `dofile` to split your mod into multiple Lua files for better organization.
- **Config files:** Store configuration in `files/` using `mod.readFile()`/`mod.writeFile()` for easy user customization.
- **Coordinate planning:** Plan your UI layout on a 512×216 grid. Use `math.floor()` for pixel-perfect positioning.

### Español

- **Rendimiento:** Mantén los hooks `onUpdate` y `onRender` ligeros. Cómputos pesados en Lua pueden ralentizar el juego. Evita operaciones costosas (E/S de archivos, bucles grandes) dentro de hooks por frame.
- **Dibujo:** Encola todos los comandos de dibujo dentro de `onRender`. Los comandos se vacían automáticamente antes del envío a la GPU.
- **Variables locales:** Usa siempre `local` para variables para evitar contaminar el espacio de nombres global (todos los mods comparten una VM de Lua).
- **Persistencia de estado:** El estado activado/desactivado se guarda automáticamente. Usa `mod.writeFile()`/`mod.readFile()` para persistencia de datos personalizada.
- **Seguridad de memoria:** Usa `setDriverField()` en lugar de escrituras de memoria directas cuando sea posible — valida el nombre del campo y maneja la alineación.
- **Depuración:** Usa `mod.log()` liberalmente para depurar tu mod. La salida aparece en la consola con el prefijo `[Mod]`.
- **Rutas de archivo:** Usa siempre barras diagonales (`/`) en las rutas de archivo, incluso en Windows.
- **Verificar `valid`:** Siempre verifica `d.valid` antes de acceder a otros campos del piloto para evitar fallos.
- **Código modular:** Usa `require` y `dofile` para dividir tu mod en múltiples archivos Lua para mejor organización.
- **Archivos de configuración:** Almacena la configuración en `files/` usando `mod.readFile()`/`mod.writeFile()` para fácil personalización por el usuario.
- **Planificación de coordenadas:** Planifica tu diseño UI en una cuadrícula de 512×216. Usa `math.floor()` para posicionamiento perfecto de píxeles.

---

> **CTR Native — Modding Guide**  
> English v1.0 | Spanish v1.0  
> *This guide is a living document — if you find errors or missing information, please contribute!*
