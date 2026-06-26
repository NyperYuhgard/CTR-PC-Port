#ifndef PLATFORM_NATIVE_NETPLAY_H
#define PLATFORM_NATIVE_NETPLAY_H

#include <macros.h>

#define NETPLAY_MAX_PLAYERS      8
#define NETPLAY_DEFAULT_PORT     14200
#define NETPLAY_FRAME_HISTORY    120

/* Wire protocol version. Bump when packet layouts change incompatibly.
 * The host rejects clients whose version does not match. */
#define NETPLAY_PROTOCOL_VERSION 1

/* Number of frames the input pipeline waits before consuming a remote
 * input. Higher = smoother under packet loss/jitter, but adds latency.
 * 0 disables the delay buffer (legacy behaviour). */
#define NETPLAY_INPUT_DELAY_FRAMES 2

enum NetplayPacketType
{
        NETPLAY_PACKET_HELLO       = 0x01,
        NETPLAY_PACKET_INPUT       = 0x02,
        NETPLAY_PACKET_PING        = 0x03,
        NETPLAY_PACKET_PONG        = 0x04,
        NETPLAY_PACKET_DISCONNECT  = 0x05,
        NETPLAY_PACKET_START_RACE       = 0x06,
        NETPLAY_PACKET_CHARACTER_SELECT = 0x07,
        NETPLAY_PACKET_TRACK_SELECT     = 0x08,
        NETPLAY_PACKET_PAUSE           = 0x09,
        NETPLAY_PACKET_UNPAUSE         = 0x0A,
        NETPLAY_PACKET_LOADED          = 0x0B,
        NETPLAY_PACKET_STATE           = 0x0C,
        NETPLAY_PACKET_CRATE_HIT       = 0x0D,
        NETPLAY_PACKET_FINISHED        = 0x0E,
        NETPLAY_PACKET_CHECKSUM        = 0x0F,
        NETPLAY_PACKET_STATE_REQ       = 0x10,
        /* New in protocol v1 */
        NETPLAY_PACKET_CHAT            = 0x11,
        NETPLAY_PACKET_PLAYER_READY    = 0x12, /* payload: u8 playerId, u8 ready */
        NETPLAY_PACKET_RETURN_LOBBY    = 0x13, /* host -> clients: race over, go back to lobby */
        NETPLAY_PACKET_PLAYER_LIST     = 0x14, /* host -> clients: full roster */
        NETPLAY_PACKET_REJECT          = 0x15, /* host -> client: rejected (version, full, etc) */
        NETPLAY_PACKET_ITEM_PICKUP     = 0x16, /* bidir: a player got an item from a crate */
        NETPLAY_PACKET_ITEM_USE        = 0x17, /* bidir: a player used their item (fired weapon) */
        NETPLAY_PACKET_RNG_SEED        = 0x18, /* host -> clients: deterministic RNG seed */
};

struct NetplayChecksumPayload
{
        u32 frameNum;
        u32 checksum;
        u8  driverId; /* which driver this checksum is about (the remote one, simulated locally) */
};

#define NETPLAY_CRATE_QUEUE_MAX 32

struct NetplayCrateHit
{
        s16 posX, posY, posZ;
        u8  modelID;
        u32 crateID; /* stable hash of (level, instance index) — robust against position collisions */
};

struct NetplayStatePayload
{
        u32 frameNum;
        s16 rotX, rotY, rotZ;
        s16 speed;
        u8  kartState;
        u8  lapIndex;
        u8  heldItemID;
        u8  numHeldItems;
        u32 actionsFlagSet;
        s32 posX, posY, posZ;
        s32 velX, velY, velZ;
        /* New in v2: extra rotation fields that the physics engine uses to
         * reconstruct rotCurr.y = unk3D4[0] + angle + turnAngleCurr.
         * Without these, the remote kart's rotation drifts because the
         * client's local angle/turnAngleCurr keep advancing from inputs
         * while we only snap rotCurr (which gets overwritten next frame). */
        s16 angle;           /* 0x39A — main heading */
        s16 turnAngleCurr;   /* drift/turn offset */
        s16 unk3D4_0;        /* base rotation offset */
        s16 rotW;            /* rotCurr.w — used by camera & some physics */
        /* v2.1: numWumpas is REQUIRED for TNT/Nitro sync. The HUD checks
         * numWumpas >= 10 to decide whether to draw the "powered-up"
         * version of TNT (Nitro), Potion, Shield. Without this, a host
         * with 10 wumpas would see Nitro on his screen while the client
         * (with 0 wumpas) sees TNT — even though both are looking at the
         * same driver. */
        u8  numWumpas;
        s16 noItemTimer;     /* weapon flicker timer — also drives HUD flicker (can be > 255) */
};

enum NetplayState
{
        NETPLAY_STATE_DISCONNECTED,
        NETPLAY_STATE_HOSTING,
        NETPLAY_STATE_CONNECTING,
        NETPLAY_STATE_CONNECTED,
};

enum NetplayRejectReason
{
        NETPLAY_REJECT_NONE             = 0,
        NETPLAY_REJECT_VERSION_MISMATCH = 1,
        NETPLAY_REJECT_SERVER_FULL      = 2,
        NETPLAY_REJECT_BAD_PROTOCOL     = 3,
};

struct NetplayInput
{
        u32 frameNum;
        u8  playerId;
        u32 buttonsHeld;
        u32 buttonsTapped;
        u32 buttonsReleased;
        s16 stickLX;
        s16 stickLY;
        s16 stickRX;
        s16 stickRY;
};

struct NetplayPlayerInfo
{
        u8  id;
        u8  connected;
        u8  ready;
        u16 pingMs;
        u32 lastFrameReceived;
        char name[32];
};

#define NETPLAY_CHAT_MSG_MAX 64

struct NetplayChatPayload
{
        u8  senderId;
        u8  reserved[3];
        char message[NETPLAY_CHAT_MSG_MAX];
};

struct NetplayPlayerListEntry
{
        u8  id;
        u8  ready;
        u16 pingMs;
        char name[32];
};

/* Item pickup / use payloads */
struct NetplayItemPayload
{
        u8  playerId;
        u8  itemId;          /* weapon ID (0..15, 0x10 = rolling, 0xF = none) */
        u8  numHeldItems;    /* for 3x missiles / 3x bombs */
        u8  reserved;
        u32 frameNum;        /* frame on the sender's side when this happened */
};

struct NetplayRngSeedPayload
{
        u32 seed;
        u32 frameNum;
};

/* Network interface entry (for in-game interface picker) */
#define NETPLAY_IFACE_NAME_MAX 64
#define NETPLAY_IFACE_LIST_MAX 16

struct NetplayInterface
{
        char name[NETPLAY_IFACE_NAME_MAX];   /* display name with IP appended (e.g. "eth0 (192.168.1.5)") */
        char ifaceName[NETPLAY_IFACE_NAME_MAX]; /* clean adapter name without IP (e.g. "eth0", "Wi-Fi") */
        u32  ip;                              /* host-byte-order IPv4 */
        char ipString[16];                    /* "192.168.1.5" */
};

typedef void (*NetplayEventFn)(u8 playerId);

int  Netplay_Init(void);
void Netplay_Shutdown(void);

int  Netplay_Host(u16 port);
int  Netplay_Connect(const char *address, u16 port);
void Netplay_Disconnect(void);

int  Netplay_GetState(void);
int  Netplay_GetLocalPlayerId(void);
int  Netplay_GetPlayerCount(void);
int  Netplay_GetExpectedPlayerCount(void); /* from --players N */
const struct NetplayPlayerInfo *Netplay_GetPlayers(int *count);
const char *Netplay_GetPlayerName(u8 playerId);
const char *Netplay_GetLocalPlayerName(void);

void Netplay_SendGamepadState(u32 frameNum, u32 buttonsHeld, u32 buttonsTapped,
                              u32 buttonsReleased, s16 stickLX, s16 stickLY,
                              s16 stickRX, s16 stickRY);
int  Netplay_ReceiveInputs(struct NetplayInput *inputs, int maxInputs);
int  Netplay_ReceiveInputsForFrame(struct NetplayInput *inputs, int maxInputs, u32 expectedFrameNum);
void Netplay_GetLatestRemoteInput(u8 playerId, struct NetplayInput *out);

void Netplay_SetJoinCallback(NetplayEventFn cb);
void Netplay_SetLeaveCallback(NetplayEventFn cb);
void Netplay_SetPlayerName(const char *name);

void Netplay_Poll(void);

void Netplay_BroadcastPacket(u16 type, u16 payloadSize, const void *payload);
void Netplay_SendStatePacket(const struct NetplayStatePayload *state);
int  Netplay_DequeueState(u8 playerId, struct NetplayStatePayload *out);
void Netplay_ClearState(u8 playerId);

void Netplay_QueueCrateHit(const struct NetplayCrateHit *hit);
int  Netplay_DequeueCrateHit(struct NetplayCrateHit *out);

void Netplay_MarkRemoteFinished(u8 playerId);
void Netplay_IsRemoteFinished(u8 playerId);
int  Netplay_AnyRemoteFinished(void);
void Netplay_ClearRemoteFinished(void);

const char *Netplay_GetAddressString(void);

/* Interface selection */
void Netplay_SetInterfaceName(const char *name);
int  Netplay_ListInterfaces(void);

/* ---- Lobby: ready / return-to-lobby ---- */
void Netplay_SetLocalReady(int ready);
int  Netplay_IsLocalReady(void);
int  Netplay_IsEveryoneReady(void); /* all connected peers AND local are ready */
int  Netplay_IsPeerReady(u8 playerId);
void Netplay_BroadcastReturnToLobby(void); /* host tells clients race is over */
int  Netplay_ConsumeReturnToLobby(void);   /* returns 1 once if a return packet arrived */
void Netplay_ForceReturnToLobby(void);     /* sets local return-to-lobby flag (both host & client) */

/* ---- Lobby: chat ---- */
void Netplay_SendChat(const char *message);
int  Netplay_DequeueChat(struct NetplayChatPayload *out);

/* ---- Loaded mask (replaces single bool, supports N players) ---- */
void Netplay_MarkLocalLoaded(void);
void Netplay_ClearLocalLoaded(void);
int  Netplay_IsLocalLoaded(void);
int  Netplay_IsEveryoneLoaded(void); /* local + all active peers */

/* ---- Pause sync ---- */
void Netplay_BroadcastPause(void);
void Netplay_BroadcastUnpause(void);
int  Netplay_ConsumeRemotePause(void);  /* returns 1 once per received PAUSE */
int  Netplay_ConsumeRemoteUnpause(void);

/* ---- Rejection feedback (for client UI) ---- */
int  Netplay_GetRejectReason(void);
const char *Netplay_GetRejectReasonString(int reason);

/* ---- Reset racing state (post-race cleanup) ---- */
void Netplay_ResetRaceState(void);

/* ---- Crate ID helper ---- */
/* Stable per-level hash used as crateID so clients match remote hits
 * even when local position has shifted slightly. */
u32 Netplay_ComputeCrateID(int levelID, int instanceIndex);

/* ---- Peer timeout configuration ---- */
void Netplay_SetPeerTimeoutMs(u32 ms);

/* ---- Lobby size configuration ---- */
/* Host: set the max number of players allowed in the lobby (including host).
 * 0 = unlimited (up to NETPLAY_MAX_PLAYERS). The host rejects HELLOs once
 * the count is reached. */
void Netplay_SetExpectedPlayerCount(int count);

/* ---- Local-player-count helper ---- */
/* Returns 1 during a netplay race (each instance is single-player locally,
 * even though numPlyrCurrGame may be 2-8 to represent the connected peers).
 * Returns gGT->numPlyrCurrGame otherwise. Use this INSTEAD of reading
 * numPlyrCurrGame directly when the engine needs to know "how many local
 * viewports am I rendering" — for camera, LOD, pushBuffer, JitPool sizing,
 * HUD layout, etc. Without this, the engine enables split-screen and
 * low-poly LOD when it shouldn't in netplay. */
int Netplay_GetLocalPlayerCount(void);

/* Convenience macro: use NUM_LOCAL_PLAYERS(gGT) wherever the engine used
 * to read gGT->numPlyrCurrGame for rendering/camera/LOD decisions. */
#define NUM_LOCAL_PLAYERS(gGT) \
        (g_NetplayRacing ? 1 : (gGT)->numPlyrCurrGame)

/* ---- Item sync ---- */
/* Called by the game (VehPhysGeneral_SetHeldItem hook) right after a driver
 * picks up an item from a crate. Broadcasts (playerId, itemId, numHeldItems)
 * to all peers so they can set the remote driver's item immediately, without
 * waiting for the next state packet (which only ships every 5 frames). */
void Netplay_BroadcastItemPickup(u8 playerId, u8 itemId, u8 numHeldItems, u32 frameNum);

/* Called by the game (VehPickupItem_ShootOnCirclePress hook) right before a
 * driver fires their weapon. Broadcasts (playerId, itemId) so peers can
 * trigger the same ShootNow on the remote driver. */
void Netplay_BroadcastItemUse(u8 playerId, u8 itemId, u32 frameNum);

/* Game-side consumer: returns 1 if there's a pending item-pickup event for
 * the given playerId, fills outItemId/outNumItems. The caller is expected
 * to apply it to the remote driver's heldItemID/numHeldItems. */
int Netplay_DequeueItemPickup(u8 playerId, u8 *outItemId, u8 *outNumItems);

/* Game-side consumer: returns 1 if there's a pending item-use event for the
 * given playerId, fills outItemId. The caller is expected to call
 * VehPickupItem_ShootNow(remoteDriver, itemId, 0). */
int Netplay_DequeueItemUse(u8 playerId, u8 *outItemId);

/* ---- RNG seed (host -> clients at race start) ---- */
/* Host: pick a seed and broadcast it. Clients should call this with the
 * received seed to set sdata->randomNumber before the first item roll. */
void Netplay_BroadcastRngSeed(u32 seed, u32 frameNum);
int  Netplay_ConsumeRngSeed(u32 *outSeed, u32 *outFrameNum);

/* ---- Chat window (separate OS window, replaces ingame chat input) ---- */
/* Opens a separate SDL window where the player can type chat messages
 * and see incoming messages. Uses SDL_StartTextInput for native keyboard
 * input. The game's main event loop must forward SDL events to
 * Netplay_HandleSDLEvent() so the chat window can process its own
 * keyboard / window events without stealing from the game. */
int  Netplay_OpenChatWindow(void);
void Netplay_CloseChatWindow(void);
int  Netplay_IsChatWindowOpen(void);
/* Write a line to the chat window. Safe to call even if the window is
 * not open (no-op). */
void Netplay_ChatPrint(const char *text);
/* Process an SDL event. Returns 1 if the event was for the chat window
 * and was consumed (the caller should NOT process it further), 0 if the
 * event is not relevant to the chat window. The game's main event loop
 * should call this for every event from SDL_PollEvent. */
int  Netplay_HandleSDLEvent(const void *sdlEvent);

/* ---- Network interface enumeration (for in-game picker) ---- */
/* Returns the number of interfaces found and fills out[] up to maxEntries.
 * Use this from the UI to render a selectable list. */
int Netplay_GetInterfaceList(struct NetplayInterface *out, int maxEntries);

/* Returns the IP string ("192.168.1.5") of the currently-selected interface,
 * or NULL if none. */
const char *Netplay_GetSelectedInterfaceIP(void);

/* Returns the IP string for interface[index] from the last GetInterfaceList
 * call. Index must be < the count returned by GetInterfaceList. */
const char *Netplay_GetInterfaceIPByIndex(int index);

/* Set the displayed address string directly (skips DNS/hostname resolution).
 * Use this after picking an interface in the UI so Netplay_GetAddressString()
 * returns the actual interface IP instead of falling back to gethostname()
 * which on Linux yields 127.0.1.1. */
void Netplay_SetAddressString(const char *ipString, u16 port);

extern int g_NetplayAutoJoin;
extern int g_NetplayRaceStarting;
extern int g_NetplayRacing;
extern int g_NetplayHostCharacter;
extern int g_NetplayClientCharacter;
extern int g_NetplayTrackId;
extern int g_NetplayNumLaps;
extern int g_NetplayLocalLoaded;
extern int g_NetplayRemoteLoaded; /* legacy: 1 when every peer is loaded */
extern int g_NetplayDisconnected;
extern int g_NetplayStateRequested;
extern u32 g_NetplayRemoteChecksumFrame;
extern u32 g_NetplayRemoteChecksumValue;
extern u32 g_NetplayReadyMask;       /* bitmask of peers that signalled ready */
extern int  g_NetplayReturnToLobby;  /* flag set when host says go back to lobby */

#endif
