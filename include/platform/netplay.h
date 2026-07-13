#ifndef PLATFORM_NETPLAY_H
#define PLATFORM_NETPLAY_H

#include <stdint.h>
#include <stddef.h>

/* Use standard C99 types throughout. This header is self-contained —
 * no dependency on the game's type system. */

/*===========================================================================
 * Netplay Protocol — based on CTR-SDK-Mods OnlineCTR (global.h)
 *
 * Architecture:
 *   - Hybrid: dedicated server OR peer-to-peer (host/client)
 *   - Transport: ENet (UDP with reliability/sequencing)
 *   - State machine: 11 states (SDK-adapted for native)
 *   - Sync: per-frame EverythingKart packets (unreliable)
 *   - State transitions: reliable packets
 *===========================================================================*/

/*──────────────────────────────────────────────
 * Constants
 *──────────────────────────────────────────────*/
#ifndef NETPLAY_NAME_LEN
#define NETPLAY_NAME_LEN        9
#endif
#ifndef NETPLAY_MAX_PLAYERS
#define NETPLAY_MAX_PLAYERS     8
#endif
#define NETPLAY_MAX_ROOMS       16
#define NETPLAY_MAX_LAPS        7
#ifndef NETPLAY_DEFAULT_PORT
#define NETPLAY_DEFAULT_PORT    14200
#endif
#ifndef NETPLAY_PROTOCOL_VERSION
#define NETPLAY_PROTOCOL_VERSION 1
#endif
#define NETPLAY_DISCONNECT_UNSYNCED_FRAMES 120

/* Packet buffer sizes */
#ifndef NETPLAY_MAX_PACKET_SIZE
#define NETPLAY_MAX_PACKET_SIZE 1400
#endif
#ifndef NETPLAY_PLAYER_NAME_MAX
#define NETPLAY_PLAYER_NAME_MAX 32
#endif
#ifndef NETPLAY_VERSION_STRING_MAX
#define NETPLAY_VERSION_STRING_MAX 16
#endif

/* ENet channels */
#define NETPLAY_CHAN_RELIABLE   0
#define NETPLAY_CHAN_UNRELIABLE 1

/*──────────────────────────────────────────────
 * Connection mode
 *──────────────────────────────────────────────*/
enum NetplayMode
{
        NETPLAY_MODE_DISABLED,        /* netplay off */
        NETPLAY_MODE_HOST,            /* P2P host (acts as server + client) */
        NETPLAY_MODE_CLIENT,          /* connects to host or dedicated server */
        NETPLAY_MODE_DEDICATED_SERVER,/* standalone server process */
};

/*──────────────────────────────────────────────
 * Client State Machine (SDK-adapted)
 *──────────────────────────────────────────────*/
enum NetplayState
{
        /* Launch / Connection */
        NETPLAY_STATE_LAUNCH_PICK_SERVER  = 0, /* country/region selection */
        NETPLAY_STATE_LAUNCH_CONNECTING   = 1, /* connecting to host/server */

        /* Lobby */
        NETPLAY_STATE_LOBBY_ASSIGN_ROLE   = 2, /* host or guest? */
        NETPLAY_STATE_LOBBY_HOST_SETUP    = 3, /* track/lap selection (host) */
        NETPLAY_STATE_LOBBY_GUEST_WAIT    = 4, /* waiting for host (guest) */
        NETPLAY_STATE_LOBBY_CHARACTER     = 5, /* character selection */
        NETPLAY_STATE_LOBBY_WAIT_LOADING  = 6, /* waiting for all to load */

        /* Race */
        NETPLAY_STATE_LOADING             = 7, /* loading track */
        NETPLAY_STATE_GAME_WAIT_RACE      = 8, /* waiting for race start */
        NETPLAY_STATE_GAME_RACE           = 9, /* racing */
        NETPLAY_STATE_GAME_END_RACE       = 10,/* race finished */

        NETPLAY_STATE_DISCONNECTED        = 11,
        NETPLAY_STATE_ERROR               = 12,
};

/*──────────────────────────────────────────────
 * Server-to-Client Packet Types (SG_)
 *──────────────────────────────────────────────*/
enum NetplayServerMsg
{
        SG_ROOMS          = 0,  /* room list for browser */
        SG_NEWCLIENT      = 1,  /* assigned client ID */
        SG_NAME           = 2,  /* player name broadcast */
        SG_TRACK          = 3,  /* track + lap chosen */
        SG_CHARACTER      = 4,  /* character selection broadcast */
        SG_STARTLOADING   = 5,  /* host: start loading track */
        SG_STARTRACE      = 6,  /* all loaded: start race */
        SG_RACEDATA       = 7,  /* per-frame kart state (unreliable) */
        SG_WEAPON         = 8,  /* weapon fire relay */
        SG_ENDRACE        = 9,  /* race results */
        SG_SERVERCLOSED   = 10, /* server shutting down */
        SG_CHAT           = 11, /* chat message */
        SG_PLAYERLIST     = 12, /* full player roster */
        SG_KICK           = 13, /* player kicked */
        SG_COUNT,
};

/*──────────────────────────────────────────────
 * Client-to-Server Packet Types (CG_)
 *──────────────────────────────────────────────*/
enum NetplayClientMsg
{
        CG_JOINROOM       = 0,  /* request to join a room */
        CG_NAME           = 1,  /* set player name */
        CG_TRACK          = 2,  /* host: set track/lap */
        CG_CHARACTER      = 3,  /* set character choice */
        CG_STARTRACE      = 4,  /* ready to start race */
        CG_LOADINGDONE    = 5,  /* finished loading track */
        CG_RACEDATA       = 6,  /* per-frame kart state (unreliable) */
        CG_WEAPON         = 7,  /* weapon fire */
        CG_ENDRACE        = 8,  /* race results */
        CG_CHAT           = 9,  /* chat message */
        CG_PING           = 10, /* ping/pong */
        CG_COUNT,
};

/*──────────────────────────────────────────────
 * Bit-packed Packet Structures
 *──────────────────────────────────────────────*/

/* ---- SG_MessageRooms (12 bytes) ---- */
struct SG_MessageRooms
{
        uint8_t type;           /* SG_ROOMS */
        uint8_t numRooms;
        uint8_t version;
        uint8_t roomClients[NETPLAY_MAX_ROOMS]; /* 1 byte per room */
} __attribute__((packed));

/* ---- SG_MessageClientStatus / SG_NEWCLIENT (2 bytes) ---- */
struct SG_NewClient
{
        uint8_t type;           /* SG_NEWCLIENT */
        uint8_t clientID : 4;   /* assigned player ID */
        uint8_t numClients : 4; /* total players in room */
} __attribute__((packed));

/* ---- SG_MessageName / CG_Name (12 bytes) ---- */
struct SG_PlayerName
{
        uint8_t type;           /* SG_NAME */
        uint8_t clientID : 4;
        uint8_t numClients : 4;
        char name[NETPLAY_NAME_LEN];
} __attribute__((packed));

/* ---- SG_MessageTrack / CG_Track (2 bytes) ---- */
struct SG_TrackSelect
{
        uint8_t type;           /* SG_TRACK */
        uint8_t trackID : 5;
        uint8_t lapCount : 3;   /* 0-7 -> numLaps = 1+lapCount*2 or custom table */
} __attribute__((packed));

/* ---- SG_MessageCharacter / CG_Character (3 bytes) ---- */
struct SG_CharacterSelect
{
        uint8_t type;           /* SG_CHARACTER */
        uint8_t clientID : 3;
        uint8_t lockedIn : 1;
        uint8_t charID : 4;
        uint8_t engineID;
} __attribute__((packed));

/* ---- EverythingKart (10 bytes) - per-frame sync ---- */
struct EverythingKart
{
        uint8_t header[2];
        /* header[0]: type(4), wumpaCount(3), boolReserves(1) */
        /* header[1]: clientID(3), kartRot1(5) */
        uint8_t kartRot2;
        uint8_t buttonHold;
        int16_t posX;
        int16_t posY;
        int16_t posZ;
} __attribute__((packed));

#define EVERYTHINGKART_SET_HDR0(wumpa, reserves) \
        (((wumpa) & 7) | (((reserves) & 1) << 3))
#define EVERYTHINGKART_SET_HDR1(pid, rot1) \
        (((pid) & 7) | (((rot1) & 0x1F) << 3))

/* ---- SG_Weapon / CG_Weapon (2 bytes) ---- */
struct SG_WeaponUse
{
        uint8_t type;           /* SG_WEAPON */
        uint8_t clientID : 3;
        uint8_t juiced : 1;
        uint8_t weaponID : 4;
} __attribute__((packed));

/* ---- SG_EndRace / CG_EndRace (10 bytes) ---- */
struct SG_EndRace
{
        uint8_t type;           /* SG_ENDRACE */
        uint8_t clientID : 4;
        uint8_t padding : 4;
        uint16_t courseTime;
        uint16_t bestLapTime;
        uint16_t posX;
        uint16_t posZ;
} __attribute__((packed));

/* ---- SG_Chat / CG_Chat (variable) ---- */
struct SG_Chat
{
        uint8_t type;
        uint8_t clientID;
        char message[64];
} __attribute__((packed));

/* ---- SG_Kick (2 bytes) ---- */
struct SG_Kick
{
        uint8_t type;           /* SG_KICK */
        uint8_t clientID;
} __attribute__((packed));

/* ---- CG_JoinRoom (2 bytes) ---- */
struct CG_JoinRoom
{
        uint8_t type;           /* CG_JOINROOM */
        uint8_t roomIndex;
} __attribute__((packed));

/* ---- CG_Ping (1 byte) ---- */
struct CG_Ping
{
        uint8_t type;           /* CG_PING */
} __attribute__((packed));

/*──────────────────────────────────────────────
 * Packet creation helpers (inline)
 *──────────────────────────────────────────────*/
static inline void packet_set_header(void *pkt, int type)
{
        *(uint8_t *)pkt = (uint8_t)type;
}

static inline int packet_get_type(const void *pkt)
{
        return *(const uint8_t *)pkt;
}

/*──────────────────────────────────────────────
 * Room configuration
 *──────────────────────────────────────────────*/
enum NetplayRoomType
{
        ROOM_ITEMLESS   = 0,
        ROOM_ITEMS      = 5,
        ROOM_RETRO      = 10,
        ROOM_ITEMRETRO  = 13,
};

#define ROOM_IS_ITEMS(rn)  ((rn) >= ROOM_ITEMS && (rn) < ROOM_RETRO)
#define ROOM_IS_RETRO(rn)  ((rn) >= ROOM_RETRO)

/*──────────────────────────────────────────────
 * Player info (runtime state, not wire)
 *──────────────────────────────────────────────*/
struct NetplayPlayer
{
        uint8_t  id;
        uint8_t  connected;
        uint8_t  ready;
        uint8_t  characterID;
        uint8_t  engineID;
        uint16_t pingMs;
        char name[NETPLAY_NAME_LEN + 1];
};

#endif /* PLATFORM_NETPLAY_H */
