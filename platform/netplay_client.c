/*===========================================================================
 * CTR-Netplay Client — ENet transport + protocol layer
 *
 * Internal module, designed to be #include'd from native_netplay.c.
 * Provides the new ENet-based transport, state machine, and packet protocol
 * without conflicting with the public native_netplay.h API.
 *
 * All public-facing Netplay_* functions remain in native_netplay.c and call
 * into these internal helpers.
 *===========================================================================*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <enet/enet.h>

/* Reuse the protocol definitions from netplay.h */
#include <platform/netplay.h>

/*──────────────────────────────────────────────
 * Packet helpers
 *──────────────────────────────────────────────*/
/* Extract the 4-bit packet type from the first byte (also defined inline
 * in netplay.h, but redefined here as a true function for internal use) */
static inline uint8_t _np_pkt_type(const uint8_t *data)
{
        return data[0] & 0x0F;
}

static inline void _np_pkt_set_type(uint8_t *data, uint8_t type)
{
        data[0] = (data[0] & 0xF0) | (type & 0x0F);
}

/*──────────────────────────────────────────────
 * Internal state
 *──────────────────────────────────────────────*/
struct _NetplayState
{
        /* ENet */
        ENetHost   *host;
        ENetPeer   *serverPeer;   /* connected peer */

        /* Mode */
        enum NetplayMode mode;

        /* State machine */
        enum NetplayState state;
        int            localPlayerId;
        int            playerCount;

        /* Player roster */
        struct NetplayPlayer players[NETPLAY_MAX_PLAYERS];
        char               localName[NETPLAY_NAME_LEN + 1];

        /* Track / character */
        uint8_t pendingTrackId;
        uint8_t pendingLapCount;
        uint8_t pendingCharId;
        int     pendingCharLocked;

        /* Load / race sync */
        int localLoaded;
        int everyoneLoaded;
        int everyoneRacing;

        /* Kart state ring buffers (per remote player) */
#define NP_KART_QUEUE_MAX 16
        struct EverythingKart kartQueue[NETPLAY_MAX_PLAYERS][NP_KART_QUEUE_MAX];
        int  kartQueueHead[NETPLAY_MAX_PLAYERS];
        int  kartQueueTail[NETPLAY_MAX_PLAYERS];

        /* Weapon queue */
        struct {
                uint8_t playerId;
                uint8_t weaponId;
                uint8_t juiced;
                int     valid;
        } weaponQueue[8];
        int weaponQueueHead;
        int weaponQueueTail;

        /* Chat queue */
        char chatQueue[8][64];
        int  chatQueueHead;
        int  chatQueueTail;

        /* Return-to-lobby flag */
        int returnToLobby;

        /* Server address string (for display) */
        char serverAddress[64];
};

/* Single instance */
static struct _NetplayState s_np;

/* Forward declarations */
static void _np_disconnect(void);
static void _np_poll(void);
static void _np_push_raw(const uint8_t *data, size_t len, ENetPeer *peer);

/*──────────────────────────────────────────────
 * Send helpers
 *──────────────────────────────────────────────*/
static int _np_send_reliable(ENetPeer *peer, const void *data, size_t size)
{
        if (!peer) return -1;
        ENetPacket *pkt = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
        return enet_peer_send(peer, NETPLAY_CHAN_RELIABLE, pkt);
}

static int _np_send_unreliable(ENetPeer *peer, const void *data, size_t size)
{
        if (!peer) return -1;
        ENetPacket *pkt = enet_packet_create(data, size, ENET_PACKET_FLAG_UNSEQUENCED);
        return enet_peer_send(peer, NETPLAY_CHAN_UNRELIABLE, pkt);
}

/*──────────────────────────────────────────────
 * State machine helpers
 *──────────────────────────────────────────────*/
static void _np_set_state(enum NetplayState newState)
{
        s_np.state = newState;
}

/*──────────────────────────────────────────────
 * Process incoming packets
 *──────────────────────────────────────────────*/
static void _np_process_packet(ENetPacket *packet)
{
        uint8_t type = _np_pkt_type(packet->data);
        size_t len = packet->dataLength;

        switch (type)
        {
        case SG_ROOMS: {
                if (len < sizeof(struct SG_MessageRooms)) break;
                struct SG_MessageRooms *rooms = (struct SG_MessageRooms *)packet->data;
                printf("[Netplay] Rooms: version=%d count=%d\n",
                       rooms->version, rooms->numRooms);

                /* Find first room with space and join it */
                for (int r = 0; r < NETPLAY_MAX_ROOMS && r < rooms->numRooms; r++)
                {
                        if (rooms->roomClients[r] < NETPLAY_MAX_PLAYERS)
                        {
                                struct CG_JoinRoom join;
                                join.type = CG_JOINROOM;
                                join.roomIndex = (uint8_t)r;
                                _np_send_reliable(s_np.serverPeer, &join, sizeof(join));
                                break;
                        }
                }
                break;
        }

        case SG_NEWCLIENT: {
                if (len < sizeof(struct SG_NewClient)) break;
                struct SG_NewClient *nc = (struct SG_NewClient *)packet->data;
                s_np.localPlayerId = nc->clientID;
                s_np.playerCount = nc->numClients;
                printf("[Netplay] Assigned ID=%d, room has %d player(s)\n",
                       nc->clientID, nc->numClients);

                /* Bridge to legacy globals */
                s_localPlayerId = nc->clientID;
                s_playerCount = nc->numClients;
                g_NetplayRemoteLoaded = 0;
                g_NetplayReturnToLobby = 0;

                /* Send our name */
                struct SG_PlayerName nameMsg;
                memset(&nameMsg, 0, sizeof(nameMsg));
                nameMsg.type = CG_NAME;
                nameMsg.clientID = (uint8_t)nc->clientID;
                strncpy(nameMsg.name, s_np.localName, NETPLAY_NAME_LEN);
                _np_send_reliable(s_np.serverPeer, &nameMsg, sizeof(nameMsg));

                _np_set_state(NETPLAY_STATE_LOBBY_ASSIGN_ROLE);
                break;
        }

        case SG_NAME: {
                if (len < sizeof(struct SG_PlayerName)) break;
                struct SG_PlayerName *nm = (struct SG_PlayerName *)packet->data;
                int pid = nm->clientID;
                if (pid >= 0 && pid < NETPLAY_MAX_PLAYERS) {
                        strncpy(s_np.players[pid].name, nm->name, NETPLAY_NAME_LEN);
                        s_np.players[pid].name[NETPLAY_NAME_LEN] = 0;
                        s_np.players[pid].id = pid;
                        s_np.players[pid].connected = 1;
                }
                /* Update player count from server's authoritative count */
                s_np.playerCount = nm->numClients;
                s_playerCount = nm->numClients;
                if (s_np.localPlayerId == 0 &&
                    s_np.state == NETPLAY_STATE_LOBBY_ASSIGN_ROLE)
                        _np_set_state(NETPLAY_STATE_LOBBY_HOST_SETUP);
                break;
        }

        case SG_TRACK: {
                if (len < sizeof(struct SG_TrackSelect)) break;
                struct SG_TrackSelect *ts = (struct SG_TrackSelect *)packet->data;
                s_np.pendingTrackId = ts->trackID;
                s_np.pendingLapCount = ts->lapCount;
                /* Bridge to legacy globals */
                g_NetplayTrackId = ts->trackID;
                g_NetplayNumLaps = ts->lapCount;
                /* Non-host clients: signal that character selection can begin.
                 * The host drives the menu locally and must NOT re-enter
                 * character selection upon receiving its own broadcast. */
                if (s_np.localPlayerId != 0)
                {
                        g_NetplayRaceStarting = 1;
                        _np_set_state(NETPLAY_STATE_LOBBY_CHARACTER);
                }
                printf("[Netplay] Track set: %d, laps=%d (localId=%d)\n",
                       ts->trackID, ts->lapCount, s_np.localPlayerId);
                break;
        }

        case SG_CHARACTER: {
                if (len < sizeof(struct SG_CharacterSelect)) break;
                struct SG_CharacterSelect *cs = (struct SG_CharacterSelect *)packet->data;
                int pid = cs->clientID;
                if (pid >= 0 && pid < NETPLAY_MAX_PLAYERS) {
                        s_np.players[pid].characterID = cs->charID;
                        s_np.players[pid].engineID = cs->engineID;
                        /* Bridge to legacy globals */
                        g_NetplayCharacters[pid] = cs->charID;
                        g_NetplayEngines[pid] = cs->engineID;
                }
                break;
        }

        case SG_STARTLOADING: {
                printf("[Netplay] Server says: start loading\n");
                g_NetplayLocalLoaded = 0;
                g_NetplayStartRaceRequested = 1;
                _np_set_state(NETPLAY_STATE_LOADING);
                break;
        }

        case SG_STARTRACE: {
                printf("[Netplay] Server says: start race!\n");
                s_np.everyoneRacing = 1;
                g_NetplayRacing = 1;
                g_NetplayRemoteLoaded = 1;
                /* Mark all peers as loaded so Netplay_IsEveryoneLoaded() works */
                for (int i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                        if (s_peers[i].active)
                                s_peers[i].loaded = 1;
                _np_set_state(NETPLAY_STATE_GAME_RACE);
                break;
        }

        case SG_RACEDATA: {
                if (len < sizeof(struct EverythingKart)) break;
                struct EverythingKart *ek = (struct EverythingKart *)packet->data;
                uint8_t pid = ek->header[1] & 7;
                int tail = s_np.kartQueueTail[pid];
                int next = (tail + 1) % NP_KART_QUEUE_MAX;
                if (next != s_np.kartQueueHead[pid]) {
                        memcpy(&s_np.kartQueue[pid][tail], ek, sizeof(*ek));
                        s_np.kartQueueTail[pid] = next;
                }
                break;
        }

        case SG_WEAPON: {
                if (len < sizeof(struct SG_WeaponUse)) break;
                struct SG_WeaponUse *wp = (struct SG_WeaponUse *)packet->data;
                int tail = (s_np.weaponQueueTail + 1) % 8;
                if (tail != s_np.weaponQueueHead) {
                        s_np.weaponQueue[s_np.weaponQueueTail].playerId = wp->clientID;
                        s_np.weaponQueue[s_np.weaponQueueTail].weaponId  = wp->weaponID;
                        s_np.weaponQueue[s_np.weaponQueueTail].juiced    = wp->juiced;
                        s_np.weaponQueue[s_np.weaponQueueTail].valid     = 1;
                        s_np.weaponQueueTail = tail;
                }
                break;
        }

        case SG_ENDRACE: {
                /* Bridge to legacy globals */
                g_NetplayReturnToLobby = 1;
                g_NetplayRacing = 0;
                break;
        }

        case SG_SERVERCLOSED:
                printf("[Netplay] Server closed connection\n");
                _np_set_state(NETPLAY_STATE_DISCONNECTED);
                break;

        default:
                break;
        }
}

/*──────────────────────────────────────────────
 * Poll ENet events
 *──────────────────────────────────────────────*/
static void _np_poll(void)
{
        if (!s_np.host) return;

        ENetEvent ev;
        while (enet_host_service(s_np.host, &ev, 0) > 0)
        {
                switch (ev.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                        printf("[Netplay] ENet connected\n");
                        break;

                case ENET_EVENT_TYPE_DISCONNECT:
                        printf("[Netplay] ENet disconnected\n");
                        s_np.serverPeer = NULL;
                        _np_set_state(NETPLAY_STATE_DISCONNECTED);
                        break;

                case ENET_EVENT_TYPE_RECEIVE:
                        /* Push to legacy raw queue */
                        _np_push_raw(ev.packet->data, ev.packet->dataLength, ev.peer);
                        /* Process SDK protocol packets */
                        _np_process_packet(ev.packet);
                        enet_packet_destroy(ev.packet);
                        break;

                default:
                        break;
                }
        }
}

/*──────────────────────────────────────────────
 * Initialization / shutdown (idempotent)
 *──────────────────────────────────────────────*/
int _np_init(void)
{
        memset(&s_np, 0, sizeof(s_np));
        s_np.state = NETPLAY_STATE_DISCONNECTED;
        s_np.mode = NETPLAY_MODE_DISABLED;
        s_np.localPlayerId = -1;

        if (enet_initialize() != 0) {
                fprintf(stderr, "[Netplay] enet_initialize() failed\n");
                return -1;
        }
        printf("[Netplay] ENet initialized (protocol v%d)\n", NETPLAY_PROTOCOL_VERSION);
        return 0;
}

void _np_shutdown(void)
{
        _np_disconnect();

        if (s_np.host) {
                enet_host_destroy(s_np.host);
                s_np.host = NULL;
        }
        enet_deinitialize();
        memset(&s_np, 0, sizeof(s_np));
        printf("[Netplay] Shutdown\n");
}

/*──────────────────────────────────────────────
 * Host / Connect / Disconnect
 *──────────────────────────────────────────────*/
int _np_host(uint16_t port)
{
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        s_np.host = enet_host_create(&address, NETPLAY_MAX_PLAYERS, 2, 0, 0);
        if (!s_np.host) {
                fprintf(stderr, "[Netplay] enet_host_create (host) failed on port %d\n", port);
                return -1;
        }

        s_np.mode = NETPLAY_MODE_HOST;
        s_np.localPlayerId = 0;
        s_np.playerCount = 1;
        memset(s_np.players, 0, sizeof(s_np.players));
        s_np.players[0].id = 0;
        s_np.players[0].connected = 1;
        strncpy(s_np.players[0].name, s_np.localName, NETPLAY_NAME_LEN);

        _np_set_state(NETPLAY_STATE_LOBBY_ASSIGN_ROLE);
        printf("[Netplay] Hosting on port %u (P2P)\n", (unsigned)port);
        return 0;
}

int _np_connect(const char *address, uint16_t port)
{
        s_np.host = enet_host_create(NULL, 1, 2, 0, 0);
        if (!s_np.host) {
                fprintf(stderr, "[Netplay] enet_host_create (client) failed\n");
                return -1;
        }

        ENetAddress addr;
        if (enet_address_set_host(&addr, address) != 0) {
                fprintf(stderr, "[Netplay] Cannot resolve: %s\n", address);
                enet_host_destroy(s_np.host);
                s_np.host = NULL;
                return -1;
        }
        addr.port = port;

        s_np.serverPeer = enet_host_connect(s_np.host, &addr, 2, 0);
        if (!s_np.serverPeer) {
                fprintf(stderr, "[Netplay] enet_host_connect failed\n");
                enet_host_destroy(s_np.host);
                s_np.host = NULL;
                return -1;
        }

        ENetEvent ev;
        if (enet_host_service(s_np.host, &ev, 5000) > 0 &&
            ev.type == ENET_EVENT_TYPE_CONNECT)
        {
                s_np.mode = NETPLAY_MODE_CLIENT;
                strncpy(s_np.serverAddress, address, sizeof(s_np.serverAddress) - 1);
                _np_set_state(NETPLAY_STATE_LAUNCH_CONNECTING);
                printf("[Netplay] Connected to %s:%u\n", address, (unsigned)port);
                return 0;
        }

        enet_peer_reset(s_np.serverPeer);
        s_np.serverPeer = NULL;
        enet_host_destroy(s_np.host);
        s_np.host = NULL;
        fprintf(stderr, "[Netplay] Connection TIMEOUT to %s:%u\n", address, (unsigned)port);
        return -1;
}

void _np_disconnect(void)
{
        if (s_np.serverPeer) {
                enet_peer_disconnect(s_np.serverPeer, 0);
                ENetEvent ev;
                memset(&ev, 0, sizeof(ev));
                while (enet_host_service(s_np.host, &ev, 3000) > 0) {
                        if (ev.type == ENET_EVENT_TYPE_DISCONNECT) break;
                        if (ev.packet) enet_packet_destroy(ev.packet);
                }
                if (ev.type != ENET_EVENT_TYPE_DISCONNECT)
                        enet_peer_reset(s_np.serverPeer);
                s_np.serverPeer = NULL;
        }

        if (s_np.host && s_np.mode != NETPLAY_MODE_HOST) {
                enet_host_destroy(s_np.host);
                s_np.host = NULL;
        }

        memset(&s_np, 0, sizeof(s_np));
        s_np.state = NETPLAY_STATE_DISCONNECTED;
        s_np.mode = NETPLAY_MODE_DISABLED;
        s_np.localPlayerId = -1;
}

/*──────────────────────────────────────────────
 * Raw packet queue (bridge to legacy code)
 *──────────────────────────────────────────────*/
#define NP_RAW_QUEUE_SIZE 256

struct RawPacket {
        uint8_t data[NETPLAY_MAX_PACKET_SIZE];
        size_t  len;
        ENetPeer *fromPeer;  /* originating peer (NULL if unknown) */
};

static struct RawPacket s_rawQueue[NP_RAW_QUEUE_SIZE];
static volatile int     s_rawQueueHead;
static volatile int     s_rawQueueTail;

static void _np_push_raw(const uint8_t *data, size_t len, ENetPeer *peer)
{
        int tail = s_rawQueueTail;
        int next = (tail + 1) % NP_RAW_QUEUE_SIZE;
        if (next == s_rawQueueHead) {
                /* Drop oldest */
                s_rawQueueHead = (s_rawQueueHead + 1) % NP_RAW_QUEUE_SIZE;
        }
        size_t copyLen = len < sizeof(s_rawQueue[tail].data) ? len : sizeof(s_rawQueue[tail].data);
        memcpy(s_rawQueue[tail].data, data, copyLen);
        s_rawQueue[tail].len = copyLen;
        s_rawQueue[tail].fromPeer = peer;
        s_rawQueueTail = next;
}

int _np_recv_raw(uint8_t *buf, size_t bufSize, size_t *outLen, ENetPeer **outPeer)
{
        int head = s_rawQueueHead;
        if (head == s_rawQueueTail)
                return 0;
        size_t copyLen = s_rawQueue[head].len < bufSize ? s_rawQueue[head].len : bufSize;
        memcpy(buf, s_rawQueue[head].data, copyLen);
        if (outLen)  *outLen = s_rawQueue[head].len;
        if (outPeer) *outPeer = s_rawQueue[head].fromPeer;
        s_rawQueueHead = (head + 1) % NP_RAW_QUEUE_SIZE;
        return 1;
}

int _np_send_raw(const uint8_t *data, size_t len, int reliable)
{
        if (!s_np.serverPeer) return -1;
        ENetPacket *pkt = enet_packet_create(data, len,
                reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
        return enet_peer_send(s_np.serverPeer,
                reliable ? NETPLAY_CHAN_RELIABLE : NETPLAY_CHAN_UNRELIABLE, pkt) == 0 ? 0 : -1;
}

int _np_send_to_peer(ENetPeer *peer, const uint8_t *data, size_t len, int reliable)
{
        if (!peer) return -1;
        ENetPacket *pkt = enet_packet_create(data, len,
                reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
        return enet_peer_send(peer,
                reliable ? NETPLAY_CHAN_RELIABLE : NETPLAY_CHAN_UNRELIABLE, pkt) == 0 ? 0 : -1;
}

int _np_host_broadcast_raw(const uint8_t *data, size_t len, int reliable)
{
        if (s_np.mode != NETPLAY_MODE_HOST || !s_np.host) return -1;
        ENetPacket *pkt = enet_packet_create(data, len,
                reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
        enet_host_broadcast(s_np.host,
                reliable ? NETPLAY_CHAN_RELIABLE : NETPLAY_CHAN_UNRELIABLE, pkt);
        return 0;
}

int _np_host_get_peer_count(void)
{
        if (s_np.mode != NETPLAY_MODE_HOST || !s_np.host) return 0;
        return s_np.host->connectedPeers;
}

ENetPeer *_np_host_get_peer(int index)
{
        if (s_np.mode != NETPLAY_MODE_HOST || !s_np.host) return NULL;
        if (index < 0 || index >= (int)s_np.host->connectedPeers) return NULL;
        return &s_np.host->peers[index];
}

/*──────────────────────────────────────────────
 * Accessors (for native_netplay.c to call)
 *──────────────────────────────────────────────*/
static inline int                _np_get_state(void)          { return s_np.state; }
static inline int                _np_get_local_player_id(void){ return s_np.localPlayerId; }
static inline int                _np_get_player_count(void)   { return s_np.playerCount; }
static inline const char        *_np_get_server_address(void){ return s_np.serverAddress; }
static inline struct _NetplayState *_np_state(void)          { return &s_np; }
