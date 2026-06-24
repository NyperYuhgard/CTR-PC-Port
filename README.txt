============================================================
  CTR Native — Mod Engine + Reserve Bar Mod
  Paquete de instalacion v5
============================================================

CONTENIDO DEL PAQUETE
=====================

1. Motor de Mods (C engine) — LECTURA + ESCRITURA
   - platform/native_mods.c       — Sistema de mods con Lua VM (read + write API)
   - platform/native_bigfile.c    — Lectura dual de BIGFILE (packed/unpacked/hybrid)
   - platform/native_assets.c     — Gestion de assets (acepta BIGFILE/ como fuente)
   - include/platform/native_mods.h  — Header del motor de mods
   - include/platform/native_bigfile.h — Header del sistema bigfile dual

2. Menu de Mods (CTR-native style)
   - game/230/230_86_MM_Mods_Menu.c  — Menu de mods integrado en el menu principal
   - game/230/D230.c                 — Estructura del menu (fila "MODS" agregada)

3. Correcciones criticas del pipeline de carga
   - game/LOAD/LOAD_21_ReadFile.c         — FIX: callback chain ya no se sobreescribe
   - game/LOAD/LOAD_16_DramFileCallback.c — Debug logging + fix de callback
   - game/LOAD/LOAD_18_VramFileCallback.c — Debug logging para uploads VRAM
   - game/LOAD/LOAD_36_NextQueuedFile.c   — Debug logging para dequeue/dispatch
   - game/LOAD/LOAD_09_Callback_DriverModels.c — Debug logging load_inProgress
   - game/LOAD/LOAD_07_Callback_LEV.c     — Debug logging para callback LEV

4. Mods Lua
   - mods/reserve_bar/main.lua   — Mod de barra de reservas (con demo de escritura)
   - example_mod/main.lua        — Mod de ejemplo (Speed Display)
   - example_mod/files/config.txt — Config del mod de ejemplo


INSTRUCCIONES DE INSTALACION
=============================

PASO 1: Copiar archivos fuente
-------------------------------
Copia los archivos a tu proyecto CTR Native, manteniendo la estructura
de directorios:

  platform/          ->  tu_proyecto/platform/
  include/platform/  ->  tu_proyecto/include/platform/
  game/230/          ->  tu_proyecto/game/230/
  game/LOAD/         ->  tu_proyecto/game/LOAD/

Estos archivos REEMPLAZAN los originales del mismo nombre.
El proyecto usa unity build (main.c incluye todos los .c), asi que
no necesitas modificar CMakeLists.txt.


PASO 2: Crear carpeta de mods
------------------------------
Crea una carpeta "mods/" junto al ejecutable (o dentro de assets/):

  tu_ejecutable/
    ctr_native
    assets/
      BIGFILE.BIG (o BIGFILE/)
      mods/           <--- crear esta carpeta
        reserve_bar/
          main.lua    <--- copiar desde mods/reserve_bar/main.lua


PASO 3: Compilar
----------------
Compila el proyecto normalmente:

  Linux/macOS:  ./build.sh
  Windows:      build.bat

El CMakeLists.txt no necesita cambios porque el proyecto usa unity build.


COMO FUNCIONA EL SISTEMA DE MODS
=================================

1. Al iniciar, el motor escanea la carpeta mods/ buscando
   subcarpetas que contengan un archivo main.lua

2. Cada mod descubierto se agrega al menu de mods (en el menu
   principal del juego, fila "MODS")

3. Los mods habilitados se cargan automaticamente: su main.lua
   se ejecuta en una VM de Lua dedicada

4. Los mods registran callbacks via mod.hook() que se ejecutan
   en puntos especificos del game loop:
     - onInit:     Al cargar los mods
     - onUpdate:   Cada frame, durante la logica del juego
     - onRender:   Cada frame, durante el renderizado (para dibujar)
     - onInput:    Al procesar input del gamepad
     - onTitleInit: Al inicializar la pantalla de titulo

5. Los mods pueden LEER estado del juego (observar) y ESCRIBIR
   en la memoria del juego (modificar comportamiento)


API DE MODS LUA — LECTURA
==========================

mod.getDriver(index)
  Retorna tabla con datos del jugador (0-7):
    { valid, reserves, fireSpeedCap, turbo_MeterRoomLeft,
      turbo_outsideTimer, numTurbos, const_SacredFireSpeed,
      const_SingleTurboSpeed, speedApprox, actionsFlagSet,
      const_turboMaxRoom, driverID }

mod.getNumPlayers()
  Numero de jugadores en la partida actual

mod.getGameMode()
  Modo de juego actual (gameMode1)

mod.getGameTracker()
  Retorna puntero (lightuserdata) al GameTracker.
  Combinado con readS16/readU8/readS32 permite leer CUALQUIER
  campo del estado del juego.
  Offsets conocidos:
    0x000  gameMode1       (s32)
    0x343  numPlyrCurrGame (u8)
    0x24EC drivers[]       (array de 8 punteros)

mod.readS16(pointer, offset)   — Leer s16 de memoria
mod.readU8(pointer, offset)    — Leer u8 de memoria
mod.readS32(pointer, offset)   — Leer s32 de memoria


API DE MODS LUA — ESCRITURA (CAMBIAR COMPORTAMIENTO)
=====================================================

mod.setDriverField(playerIndex, fieldName, value)
  Escribe un campo del driver de forma segura por nombre.
  Campos escribibles:
    "reserves"            — Cantidad de reservas (s16)
    "fireSpeedCap"        — Velocidad maxima de fuego (s16)
    "turbo_MeterRoomLeft" — Espacio en barra de turbo (s16)
    "turbo_outsideTimer"  — Timer de turbo externo (s16)
    "numTurbos"           — Numero de turbos acumulados (u8)
    "kartState"           — Estado del kart (u8)
    "actionsFlagSet"      — Flags de acciones (s16)
    "speedApprox"         — Velocidad aproximada (s16)
  Retorna: true si exito, false si fallo

mod.writeS16(pointer, offset, value)  — Escribir s16 en memoria
mod.writeU8(pointer, offset, value)   — Escribir u8 en memoria
mod.writeS32(pointer, offset, value)  — Escribir s32 en memoria

  Estos permiten modificar CUALQUIER campo del juego si se conoce
  el offset correcto. Usar con cuidado.


API DE MODS LUA — DIBUJO
==========================

mod.drawRect(x, y, w, h, r, g, b, [a])
  Dibujar rectangulo coloreado (a=255 por defecto, opaco)

mod.drawText(text, x, y, [font], [justify])
  Dibujar texto en pantalla
  font: 1=grande, 2=pequeno (default: 2)
  justify: 0=izquierda, 1=centro, 2=derecha (default: 0)


API DE MODS LUA — UTILIDADES
==============================

mod.log(msg)               — Imprimir en consola
mod.getModPath()           — Ruta del mod actual
mod.readFile(path)         — Leer archivo de files/
mod.writeFile(path, data)  — Escribir archivo en files/
mod.hook(name, func)       — Registrar callback


EJEMPLOS DE MODS QUE CAMBIAN COMPORTAMIENTO
=============================================

1. Reservas infinitas:
   mod.hook("onUpdate", function()
       local d = mod.getDriver(0)
       if d.valid and d.reserves > 0 then
           mod.setDriverField(0, "reserves", 9600)
       end
   end)

2. Velocidad maxima aumentada:
   mod.hook("onUpdate", function()
       mod.setDriverField(0, "fireSpeedCap", 0x4000)
   end)

3. Acceder al GameTracker para modificar estado global:
   mod.hook("onUpdate", function()
       local gGT = mod.getGameTracker()
       if gGT then
           -- Cambiar numero de jugadores (offset 0x343)
           mod.writeU8(gGT, 0x343, 4)
       end
   end)

4. Leer cualquier campo del Driver con offset personalizado:
   mod.hook("onUpdate", function()
       local d = mod.getDriver(0)
       if d.valid then
           local gGT = mod.getGameTracker()
           if gGT then
               -- Leer puntero al driver[0] desde el array
               local driverPtr = mod.readS32(gGT, 0x24EC)
               if driverPtr then
                   -- Leer campo arbitrario (ejemplo: offset 0x3C0)
                   local val = mod.readS16(driverPtr, 0x3C0)
               end
           end
       end
   end)


MOD DE BARRA DE RESERVAS
=========================

El mod "reserve_bar" muestra una barra visual en el HUD:

  Gris:      Sin reservas
  Verde:     Reservas normales
  Amarillo:  Sacred Fire
  Naranja:   Ultra Sacred Fire

Configuracion editable en mods/reserve_bar/main.lua:

  barX              = 0x0A     (posicion X)
  barY              = 0x68     (posicion Y)
  barWidth          = 0x3C     (ancho, 60px)
  barHeight         = 0x06     (alto, 6px)
  maxReserveDisplay = 4800     (valor max para barra llena)
  showText          = true     (mostrar etiqueta "RESERVE")

  -- Comportamiento modificable:
  infiniteReserves  = false    (true = reservas infinitas)
  reserveCap        = 0        (> 0 = limitar reservas a este valor)


NOTAS IMPORTANTES
=================

- El fix critico en LOAD_21_ReadFile.c elimina la linea que sobreescribia
  callbackFuncPtr, lo cual causaba que el juego se congelara al iniciar.

- El sistema bigfile soporta 3 modos:
  PACKED:   Solo BIGFILE.BIG (original)
  UNPACKED: Solo carpeta BIGFILE/ (archivos extraidos)
  HYBRID:   Ambos existen; BIGFILE/ sobreescribe BIGFILE.BIG (ideal para modding)

- Los macros BFDBG_PRINTF generan log detallado en terminal.
  Filtrar con:  ./ctr_native 2>&1 | grep "[BF-DBG]"

- Coordenadas PS1 OT: 512x216 pixeles
  Centro: X=0x100 (256), Y=0x6C (108)

- Los hooks del motor ya estan integrados en el game loop:
    MainMain.c:    NativeMods_CacheGameState() + CallHook(ON_UPDATE)
    RenderFrame.c: NativeMods_CallHook(ON_RENDER) + FlushDrawQueue()
    main.c:        NativeMods_Init() + ScanMods()

- ADVERTENCIA: Las funciones de escritura (writeS16, writeU8, writeS32,
  setDriverField) modifican la memoria del juego directamente. Un mod
  mal escrito puede causar crashes. Usar con cuidado.
============================================================
