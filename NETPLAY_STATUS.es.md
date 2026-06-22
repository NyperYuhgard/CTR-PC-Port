# CTR PC Port — Estado de la Implementación de Netplay

## Resumen del Proyecto

Este documento describe la implementación de netplay (multijugador online) para
el proyecto CTR PC Port (`ctr_native`). El objetivo es carreras peer-to-peer
para dos jugadores sobre UDP, donde cada jugador ve su propia cámara a
pantalla completa (sin split-screen).

**Sistema de compilación:** CMake + MinGW Makefiles, 32 bits (`i686`), C99
(`-std=c99`).
**Plataforma:** Windows nativo (SDL/PsyCross), con directivas `CTR_NATIVE`
protegiendo todas las adiciones nativas.

---

## Arquitectura

### Modelo de Red
- **UDP peer-to-peer** — modelo host/cliente (sin servidor dedicado).
- **Puerto por defecto:** 14200.
- **Input replay** — cada máquina ejecuta la simulación completa del juego
  localmente, recibe el estado del mando del jugador remoto cada fotograma,
  y lo aplica al piloto correcto. No hay rollback, ni predicción, ni
  coincidencia de números de fotograma.
- **Tipos de paquete** (ver `include/platform/native_netplay.h:10-20`):
  - `HELLO` (0x01) — handshake
  - `INPUT` (0x02) — estado del mando por fotograma
  - `PING` (0x03) / `PONG` (0x04) — medición de latencia
  - `DISCONNECT` (0x05)
  - `START_RACE` (0x06) — el host señala preparación de carrera
  - `CHARACTER_SELECT` (0x07) — ambas direcciones
  - `TRACK_SELECT` (0x08) — host→cliente

### Mapeo de Inputs (ambas máquinas)

```
gamepad[0] = input físico del host
gamepad[1] = input físico del cliente
driver[0]  = personaje del host → lee de gamepad[0]
driver[1]  = personaje del cliente → lee de gamepad[1]
```

El bloque de sincronización de inputs está en
`game/MAIN/MainMain.c:356-437`.

### Mapeo de Cámaras (después de nuestra corrección)

**Máquina del host:**
```
cameraDC[0] → sigue a driver[0] (host) → escribe en pushBuffer[0] → RENDERIZADO pantalla completa
cameraDC[1] → sigue a driver[1] (cliente) → escribe en pushBuffer[1] → NO renderizado
El host ve: perspectiva de driver[0] ✓
```

**Máquina del cliente** (después del intercambio de cámaras):
```
cameraDC[0] → sigue a driver[1] (cliente) → escribe en pushBuffer[0] → RENDERIZADO pantalla completa
cameraDC[1] → sigue a driver[0] (host) → escribe en pushBuffer[1] → NO renderizado
El cliente ve: perspectiva de driver[1] ✓
```

### Flujo: Lobby → Carrera

1. El usuario se une o crea una partida → `main.c` llama a `Netplay_Host()`
   o `Netplay_Connect()`
2. La UI del lobby (`230_22_MM_Online_Menu.c`) maneja la selección de
   personaje/pista mediante `NETPLAY_PACKET_CHARACTER_SELECT` /
   `NETPLAY_PACKET_TRACK_SELECT`
3. `Online_StartRace()` (`230_22_MM_Online_Menu.c:62`) se llama en ambas
   máquinas:
   - Establece `g_NetplayRacing = 1`
   - Establece `data.characterIDs[0]` = personaje del host,
     `data.characterIDs[1]` = personaje del cliente
   - Establece `gGT->numPlyrCurrGame = 2`, `gGT->numPlyrNextGame = 2`
   - Dispara la carga de la pista mediante `data.menuQueueLoadTrack`
4. Durante la carga: `LOAD_44_TenStages.c` copia `numPlyrNextGame → numPlyrCurrGame`
5. `MainInit_06_Drivers.c` crea 2 pilotos (omite IA porque `!g_NetplayRacing`)
6. `MainInit_07_FinalizeInit.c`:
   - Inicializa 4 pushBuffers con rectángulos de split-screen 2P
   - Crea cámaras para ambos pilotos
   - **Código nuevo:** intercambia `driverToFollow` para el cliente,
     reinicia pushBuffer[0] con rectángulo 1P
7. El bucle principal del juego (`MainMain.c`) comienza, la sincronización
   de inputs se ejecuta cada fotograma
8. **Código nuevo:** durante `MainFrame_RenderFrame`, establece
   `numPlyrCurrGame = 1` para que solo se renderice 1 viewport a pantalla
   completa, sin líneas de split-screen

### Variables Globales Clave

| Variable | Archivo | Propósito |
|----------|---------|-----------|
| `g_NetplayAutoJoin` | `native_netplay.c:130` | Flag CLI para auto-conectar |
| `g_NetplayRaceStarting` | `native_netplay.c:131` | El cliente recibió señal de inicio |
| `g_NetplayRacing` | `native_netplay.c:132` | Actualmente en una carrera online |
| `g_NetplayHostCharacter` | `native_netplay.c:133` | -1 o índice del personaje del host |
| `g_NetplayClientCharacter` | `native_netplay.c:134` | -1 o índice del personaje del cliente |
| `g_NetplayTrackId` | `native_netplay.c:135` | ID de LEV de la pista seleccionada |
| `g_NetplayNumLaps` | `native_netplay.c:136` | Número de vueltas (por defecto 3) |

---

## Cambios Realizados

### 1. Ping Periódico (`platform/native_netplay.c`)

**Problema:** La infraestructura Ping/Pong existía pero nunca se activaba.

**Solución:** Se añadió un broadcast periódico de `NETPLAY_PACKET_PING` cada
segundo en `Netplay_Poll()` (`native_netplay.c:1017`). Cada ping lleva un
timestamp; el par lo devuelve como `NETPLAY_PACKET_PONG`. El RTT se calcula
y almacena en `playerInfo[].pingMs`.

**Archivo:** `platform/native_netplay.c:1017+` (dentro de `Netplay_Poll()`)

### 2. g_NetplayRacing Limpiado al Volver al Menú

**Problema:** Volver a la pantalla de título o desconectarse dejaba
`g_NetplayRacing = 1`, causando que el bloque de sincronización de inputs
sobrescribiera los estados del mando en los menús (dejándolos inoperativos).

**Solución:** Establecer `g_NetplayRacing = 0` en:
- `MM_JumpTo_Title_FirstTime()` (`230_72_MM_JumpTo_Title_FirstTime.c:34`)
- `Netplay_Disconnect()` (`native_netplay.c:875`)

### 3. Flujo de Selección de Personaje

**Problema:** Múltiples problemas:
- `g_NetplayHostCharacter` / `g_NetplayClientCharacter` inicializados a `0`,
  que coincidía con el índice de Crash.
- El host nunca enviaba su elección de personaje al cliente.
- El cliente no esperaba el personaje del host antes de empezar.

**Soluciones:**
- Cambiar el centinela de `0` a `-1` para "no seleccionado".
- El host ahora envía `NETPLAY_PACKET_CHARACTER_SELECT` con su elección
  (`230_22_MM_Online_Menu.c:418-421`).
- El manejador del cliente almacena el personaje del host en
  `g_NetplayHostCharacter` (`native_netplay.c:612-622`).
- El host espera `g_NetplayClientCharacter >= 0` antes de proceder a
  selección de pista.
- El cliente espera tanto `g_NetplayRaceStarting` como
  `g_NetplayHostCharacter >= 0` antes de llamar a `Online_StartRace()`.

### 4. Corrección de numPlyrNextGame

**Problema:** `Online_StartRace()` establecía `gGT->numPlyrCurrGame = 2`
pero no `numPlyrNextGame`. Durante la carga de pista
(`LOAD_44_TenStages.c:111`), `numPlyrCurrGame` se sobrescribía con el
`numPlyrNextGame` desactualizado (=1), causando que solo 1 piloto se creara.

**Solución:** Añadir `gGT->numPlyrNextGame = playerCount` en
`Online_StartRace()` (`230_22_MM_Online_Menu.c:71`).

### 5. Sin Split-Screen (Viewport Único por Máquina)

**Problema:** Con `numPlyrCurrGame = 2`, el renderizador produce split-screen
(mitades superior/inferior), cada una mostrando la cámara de un piloto
distinto. Cada jugador debería ver su propio personaje a pantalla completa.

**Solución:** Dos cambios:

#### 5a. Intercambio de Cámaras para el Cliente (`MainInit_07_FinalizeInit.c:132-148`)

Después de la inicialización de cámaras, cuando `g_NetplayRacing`:
- Para `localId != 0` (cliente): intercambia
  `cameraDC[0].driverToFollow` ↔ `cameraDC[1].driverToFollow` para que
  camera[0] siga al piloto local (cliente).
- Reinicia `pushBuffer[0]` con `PushBuffer_Init(&pushBuffer[0], 0, 1)` para
  obtener rectángulo de viewport a pantalla completa (`0x200×0xD8`) en lugar
  del rectángulo 2P de mitad superior (`0x200×0x6A`).

Incluye `<platform/native_netplay.h>` para acceder a `Netplay_GetLocalPlayerId()`.

#### 5b. Sobrescribir numPlyrCurrGame Durante el Renderizado (`MainMain.c:603-616`)

Antes de `MainFrame_RenderFrame()`:
- Guarda `gGT->numPlyrCurrGame`
- Establece `gGT->numPlyrCurrGame = 1` cuando `g_NetplayRacing`
- Después de RenderFrame, restaura el valor original

Esto causa que todas las funciones de renderizado usen la ruta 1P:
- Viewport único (pushBuffer[0]) renderizado a pantalla completa
- Líneas divisorias de split-screen omitidas
- Geometría de nivel renderizada mediante ruta 1P (`DrawLevelOvr1P`,
  `AnimateWater1P`, etc.)

---

## Bugs Conocidos y Problemas

### Críticos

#### B1. El Cliente No Puede Pausar/Reanudar

**Ubicación:** Múltiples sitios comprueban `gamepad[0].buttonsTapped`, ej.
`UI_44_RenderFrame_Racing.c:86`, lógica de pausa en MainMain.c.

**Problema:** `gamepad[0]` = input del host en AMBAS máquinas. En el cliente,
pulsar START en el mando físico va a `gamepad[1]`, pero la lógica de pausa
comprueba exclusivamente `gamepad[0]`. El cliente nunca puede pausar.

**Solución necesaria:** Modificar todas las comprobaciones de gamepad[0] para
que también comprueben gamepad[1], o añadir manejo de pausa específico para
netplay (ej. sincronizar estado de pausa por red). La solución más simple:
comprobar `gamepad[0] || gamepad[1]` para pausa en el bucle principal, y
broadcast del estado de pausa para que ambas máquinas se pausen juntas.

#### B2. El HUD se Dibuja para Ambos Pilotos (Basura para el Segundo)

**Ubicación:** `UI_44_RenderFrame_Racing.c`

**Problema:** El bucle del HUD en la línea 135 itera sobre TODOS los hilos
de jugador (`gGT->threadBuckets[PLAYER].thread` lista enlazada), NO sobre
`numPlyrCurrGame`. Con 2 pilotos, ambos reciben HUD. Pero `hudStructPtr` en
la línea 54 se selecciona basado en `numPlyrCurrGame = 1`, proporcionando
solo 1 conjunto de posiciones de elementos HUD. El HUD del segundo piloto
se dibuja usando datos de posición fuera de límites (leyendo más allá del
layout 1P).

**Impacto:** Corrupción visual — elementos HUD extra en posiciones de
pantalla basura. Podría potencialmente crash si coordenadas basura causan
operaciones de GPU inválidas.

**Solución necesaria:** O bien:
- (a) Saltar la segunda iteración del hilo de jugador cuando `g_NetplayRacing`
- (b) Cambiar el bucle para usar `numPlyrCurrGame` en lugar de la iteración
      de lista enlazada de hilos
- (c) Crear una ruta HUD específica para netplay que solo dibuje para el
      jugador local

La opción (b) es la más limpia: cambiar
`do { ... } while (playerThread != 0)` por
`for (int i = 0; i < numPlyr; i++)` con guarda temprana: en el host, saltar
`i == 1`; en el cliente, saltar `i == 0`.

#### B3. Advertencia "Mando 2 Desconectado"

**Ubicación:** `MainFrame_08_RenderFrame.c:22` (`DrawUnpluggedMsg`)

**Problema:** `DrawUnpluggedMsg` comprueba
`MainFrame_HaveAllPads(gGT->numPlyrNextGame)`. Como `numPlyrNextGame = 2`,
espera 2 mandos físicos. En ambas máquinas, solo hay 1 mando físico conectado
(el local).

**Solución necesaria:** En `DrawUnpluggedMsg`, saltar la comprobación de
mandos cuando `g_NetplayRacing` (o reducir los mandos esperados a 1 para
netplay).

### Medios

#### B4. Sin Coincidencia de Número de Fotograma en Sincronización de Inputs

**Ubicación:** `MainMain.c:356-437`, `native_netplay.c:986-998`

**Problema:** `Netplay_ReceiveInputs()` desencola lo que sea que esté en la
cola de inputs independientemente del número de fotograma. No hay
comprobación de que el fotograma del input recibido coincida con el fotograma
local actual. Combinado con diferentes tiempos de carga (las máquinas
empiezan la carrera con diferentes contadores de fotograma), esto significa:
- El host en el fotograma N usa el input del cliente del fotograma N-delta
  (input antiguo)
- El cliente en el fotograma M usa el input del host del fotograma M-delta
  (input antiguo)
- El delta es la diferencia en tiempos de inicio, típicamente 10-30 fotogramas

**Solución necesaria:** Implementar coincidencia de número de fotograma para
que los inputs se apliquen al fotograma de simulación correcto. Esto requiere
o bien:
- Lockstep: ambas máquinas esperan a la otra en cada fotograma
- Delay-based: bufferizar inputs durante un número fijo de fotogramas
- Rollback: predecir y corregir

La solución simple a corto plazo: añadir un parámetro de número de fotograma
a `Netplay_ReceiveInputs` y saltar inputs que no coincidan con el fotograma
actual, cayendo al último input coincidente o estado neutral.

#### B5. Inicio de Carrera No Sincronizado

**Ubicación:** `Online_StartRace()` dispara la carga de pista; la cuenta
atrás 3-2-1 comienza independientemente en cada máquina cuando la carga
termina.

**Problema:** La máquina A puede terminar de cargar 500ms antes que la
máquina B. La cuenta atrás comienza en diferente tiempo de pared, y los
contadores de fotograma divergen. Acoplado con B4, esto causa desajuste
input-fotograma.

**Solución necesaria:** Sincronizar el inicio de la carrera:
- Ambas máquinas señalan "listo" después de que la carga termine
- El host envía broadcast "GO" después de que ambas estén listas
- Ambas máquinas comienzan la cuenta atrás en el mismo fotograma

#### B6. Desincronización de Posición/Física de Vehículos

**Ubicación:** Toda la simulación (lógica de carrera en overlays, física en
archivos `Veh*`)

**Problema:** Usando input replay puro, cualquier diferencia en la temporización
de fotogramas, cómputo físico, o generación de números aleatorios causa que
las posiciones de los vehículos diverjan. En una carrera de 3 vueltas (2-3
minutos), la divergencia puede ser significativa.

**Solución necesaria:** Paquetes periódicos de sincronización de posición o
comparación de checksums de estado. Como mínimo, broadcast de la
posición/velocidad de cada piloto cada N fotogramas y aplicar correcciones
(con interpolación para evitar tirones).

#### B7. Objetos/Powerups/Cajas No Sincronizados

**Problema:** Las cajas de objetos (`pickup_types`), powerups, frutas wumpa,
y estados de destrucción de cajas son locales a cada máquina. El Jugador A
recoge una caja en la Máquina A, pero la Máquina B todavía muestra la caja
como disponible. El RNG para la asignación de objetos también es local, así
que el Jugador A podría recibir un misil mientras el Jugador B (viendo el
mismo pickup) le da al Jugador A un escudo.

**Solución necesaria:** O bien:
- Broadcast de eventos de recogida de cajas
- Usar una semilla RNG compartida (pero esto requiere ejecución determinista)

#### B8. Final de Carrera / Resultados No Sincronizados

**Problema:** El cruce de la línea de meta, el conteo de vueltas, y los
resultados de la carrera se calculan localmente. Ambas máquinas pueden no
estar de acuerdo sobre quién terminó primero, especialmente si las
posiciones han derivado (B6).

**Solución necesaria:** Sincronizar eventos de finalización y reconciliar
resultados. El host debería ser autoritario para los resultados de la
carrera, o ambas máquinas deberían acordar mediante checksum.

### Bajos

#### B9. El Hilo de Cámara Usa cameraID para Alguna Lógica

**Ubicación:** `CAM_ThTick` (en overlay 226-229), varias funciones de modo
de cámara

**Problema:** Después de nuestro intercambio de `driverToFollow` en el cliente,
`cameraDC[0]` sigue a `driver[1]` pero `cameraDC[0].cameraID` sigue siendo
`0`. Algunas funciones de cámara pueden usar `cameraID` (no `driverToFollow`)
para indexar en arrays. Si `cameraID` se usa para leer `driver[cameraID]`,
la cámara obtendría el piloto equivocado.

**Verificación necesaria:** Buscar usos de `cDC->cameraID` vs
`cDC->driverToFollow` en el código de cámara (overlay 226-229). Si cameraID
se usa para buscar pilotos o pushBuffers, el enfoque de intercambio puede
necesitar ajuste. Un enfoque más seguro sería intercambiar los punteros de
pushBuffer en lugar de driverToFollow.

#### B10. Desconexión Durante la Carrera

**Problema:** Si un jugador se desconecta en medio de una carrera, no hay
manejo. El input del par deja de llegar (gamepad se vuelve neutral/cero),
su piloto se detiene, y la carrera continúa con un coche parado en la pista.
Sin notificación ni pausa.

**Solución necesaria:** Manejar eventos de desconexión durante la carrera:
mostrar mensaje, opcionalmente pausar, y terminar la carrera gracefully.

#### B11. La Interpolación 60fps Solo Procesa pushBuffer[0]

**Ubicación:** `MainMain.c:526-563`

**Problema:** El código de interpolación 60fps usa `gGT->numPlyrCurrGame`
para determinar cuántos pushBuffers guardar/interpolar. Desde que lo
establecemos a 1 antes del bloque de interpolación, solo pushBuffer[0] se
interpola. pushBuffer[1] mantiene datos de cámara obsoletos. Esto no afecta
al renderizado (solo pushBuffer[0] se renderiza), pero si algún código lee
la posición de pushBuffer[1], obtiene datos interpolados u obsoletos.

**Impacto:** Mínimo, ya que pushBuffer[1] no se usa para nada crítico durante
carreras netplay.

#### B12. pushBuffer[1] Conserva el Rectángulo de Split-Screen 2P

**Ubicación:** `MainInit_07_FinalizeInit.c`

**Problema:** Reiniciamos `pushBuffer[0]` con rectángulo 1P, pero
`pushBuffer[1]` todavía tiene el rectángulo 2P de mitad inferior de la
inicialización original. Si algo lee `pushBuffer[1].rect`, obtiene
y=0x6E, h=0x6A (mitad inferior).

**Impacto:** Mínimo por ahora. Si una funcionalidad futura lee
pushBuffer[1].rect para posicionamiento, será incorrecto.

---

## Lista de Tareas

### Prioridad 1: Debe Arreglarse (Ruinoso)

#### T1. Arreglar Pausa para el Cliente

**Archivos:** `game/MAIN/MainMain.c` (~línea 580-592 en el bucle principal),
`game/UI/UI_44_RenderFrame_Racing.c:86`

**Qué hacer:**
Añadir manejo de pausa consciente de netplay. Cuando `g_NetplayRacing`:
- Comprobar tanto `gamepad[0]` como `gamepad[1]` para pulsación de START
- Broadcast del estado de pausa mediante un nuevo tipo de paquete
- Ambas máquinas se pausan/reanudan juntas cuando cualquier jugador pulsa START

**Alternativa simple:**
Simplemente comprobar `gamepad[0]` o `gamepad[1]` para pausa cuando
`g_NetplayRacing`. Esto permite que cualquier jugador pause localmente pero
no sincroniza el estado de pausa. Mejor que nada.

#### T2. Arreglar HUD para Viewport Único

**Archivos:** `game/UI/UI_44_RenderFrame_Racing.c`

**Qué hacer:**
Cambiar el bucle de hilos de jugador (líneas 135-579) para iterar sobre
`numPlyrCurrGame` en lugar de seguir la lista enlazada de hilos. Cuando
`g_NetplayRacing && numPlyrCurrGame == 1`, solo se dibuja el HUD del
jugador local.

Específicamente:
- Cambiar `do { ... } while (playerThread != 0)` por
  `for (int i = 0; i < numPlyr; i++)`
- Usar `gGT->drivers[i]` en lugar de `playerThread->object`
- En el host, solo `i == 0` debe dibujar (driver[0] = host)
- En el cliente (después del intercambio de cámaras), `i == 1` debe dibujar
  (driver[1] = cliente)
- Pero como `numPlyrCurrGame = 1` durante el renderizado, solo `i == 0` itera
- En el cliente, `gGT->drivers[0]` es el piloto del host, pero la cámara
  muestra la vista del cliente
- Así que necesitamos: host → dibujar para driver[0], cliente → dibujar para
  driver[1]

**Mejor enfoque:**
Después del intercambio de cámaras en el cliente, intercambiar qué piloto
está en el índice 0 para propósitos de HUD. O usar
`cameraDC[0].driverToFollow` para determinar qué HUD de piloto dibujar
(pero el código HUD no tiene acceso fácil a cameraDC).

**Solución más simple:**
Temporalmente (antes del dibujado de HUD) intercambiar `gGT->drivers[0]`
y `gGT->drivers[1]` en la máquina del cliente, para que `drivers[0]` =
el jugador local. Entonces el código HUD 1P existente (que dibuja para
`drivers[0]`) muestra las estadísticas correctas. Restaurar después del
dibujado de HUD.

#### T3. Suprimir Advertencia de Mando Desconectado

**Archivos:** `game/MAIN/MainFrame_08_RenderFrame.c` en `DrawUnpluggedMsg()`
(~línea 222)

**Qué hacer:**
Añadir retorno temprano cuando `g_NetplayRacing`:
```c
if (g_NetplayRacing) return;
```
al inicio de `DrawUnpluggedMsg`, o después de las comprobaciones de retorno
temprano existentes.

O, más precisamente, cambiar la comprobación de mandos de
`gGT->numPlyrNextGame` a `1` cuando `g_NetplayRacing`.

### Prioridad 2: Importante para la Jugabilidad

#### T4. Implementar Input Sincronizado por Fotograma

**Archivos:** `platform/native_netplay.c` (Netplay_ReceiveInputs),
`game/MAIN/MainMain.c` (bloque de sincronización de inputs)

**Qué hacer:**
Añadir coincidencia de número de fotograma. `Netplay_ReceiveInputs` debería
tomar un parámetro `currentFrame` y solo devolver inputs cuyo `frameNum`
coincida (o esté cerca) con el fotograma actual. Los inputs para fotogramas
incorrectos se descartan o bufferizan.

Esto requiere:
1. Ambas máquinas acuerdan un fotograma de inicio (enviarlo en el paquete
   START_RACE)
2. Cada máquina rastrea su offset de fotograma desde el inicio
3. Los inputs se etiquetan con el número de fotograma absoluto
4. Al recibir, inputs con frameNum != currentFrame se bufferizan (si están
   adelantados) o descartan (si están atrasados, usar el último input conocido
   como fallback)

#### T5. Sincronizar Inicio de Carrera

**Archivos:** `game/230/230_22_MM_Online_Menu.c` (Online_StartRace),
`platform/native_netplay.c`

**Qué hacer:**
Después de que ambas máquinas terminen de cargar:
1. Cada una envía un paquete "listo" a la otra
2. El host cuenta atrás (ej. 90 fotogramas = 3 segundos) después de recibir
   el "listo" del cliente
3. El host envía broadcast del paquete "GO" con el número de fotograma de inicio
4. Ambas máquinas comienzan la cuenta atrás de la carrera en el mismo fotograma

### Prioridad 3: Pulido y Robustez

#### T6. Sincronización Periódica de Posición

**Archivos:** `platform/native_netplay.c`, `game/MAIN/MainMain.c`

**Qué hacer:**
Cada N fotogramas (ej. 30 = una vez por segundo), broadcast de la posición,
velocidad y rotación de cada piloto. Al recibir, aplicar una corrección si
la diferencia excede un umbral (con interpolación para evitar tirones).

Nuevo tipo de paquete: `NETPLAY_PACKET_POSITION_SYNC` (0x09).

#### T7. Sincronización de Cajas de Objetos

**Archivos:** `platform/native_netplay.c`, código de recogida de cajas
(overlays)

**Qué hacer:**
Cuando un jugador recoge una caja, broadcast del índice de la caja. La
máquina remota marca esa caja como recogida. Los objetos asignados por RNG
necesitan una semilla compartida o que el host transmita el tipo de objeto
otorgado.

#### T8. Sincronizar Final de Carrera / Resultados

**Archivos:** Código de detección de final de carrera,
`platform/native_netplay.c`

**Qué hacer:**
Broadcast de eventos de cruce de línea de meta. El host es autoritario para
las posiciones finales de la carrera. Al desconectarse, el jugador restante
gana automáticamente.

#### T9. Manejar Desconexión en Medio de la Carrera

**Archivos:** `platform/native_netplay.c`

**Qué hacer:**
Cuando `Netplay_HandleDisconnect` se dispara durante `g_NetplayRacing`,
mostrar una notificación y terminar la carrera gracefulmente (o pausar y
ofrecer volver al menú).

### Prioridad 4: Verificación y Limpieza

#### T10. Verificar Robustez del Intercambio de Cámaras

**Archivos:** Funciones de cámara en overlays (226-229),
`namespace_Camera.h`

**Qué hacer:**
Comprobar que todas las funciones de modo de cámara usan
`cDC->driverToFollow` para acceder al piloto, no `cDC->cameraID`. Si algún
código hace `gGT->drivers[cDC->cameraID]`, el enfoque de intercambio se
rompe y necesita cambiarse a intercambiar punteros de pushBuffer en lugar de
driverToFollow.

#### T11. Localización de Audio

**Problema:** Los sonidos de motor, objetos y otros son posicionales basados
en la posición de la cámara. Ambas máquinas reproducen audio para ambos
pilotos (ya que ambos existen en la simulación). El cliente oye el motor del
host como si estuviera cerca.

**Solución:** Filtrar fuentes de audio basándose en si pertenecen al jugador
local o a jugadores cercanos. Esto es de baja prioridad — el audio podría ser
aceptable tal como está.

#### T12. Mejorar Visualización de Ping

**Ubicación:** `230_22_MM_Online_Menu.c`

**Actual:** El estado muestra "Connected" pero no el ping.
**Añadir:** Mostrar pingMs para cada par en el lobby.

---

## Instrucciones de Prueba

1. **Compilar:**
   ```
   cd build
   cmake .. -G "MinGW Makefiles" -DCMAKE_C_FLAGS="-std=c99"
   mingw32-make -j4
   ```

2. **Ejecutar (dos instancias en la misma máquina):**
   ```
   # Terminal 1 (host):
   ./ctr_native.exe --netplay host

   # Terminal 2 (cliente):
   ./ctr_native.exe --netplay connect 127.0.0.1
   ```

3. **Flujo de prueba:**
   - Ambas máquinas llegan al menú ONLINE
   - El host pulsa START cuando ve "Players: 2"
   - Ambos eligen personaje (pueden ser iguales o diferentes)
   - El host elige pista y vueltas, pulsa X
   - La carrera carga en ambas máquinas
   - Ambos coches deberían aparecer en la pista, cada máquina muestra su
     propio piloto a pantalla completa
   - Ambos pilotos responden a sus respectivos inputs
   - La línea divisoria de split-screen NO debería aparecer

4. **Modos de fallo conocidos:**
   - Puede aparecer advertencia de mando desconectado (B3, necesita T3)
   - El cliente puede no ser capaz de pausar (B1, necesita T1)
   - El HUD puede mostrar elementos duplicados/basura (B2, necesita T2)
   - Con el tiempo, los coches pueden separarse en posición (B6, necesita T6)

---

## Resumen de Archivos de Referencia

| Archivo | Propósito |
|---------|-----------|
| `include/platform/native_netplay.h` | API de netplay, structs, globales |
| `platform/native_netplay.c` | Netcode principal (UDP, paquetes, peers) |
| `platform/native_platform.c` | Bucle de plataforma, llama a `Netplay_Poll()` |
| `main.c` | Punto de entrada, args CLI `--netplay` |
| `game/MAIN/MainMain.c` | Bucle principal del juego, bloque de sync de inputs |
| `game/MAIN/MainInit_06_Drivers.c` | Creación de pilotos (omite IA para netplay) |
| `game/MAIN/MainInit_07_FinalizeInit.c` | Inicialización de cámara/pushbuffer, intercambio de cámara |
| `game/MAIN/MainFrame_08_RenderFrame.c` | Renderizado, HUD, líneas de split-screen |
| `game/UI/UI_44_RenderFrame_Racing.c` | Dibujado de HUD en carrera |
| `game/230/230_22_MM_Online_Menu.c` | UI/flujo del menú lobby online |
| `game/230/230_72_MM_JumpTo_Title_FirstTime.c` | Inicialización de título, limpia g_NetplayRacing |
| `game/PushBuffer.c` | Configuración de rectángulos de viewport para 1P/2P/3P/4P |
| `include/namespace_Camera.h` | Definición del struct CameraDC |
| `include/namespace_Main.h` | Definición del struct GameTracker |
