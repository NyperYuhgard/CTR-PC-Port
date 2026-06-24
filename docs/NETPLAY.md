# Netplay

CTR-Native incluye un sistema de netplay UDP peer-to-peer con host autoritativo
para el relay de paquetes. Esta página documenta el protocolo, las nuevas APIs
y cómo extenderlo.

## Estado

Esta versión es la **post-overhaul v2** que arregla los problemas principales del
prototipo inicial y añade sincronización de items + UI ingame completa:

- **Soporte de N jugadores** (hasta `NETPLAY_MAX_PLAYERS = 8`). El loop de
  MainMain.c ya no asume 2 jugadores; llena `gGS->gamepad[0..N-1]` a partir
  de los inputs de cada peer.
- **Relay correcto en partidas de 3+**. El host reenvía `INPUT`, `STATE`,
  `CRATE_HIT`, `CHECKSUM`, `CHARACTER_SELECT`, `CHAT`, `PLAYER_READY`,
  `FINISHED`, `LOADED`, `STATE_REQ`, `ITEM_PICKUP`, `ITEM_USE` a todos los
  clientes excepto al emisor. Sin esto, los clientes no se veían entre sí.
- **Handshake robusto**. El cliente retransmite HELLO cada 500ms hasta
  recibir el accept o agotar 10s de timeout. El host valida `protocolVersion`
  y rechaza clientes con `NETPLAY_REJECT_*`.
- **Timeout de peer**. Si un peer no manda nada en 10s (configurable con
  `Netplay_SetPeerTimeoutMs`), se lo dropea. Previere peers zombies cuando
  alguien pierde WiFi o mata el proceso.
- **Loaded mask por peer** en vez de un bool global. El semáforo solo arranca
  cuando TODOS los peers (incluido el local) mandaron `LOADED`.
- **Pause sync real**. `PAUSE`/`UNPAUSE` ya no solo loguean: setean
  `PAUSE_ALL` en el `gameMode1` remoto.
- **Reset post-carrera**. `Netplay_ResetRaceState()` limpia todos los flags
  per-race sin tocar la conexión, así se puede volver al lobby y jugar otra
  pista sin reconectar.
- **Return-to-lobby**. El host puede mandar `RETURN_LOBBY` para que todos
  vuelvan al lobby (paquete `0x13`).
- **Chat básico en lobby**. `CROSS` abre input, `START` o `CROSS` envía,
  `TRI`/`SQR` cancela.
- **Ready states**. `SQR` togglea ready en el lobby. El host solo puede
  arrancar la carrera cuando `Netplay_IsEveryoneReady()` y hay ≥2 jugadores.
- **Crate ID estable**. `Netplay_ComputeCrateID(levelID, instanceIndex)`
  genera un hash FNV-1a por instancia. El receptor hace match por ID en vez
  de por posición exacta (que fallaba si había dos crates juntos o el local
  ya había movido el crate).
- **Checksum correcto**. El checksum ahora es sobre el **driver remoto
  simulado localmente**, no sobre el propio. Ambas máquinas computan el
  checksum del mismo driver y lo comparan; si difieren, se pide STATE_REQ.
- **`--players N` enforcement**. `Netplay_SetExpectedPlayerCount(N)` en el
  host rechaza HELLOs cuando el lobby llega a N.

### Novedades v2 (sincronización de items + UI ingame)

- **Sincronización de items**. Antes cada máquina generaba items con su
  propio `rand()` y el HUD remoto mostraba el item equivocado. Ahora:
  - `VehPhysGeneral_SetHeldItem` hace broadcast de `(playerId, itemId,
    numHeldItems)` apenas se asigna el item. El receptor lo aplica al
    driver remoto inmediatamente (sin esperar al state packet de 5 frames).
  - `VehPickupItem_ShootOnCirclePress` hace broadcast de `(playerId,
    itemId)` cuando el jugador dispara. El receptor llama
    `VehPickupItem_ShootNow(remoteDriver, itemId, 0)` para que el arma
    también aparezca en su máquina.
  - `Netplay_BroadcastRngSeed()` (host → clients al iniciar la carrera)
    sincroniza la semilla de `sdata->randomNumber` para que los item rolls
    sean deterministas al inicio. La sincronización posterior se mantiene
    vía los broadcasts de ITEM_PICKUP, que son autoritativos.
- **Sincronización de rotación corregida**. Antes el state packet solo
  enviaba `rotCurr.x/y/z`, pero la física de CTR reconstruye `rotCurr.y`
  cada frame como `unk3D4[0] + angle + turnAngleCurr`. Sin sincronizar
  esos tres campos, el snap de rotación se deshacía al frame siguiente.
  Ahora el state payload incluye `angle`, `turnAngleCurr`, `unk3D4[0]` y
  `rotCurr.w`, y la aplicación usa **lerp angular modular** (shortest-path
  en el rango 12-bit `0x000-0xFFF`) en lugar de snap directo, así el kart
  no gira 360° cuando cruza el boundary `0xFFF → 0x000`.
- **Lerp de velocity y speed**. Antes solo se lerpeaba posición, lo que
  hacía que la velocidad del kart remoto se "pelee" con la posición
  corregida. Ahora la velocity también se aproxima suavemente al target.
- **Selección de interfaz ingame**. Ya no hace falta `--interface` desde la
  consola. El menú Online ahora tiene una fase `PICK_ROLE` → `PICK_INTERFACE`
  (host) o `ENTER_HOST_IP` (client). La lista de interfaces se obtiene con
  `Netplay_GetInterfaceList()` que devuelve nombre + IP de cada adaptador.
- **Host/Connect desde el menú**. Tampoco hace falta `--host` o `--connect`
  desde la consola. Todo se gestiona desde el menú Online:
  1. Elegís Host o Connect
  2. Si Host: elegís la interfaz de red de la lista
  3. Si Connect: tipeás la IP del host con el D-pad
  4. Lobby → ready → carrera
- **Teclado virtual para IP**. En vez del input "D-pad cycle + confirm"
  anterior (que no mostraba preview), ahora hay un teclado grid 5×3
  (`0-9 . BKSP CLR`) con la tecla resaltada visible siempre, IP tipeada
  mostrada arriba con cursor parpadeante, y navegación con las 4 flechas.
  `START` o `SQUARE` conecta.
- **Port editable ingame**. L1/R1 en la pantalla de rol cambia el puerto
  (default 14200).
- **Ventana de chat separada**. El chat ya no se tipea con el D-pad del
  controller (tedioso). En su lugar, al entrar al lobby se abre
  automáticamente una **ventana de consola separada** (Windows: nueva
  consola via `AllocConsole`; Linux: usa el terminal existente) donde
  podés escribir con el teclado de tu PC. Los mensajes recibidos se
  muestran tanto en la ventana como en una notificación pequeña ingame.
  La ventana se cierra al salir del menú Online.

## Protocolo v1

Todos los paquetes comparten el header:

```c
struct NetplayPacketHeader {
    u32 magic;          // 'NETP' little-endian = 0x5054454e
    u16 type;           // NetplayPacketType
    u16 flags;          // 0 por ahora
    u8  playerId;       // ID del emisor
    u8  playerCount;    // cantidad de jugadores conectados
    u16 payloadSize;    // tamaño del payload que sigue
};
```

| Tipo | Hex | Dirección | Payload | Notas |
|------|-----|-----------|---------|-------|
| HELLO | 0x01 | C→H, H→C | `NetplayHelloPayload` (C→H) o `NetplayAcceptPayload` (H→C) | Handshake. Retransmitido cada 500ms. |
| INPUT | 0x02 | bidir | `NetplayInputPayload` | Relay en host. |
| PING | 0x03 | bidir | `NetplayPingPongPayload` | Cada 1s. |
| PONG | 0x04 | bidir | `NetplayPingPongPayload` | Echo del ping. |
| DISCONNECT | 0x05 | bidir | (vacío) | El receptor dropea al sender de `s_peers[]`. |
| START_RACE | 0x06 | H→C | (vacío) | Trigger legacy para que los clientes pasen a selección de personaje. |
| CHARACTER_SELECT | 0x07 | bidir | `u8 charId` | Relay en host. |
| TRACK_SELECT | 0x08 | H→C | `u8[2] {trackId, numLaps}` | Solo host → clientes. |
| PAUSE | 0x09 | bidir | (vacío) | Setea `PAUSE_ALL` remoto. |
| UNPAUSE | 0x0A | bidir | (vacío) | Limpia `PAUSE_ALL` remoto. |
| LOADED | 0x0B | bidir | (vacío) | Marca ese peer como loaded. Relay en host. |
| STATE | 0x0C | bidir | `NetplayStatePayload` | Snapshot del driver. Relay en host. |
| CRATE_HIT | 0x0D | bidir | `NetplayCrateHit` | Incluye `crateID`. Relay en host. |
| FINISHED | 0x0E | bidir | `u8 finishedPlayerId` | Relay en host. |
| CHECKSUM | 0x0F | bidir | `NetplayChecksumPayload` (con `driverId`) | Relay en host. |
| STATE_REQ | 0x10 | bidir | (vacío) | Pide al peer un STATE inmediato. Relay en host. |
| CHAT | 0x11 | bidir | `NetplayChatPayload` (64 bytes) | Relay en host. |
| PLAYER_READY | 0x12 | bidir | `u8[2] {playerId, ready}` | Relay en host. |
| RETURN_LOBBY | 0x13 | H→C | (vacío) | Volver al lobby post-carrera. |
| PLAYER_LIST | 0x14 | H→C | `u8 count` + `count * NetplayPlayerListEntry` | Roster completo. |
| REJECT | 0x15 | H→C | `u8 reason` | Rechazo de HELLO. |
| ITEM_PICKUP | 0x16 | bidir | `NetplayItemPayload` | Un jugador obtuvo un item de un crate. Relay en host. |
| ITEM_USE | 0x17 | bidir | `NetplayItemPayload` | Un jugador disparó su arma. Relay en host. |
| RNG_SEED | 0x18 | H→C | `NetplayRngSeedPayload` | Semilla de RNG al iniciar la carrera. |

## APIs nuevas

```c
/* Lobby */
void Netplay_SetLocalReady(int ready);
int  Netplay_IsLocalReady(void);
int  Netplay_IsPeerReady(u8 playerId);
int  Netplay_IsEveryoneReady(void);
void Netplay_BroadcastReturnToLobby(void);
int  Netplay_ConsumeReturnToLobby(void);
void Netplay_SetExpectedPlayerCount(int count);

/* Chat */
void Netplay_SendChat(const char *message);
int  Netplay_DequeueChat(struct NetplayChatPayload *out);

/* Loaded mask */
void Netplay_MarkLocalLoaded(void);
void Netplay_ClearLocalLoaded(void);
int  Netplay_IsLocalLoaded(void);
int  Netplay_IsEveryoneLoaded(void);

/* Pause sync */
void Netplay_BroadcastPause(void);
void Netplay_BroadcastUnpause(void);
int  Netplay_ConsumeRemotePause(void);
int  Netplay_ConsumeRemoteUnpause(void);

/* Reject feedback */
int  Netplay_GetRejectReason(void);
const char *Netplay_GetRejectReasonString(int reason);

/* Post-race reset */
void Netplay_ResetRaceState(void);

/* Crate ID helper */
u32 Netplay_ComputeCrateID(int levelID, int instanceIndex);

/* Peer timeout */
void Netplay_SetPeerTimeoutMs(u32 ms);

/* Names */
const char *Netplay_GetPlayerName(u8 playerId);
const char *Netplay_GetLocalPlayerName(void);

/* Item sync (v2) */
void Netplay_BroadcastItemPickup(u8 playerId, u8 itemId, u8 numHeldItems, u32 frameNum);
void Netplay_BroadcastItemUse(u8 playerId, u8 itemId, u32 frameNum);
int  Netplay_DequeueItemPickup(u8 playerId, u8 *outItemId, u8 *outNumItems);
int  Netplay_DequeueItemUse(u8 playerId, u8 *outItemId);

/* RNG seed (v2) */
void Netplay_BroadcastRngSeed(u32 seed, u32 frameNum);
int  Netplay_ConsumeRngSeed(u32 *outSeed, u32 *outFrameNum);

/* Network interface enumeration (v2) */
int  Netplay_GetInterfaceList(struct NetplayInterface *out, int maxEntries);
const char *Netplay_GetSelectedInterfaceIP(void);
const char *Netplay_GetInterfaceIPByIndex(int index);
```

## Uso

### Flujo recomendado (todo desde el menú ingame)

Ya no hace falta usar la consola para nada. Simplemente:

1. Ejecutá `ctr_native.exe` (sin argumentos).
2. En el main menu, andá a **Online**.
3. Elegí **Host a new game** o **Connect to a host**.
   - Si hosteás: te aparece la lista de interfaces de red. Elegís la que
     tenga tu IP local (la que vas a compartir con los demás).
   - Si conectás: tipeás la IP del host con el D-pad
     (UP/DN cambia el dígito 0-9-., CROSS append, SQUARE backspace,
     START conectar).
4. Llegás al lobby. `SQR` para marcarte ready, `CROSS` para chatear.
5. El host arranca con `START` cuando todos están ready.
6. Al terminar la carrera, el host puede mandar "volver al lobby" para
   jugar otra pista sin reconectar.

### Argumentos CLI (opcionales, para debug/auto-test)

Los args de CLI siguen funcionando para quick testing:

```
ctr_native --host --name "Host" --players 4
ctr_native --connect 192.168.1.100 --name "Player2"
ctr_native --list-interfaces
```

Pero el flujo recomendado para jugadores finales es 100% ingame.

### Ver IP local

Desde el menú Online → Host, la lista de interfaces ya muestra tu IP. No
hace falta usar `--list-interfaces`.

## Pendientes (no implementados aún)

- **Predicción local + reconciliación**: hoy el kart local es autoritativo
  de su propia posición hasta que llega un STATE del peer que la corregiría
  (no lo hace para el driver local, solo para los remotos). Sería ideal
  añadir client-side prediction para el propio driver y reconciliar con
  snapshots del host.
- **Rollback**: `s_inputHistory` ya está reservado pero no se usa. Un
  rollback de N frames requiere guardar estado serializable del GameTracker,
  que es grande.
- **Spec mode post-finish**: cuando un jugador termina, hoy se queda viendo
  su propia cámara. Sería ideal que pueda espectar a los demás.
- **CHARACTER_SELECT con N jugadores**: hoy solo se sincronizan los
  personajes del host y de UN cliente. Para 3+ jugadores, hace falta
  extender el payload con el playerId destino o mantener un mapa
  `playerId -> charId` en el host y broadcastearlo.

## Estructura de archivos

| Archivo | Rol |
|---------|-----|
| `include/platform/native_netplay.h` | API pública, tipos, packet types. |
| `platform/native_netplay.c` | Implementación: socket, peers, handlers, poll. |
| `game/MAIN/MainMain.c` | Loop de carrera: sync de inputs, state, crates, pause, return. |
| `game/MAIN/MainInit_07_FinalizeInit.c` | Swap de cámara + `Netplay_MarkLocalLoaded()`. |
| `game/230/230_22_MM_Online_Menu.c` | Lobby UI: fases, ready, chat, character/track pick. |
| `game/231/231_061_068_RB_Crate.c` | Broadcast de crate hits con `crateID`. |
| `main.c` | CLI args (`--host`, `--connect`, `--name`, `--players`, `--interface`). |
