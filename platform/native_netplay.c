#include <platform/native_netplay.h>

#include <macros.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__GNUC__) && !defined(_WIN32)
#include <time.h>
#endif

#if defined(_WIN32)
/* Forzamos que NO se defina NOMSG bajo ninguna circunstancia */
#ifdef NOMSG
#undef NOMSG
#endif

#include <windows.h>
#include <winuser.h>
#include <winsock2.h>
#include <ws2tcpip.h>

/* Por si iphlpapi.h se sigue poniendo tonto, definimos el tipo a mano */
#ifndef LPMSG
typedef struct tagMSG *LPMSG;
#endif

#include <platform/native_win32.h>
#pragma comment(lib, "ws2_32")
#define NETPLAY_SOCKET_ERROR       SOCKET_ERROR
#define NETPLAY_SOCKET_INVALID     INVALID_SOCKET
#define NETPLAY_SOCKET             SOCKET
#define NETPLAY_CLOSE_SOCKET(s)    closesocket(s)
typedef int socklen_t;
#elif defined(__GNUC__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#if !defined(_WIN32)
#include <ifaddrs.h>
#include <sys/select.h>
#include <sys/types.h>
#endif
#define NETPLAY_SOCKET_ERROR       (-1)
#define NETPLAY_SOCKET_INVALID     (-1)
#define NETPLAY_SOCKET             int
#define NETPLAY_CLOSE_SOCKET(s)    close(s)
#endif

#define NATIVE_NETPLAY_MAGIC      0x5054454eu /* 'NETP' little-endian */

#define NETPLAY_MAX_PACKET_SIZE   1400
#define NETPLAY_PLAYER_NAME_MAX   32
#define NETPLAY_VERSION_STRING_MAX 16

/* Timers (ms) */
#define NETPLAY_HELLO_RESEND_MS     500
#define NETPLAY_HELLO_TIMEOUT_MS    10000
#define NETPLAY_PEER_TIMEOUT_MS     10000
#define NETPLAY_PING_INTERVAL_MS    1000

/* Default size for chat queue (small ring). */
#define NETPLAY_CHAT_QUEUE_MAX 8

struct NetplayPacketHeader
{
        u32 magic;
        u16 type;
        u16 flags;
        u8  playerId;
        u8  playerCount;
        u16 payloadSize;
};

struct NetplayHelloPayload
{
        u8  playerName[NETPLAY_PLAYER_NAME_MAX];
        u8  gameVersion[NETPLAY_VERSION_STRING_MAX];
        u32 protocolVersion;
};

/* Accept packet (host -> client) — replaces the 2-byte accept payload */
struct NetplayAcceptPayload
{
        u8  assignedId;
        u8  playerCount;
        u8  reserved[2];
};

struct NetplayInputPayload
{
        u32 frameNum;
        u32 buttonsHeld;
        u32 buttonsTapped;
        u32 buttonsReleased;
        s16 stickLX;
        s16 stickLY;
        s16 stickRX;
        s16 stickRY;
};

struct NetplayPingPongPayload
{
        u32 timestamp;
};

struct NetplayPeer
{
        struct sockaddr_in addr;
        u8  id;
        u8  active;
        u8  ready;
        u8  loaded;
        u32 lastFrameReceived;
        u32 lastSeenMs;
        u32 pingTimestamp;
        u16 pingMs;
        char name[NETPLAY_PLAYER_NAME_MAX];
};

struct NetplayFrameInput
{
        u32 frameNum;
        u32 buttonsHeld[NETPLAY_MAX_PLAYERS];
        u32 buttonsTapped[NETPLAY_MAX_PLAYERS];
        u32 buttonsReleased[NETPLAY_MAX_PLAYERS];
        s16 stickLX[NETPLAY_MAX_PLAYERS];
        s16 stickLY[NETPLAY_MAX_PLAYERS];
        s16 stickRX[NETPLAY_MAX_PLAYERS];
        s16 stickRY[NETPLAY_MAX_PLAYERS];
        u8  hasInput[NETPLAY_MAX_PLAYERS];
};

/* === Globals === */
global_variable NETPLAY_SOCKET s_netplaySocket = NETPLAY_SOCKET_INVALID;
global_variable struct sockaddr_in s_hostAddr;
global_variable int s_netplayState;
global_variable u8  s_localPlayerId;
global_variable u8  s_playerCount;
global_variable u8  s_expectedPlayerCount;       /* from --players N, 0 = unlimited */
global_variable u32 s_nextFrameToSend;

/* Host state */
global_variable struct NetplayPeer s_peers[NETPLAY_MAX_PLAYERS];
global_variable int s_peerCount;

/* Input history ring buffer (kept for future rollback use) */
global_variable struct NetplayFrameInput s_inputHistory[NETPLAY_FRAME_HISTORY];
global_variable int s_inputHistoryWrite;

/* Received inputs queue (raw, before delay buffer) */
global_variable struct NetplayInput s_inputQueue[NETPLAY_FRAME_HISTORY * NETPLAY_MAX_PLAYERS];
global_variable int s_inputQueueHead;
global_variable int s_inputQueueTail;

/* Delay buffer: per-player ring of recent inputs, consumed N frames later. */
global_variable struct NetplayInput s_delayBuffer[NETPLAY_MAX_PLAYERS][NETPLAY_INPUT_DELAY_FRAMES + 1];
global_variable int s_delayWriteIndex;

/* Last known input per player (fallback when delay buffer empty) */
global_variable struct NetplayInput s_lastRemoteInput[NETPLAY_MAX_PLAYERS];
global_variable u32 s_lastRemoteFrame[NETPLAY_MAX_PLAYERS];

/* Latest received state snapshot per player */
global_variable struct NetplayStatePayload s_receivedState[NETPLAY_MAX_PLAYERS];
global_variable u32 s_receivedStateFrame[NETPLAY_MAX_PLAYERS];

/* Crate hit queue */
global_variable struct NetplayCrateHit s_crateHitQueue[NETPLAY_CRATE_QUEUE_MAX];
global_variable int s_crateHitQueueHead;
global_variable int s_crateHitQueueTail;

/* Remote player finished flag */
global_variable u8 s_remoteFinished[NETPLAY_MAX_PLAYERS];

/* Chat queue */
global_variable struct NetplayChatPayload s_chatQueue[NETPLAY_CHAT_QUEUE_MAX];
global_variable int s_chatQueueHead;
global_variable int s_chatQueueTail;

/* Item pickup/use queues (one slot per player, overwritten if multiple arrive
 * before consumption — we only care about the latest). */
global_variable u8 s_itemPickupPending[NETPLAY_MAX_PLAYERS];
global_variable u8 s_itemPickupId[NETPLAY_MAX_PLAYERS];
global_variable u8 s_itemPickupNum[NETPLAY_MAX_PLAYERS];
global_variable u8 s_itemUsePending[NETPLAY_MAX_PLAYERS];
global_variable u8 s_itemUseId[NETPLAY_MAX_PLAYERS];

/* RNG seed (one-shot, host -> clients) */
global_variable u32 s_rngSeedPending;
global_variable u32 s_rngSeedFrame;
global_variable int s_rngSeedReady;

/* Interface enumeration cache (filled by Netplay_GetInterfaceList) */
global_variable struct NetplayInterface s_ifaceList[NETPLAY_IFACE_LIST_MAX];
global_variable int s_ifaceCount;
global_variable int s_ifaceSelected;

/* Chat window state (SDL-based) */
global_variable int s_chatWindowOpen;
global_variable char s_chatLineBuf[NETPLAY_CHAT_MSG_MAX];
global_variable int s_chatLineLen;

/* Pause sync (one-shot consumer flags) */
global_variable int s_remotePausePending;
global_variable int s_remoteUnpausePending;

/* Loaded mask: tracks which peers have signalled LOADED. Local loaded tracked
 * via g_NetplayLocalLoaded. */
global_variable u32 s_loadedMask;

/* Ready mask (peers only; local ready is g_NetplayLocalReady) */
global_variable u32 s_readyMask;

/* Return-to-lobby one-shot */
global_variable int s_returnToLobbyPending;

/* Reject reason (set on client when host sends REJECT) */
global_variable int s_rejectReason;

/* HELLO resend tracking (client side) */
global_variable u32 s_lastHelloSendMs;
global_variable u32 s_connectStartMs;

/* Ping tracking */
global_variable u32 s_lastPingTime;

/* Peer timeout (configurable) */
global_variable u32 s_peerTimeoutMs = NETPLAY_PEER_TIMEOUT_MS;

/* Callbacks */
global_variable NetplayEventFn s_onPlayerJoin;
global_variable NetplayEventFn s_onPlayerLeave;

/* Names / display */
global_variable char s_playerNames[NETPLAY_MAX_PLAYERS][NETPLAY_PLAYER_NAME_MAX];
global_variable char s_localPlayerName[NETPLAY_PLAYER_NAME_MAX];
global_variable char s_addressString[64];
global_variable char s_interfaceName[32];

int g_NetplayAutoJoin;
int g_NetplayRaceStarting;
int g_NetplayRacing;
int g_NetplayHostCharacter;
int g_NetplayClientCharacter;
int g_NetplayTrackId;
int g_NetplayNumLaps;
int g_NetplayLocalLoaded;
int g_NetplayRemoteLoaded;
int g_NetplayDisconnected;
int g_NetplayStateRequested;
u32 g_NetplayRemoteChecksumFrame;
u32 g_NetplayRemoteChecksumValue;
u32 g_NetplayReadyMask;
int  g_NetplayReturnToLobby;

/* Local ready state */
global_variable int s_localReady;

/* Windows-specific WSA init tracking */
#if defined(_WIN32)
global_variable int s_wsaInitialized;
#endif

/* === Internal helpers === */

internal u32 Netplay_GetTimestampMs(void)
{
#if defined(_WIN32)
        return (u32)GetTickCount();
#elif defined(__GNUC__)
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (u32)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#else
        return 0;
#endif
}

internal int Netplay_SetNonBlocking(NETPLAY_SOCKET sock)
{
#if defined(_WIN32)
        u_long mode = 1;
        return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1)
                return 0;
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

internal int Netplay_IsRunning(void)
{
        return s_netplaySocket != NETPLAY_SOCKET_INVALID;
}

internal int Netplay_IsHost(void)
{
        return s_netplayState == NETPLAY_STATE_HOSTING;
}

internal void Netplay_ReportJoin(u8 playerId)
{
        if (s_onPlayerJoin != NULL)
                s_onPlayerJoin(playerId);
        fprintf(stdout, "[Netplay] Player %d joined\n", playerId);
        fflush(stdout);
}

internal void Netplay_ReportLeave(u8 playerId)
{
        if (s_onPlayerLeave != NULL)
                s_onPlayerLeave(playerId);
        fprintf(stdout, "[Netplay] Player %d left\n", playerId);
        fflush(stdout);
}

/* === Input queue (raw, pre-delay-buffer) === */

internal void Netplay_QueueInput(u8 playerId, u32 frameNum, u32 buttonsHeld,
                                 u32 buttonsTapped, u32 buttonsReleased,
                                 s16 stickLX, s16 stickLY, s16 stickRX, s16 stickRY)
{
        /* Update last-known input cache FIRST */
        if ((int)(frameNum - s_lastRemoteFrame[playerId]) > 0)
        {
                s_lastRemoteFrame[playerId] = frameNum;
                s_lastRemoteInput[playerId].frameNum = frameNum;
                s_lastRemoteInput[playerId].playerId = playerId;
                s_lastRemoteInput[playerId].buttonsHeld = buttonsHeld;
                s_lastRemoteInput[playerId].buttonsTapped = buttonsTapped;
                s_lastRemoteInput[playerId].buttonsReleased = buttonsReleased;
                s_lastRemoteInput[playerId].stickLX = stickLX;
                s_lastRemoteInput[playerId].stickLY = stickLY;
                s_lastRemoteInput[playerId].stickRX = stickRX;
                s_lastRemoteInput[playerId].stickRY = stickRY;
        }

        /* Drop oldest if queue full */
        while (1)
        {
                int tail = s_inputQueueTail;
                int next = (tail + 1) % (int)len(s_inputQueue);
                if (next != s_inputQueueHead)
                        break;
                s_inputQueueHead = (s_inputQueueHead + 1) % (int)len(s_inputQueue);
        }

        int tail = s_inputQueueTail;
        s_inputQueue[tail].frameNum = frameNum;
        s_inputQueue[tail].playerId = playerId;
        s_inputQueue[tail].buttonsHeld = buttonsHeld;
        s_inputQueue[tail].buttonsTapped = buttonsTapped;
        s_inputQueue[tail].buttonsReleased = buttonsReleased;
        s_inputQueue[tail].stickLX = stickLX;
        s_inputQueue[tail].stickLY = stickLY;
        s_inputQueue[tail].stickRX = stickRX;
        s_inputQueue[tail].stickRY = stickRY;
        s_inputQueueTail = (tail + 1) % (int)len(s_inputQueue);
}

/* === Interface listing (platform-specific) === */

void Netplay_SetInterfaceName(const char *name)
{
        if (name != NULL)
        {
                strncpy(s_interfaceName, name, sizeof(s_interfaceName) - 1);
                s_interfaceName[sizeof(s_interfaceName) - 1] = '\0';
        }
        else
        {
                s_interfaceName[0] = '\0';
        }
}

#if defined(_WIN32)
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi")
int Netplay_ListInterfaces(void)
{
        ULONG bufLen = 0;
        IP_ADAPTER_ADDRESSES *addrs = NULL;
        DWORD ret;

        ret = GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &bufLen);
        if (ret != ERROR_BUFFER_OVERFLOW)
        {
                fprintf(stdout, "[Netplay] No network interfaces found\n");
                return 0;
        }

        addrs = (IP_ADAPTER_ADDRESSES *)malloc(bufLen);
        if (addrs == NULL)
                return 0;

        ret = GetAdaptersAddresses(AF_INET, 0, NULL, addrs, &bufLen);
        if (ret != NO_ERROR)
        {
                free(addrs);
                return 0;
        }

        fprintf(stdout, "[Netplay] Available network interfaces:\n");
        int count = 0;
        for (IP_ADAPTER_ADDRESSES *a = addrs; a != NULL; a = a->Next)
        {
                if (a->OperStatus != IfOperStatusUp)
                        continue;
                IP_ADAPTER_UNICAST_ADDRESS *ua = a->FirstUnicastAddress;
                if (ua == NULL)
                        continue;
                struct sockaddr_in *sin = (struct sockaddr_in *)ua->Address.lpSockaddr;
                if (sin->sin_family != AF_INET)
                        continue;
                u32 ip = ntohl(sin->sin_addr.s_addr);
                if ((ip & 0xFF000000) == 0x7F000000)
                        continue;
                fprintf(stdout, "  %S (%s): %u.%u.%u.%u\n",
                        a->FriendlyName, a->AdapterName,
                        (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                        (ip >> 8) & 0xFF, ip & 0xFF);
                count++;
        }
        free(addrs);
        return count;
}
#elif defined(__GNUC__) && !defined(_WIN32)
int Netplay_ListInterfaces(void)
{
        struct ifaddrs *ifaddr, *ifa;
        int count = 0;

        if (getifaddrs(&ifaddr) == -1)
        {
                fprintf(stderr, "[Netplay] getifaddrs failed\n");
                return 0;
        }

        fprintf(stdout, "[Netplay] Available network interfaces:\n");
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
        {
                if (ifa->ifa_addr == NULL)
                        continue;
                if (ifa->ifa_addr->sa_family != AF_INET)
                        continue;
                struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
                u32 ip = ntohl(sin->sin_addr.s_addr);
                if ((ip & 0xFF000000) == 0x7F000000)
                        continue;
                fprintf(stdout, "  %s: %u.%u.%u.%u\n",
                        ifa->ifa_name,
                        (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                        (ip >> 8) & 0xFF, ip & 0xFF);
                count++;
        }
        freeifaddrs(ifaddr);
        return count;
}
#else
int Netplay_ListInterfaces(void)
{
        fprintf(stdout, "[Netplay] Interface listing not supported on this platform\n");
        return 0;
}
#endif

const char *Netplay_GetAddressString(void)
{
        if (s_addressString[0] == '\0')
                return "unknown";
        return s_addressString;
}

/* === Send helpers === */

internal int Netplay_SendTo(const void *data, int dataSize, const struct sockaddr_in *addr)
{
        int sent;

        if (!Netplay_IsRunning())
                return 0;

        sent = (int)sendto(s_netplaySocket, (const char *)data, dataSize, 0,
                           (const struct sockaddr *)addr, sizeof(*addr));

        return sent == dataSize;
}

internal void Netplay_SendPacket(u16 type, u16 payloadSize, const void *payload,
                                 const struct sockaddr_in *addr)
{
        u8 buffer[NETPLAY_MAX_PACKET_SIZE];
        struct NetplayPacketHeader *header = (struct NetplayPacketHeader *)buffer;

        if (payloadSize > (NETPLAY_MAX_PACKET_SIZE - (u16)sizeof(*header)))
                return;

        header->magic = NATIVE_NETPLAY_MAGIC;
        header->type = type;
        header->flags = 0;
        header->playerId = s_localPlayerId;
        header->playerCount = s_playerCount;
        header->payloadSize = payloadSize;

        if (payloadSize > 0 && payload != NULL)
                memcpy(buffer + sizeof(*header), payload, payloadSize);

        Netplay_SendTo(buffer, sizeof(*header) + payloadSize, addr);
}

void Netplay_BroadcastPacket(u16 type, u16 payloadSize, const void *payload)
{
        int i;
        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (s_peers[i].active)
                        Netplay_SendPacket(type, payloadSize, payload, &s_peers[i].addr);
        }
}

/* Send to all peers EXCEPT one (used when relaying a sender's packet to
 * the other clients). */
internal void Netplay_RelayToOtherPeers(u8 excludePlayerId, u16 type,
                                        u16 payloadSize, const void *payload)
{
        int i;
        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (s_peers[i].active && s_peers[i].id != excludePlayerId)
                        Netplay_SendPacket(type, payloadSize, payload, &s_peers[i].addr);
        }
}

/* === Peer bookkeeping === */

internal int Netplay_FindFreePeerSlot(void)
{
        int i;
        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (!s_peers[i].active)
                        return i;
        }
        return -1;
}

internal int Netplay_FindPeerByAddr(const struct sockaddr_in *addr)
{
        int i;
        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (s_peers[i].active &&
                    s_peers[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
                    s_peers[i].addr.sin_port == addr->sin_port)
                {
                        return i;
                }
        }
        return -1;
}

internal int Netplay_FindPeerById(u8 playerId)
{
        int i;
        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (s_peers[i].active && s_peers[i].id == playerId)
                        return i;
        }
        return -1;
}

internal void Netplay_RemovePeer(u8 playerId)
{
        int index = Netplay_FindPeerById(playerId);

        if (index < 0)
                return;

        s_peers[index].active = 0;
        s_peers[index].id = 0;
        s_peers[index].ready = 0;
        s_peers[index].loaded = 0;
        memset(&s_peers[index].addr, 0, sizeof(s_peers[index].addr));
        memset(s_peers[index].name, 0, NETPLAY_PLAYER_NAME_MAX);

        /* Update masks */
        s_readyMask &= ~(1u << playerId);
        s_loadedMask &= ~(1u << playerId);

        if (s_playerCount > 0)
                s_playerCount--;

        Netplay_ReportLeave(playerId);
}

/* === Packet handlers === */

internal void Netplay_SendReject(const struct sockaddr_in *addr, u8 reason)
{
        Netplay_SendPacket(NETPLAY_PACKET_REJECT, sizeof(reason), &reason, addr);
}

internal void Netplay_SendPlayerListToAll(void)
{
        /* Pack up to NETPLAY_MAX_PLAYERS entries into a single broadcast.
         * Each entry is fixed-size for cross-platform safety. */
        u8 buffer[NETPLAY_MAX_PACKET_SIZE];
        u8 *p = buffer;
        int i;
        int count = 0;

        /* Header: count */
        *p++ = (u8)s_playerCount;

        for (i = 0; i < NETPLAY_MAX_PLAYERS && count < s_playerCount; i++)
        {
                /* Local player (id == 0 on host, or our own id on client) is implicit. */
                if (i == 0 && Netplay_IsHost())
                {
                        struct NetplayPlayerListEntry e;
                        e.id = 0;
                        e.ready = (u8)(s_localReady ? 1 : 0);
                        e.pingMs = 0;
                        strncpy(e.name, s_localPlayerName, sizeof(e.name) - 1);
                        e.name[sizeof(e.name) - 1] = '\0';
                        memcpy(p, &e, sizeof(e));
                        p += sizeof(e);
                        count++;
                }
                else if (s_peers[i].active)
                {
                        struct NetplayPlayerListEntry e;
                        e.id = s_peers[i].id;
                        e.ready = s_peers[i].ready;
                        e.pingMs = s_peers[i].pingMs;
                        memcpy(e.name, s_peers[i].name, sizeof(e.name));
                        e.name[sizeof(e.name) - 1] = '\0';
                        memcpy(p, &e, sizeof(e));
                        p += sizeof(e);
                        count++;
                }
        }

        Netplay_BroadcastPacket(NETPLAY_PACKET_PLAYER_LIST,
                                (u16)(p - buffer), buffer);
}

internal void Netplay_HandleHello(const struct NetplayPacketHeader *header,
                                  const struct sockaddr_in *fromAddr,
                                  const u8 *payload, int payloadSize)
{
        const struct NetplayHelloPayload *hello = (const struct NetplayHelloPayload *)payload;
        int slot;
        u8 newPlayerId = 0;

        (void)header;

        /* ----- Client side: host's HELLO accept arrives here ----- */
        if (!Netplay_IsHost())
        {
                /* The host sends back a NetplayAcceptPayload (or, legacy, the raw
                 * 2-byte buffer). Accept either form. */
                int existing = Netplay_FindPeerByAddr(fromAddr);
                if (existing < 0)
                {
                        slot = Netplay_FindFreePeerSlot();
                        if (slot >= 0)
                        {
                                s_peers[slot].addr = *fromAddr;
                                s_peers[slot].id = header->playerId;
                                s_peers[slot].active = 1;
                                s_peers[slot].lastFrameReceived = 0;
                                s_peers[slot].lastSeenMs = Netplay_GetTimestampMs();
                                s_peers[slot].pingTimestamp = 0;
                                s_peers[slot].pingMs = 0;
                                s_peers[slot].ready = 0;
                                s_peers[slot].loaded = 0;
                                s_playerCount++;
                        }
                }
                else
                {
                        s_peers[existing].lastSeenMs = Netplay_GetTimestampMs();
                }

                if (payload != NULL && payloadSize >= (int)sizeof(struct NetplayAcceptPayload))
                {
                        const struct NetplayAcceptPayload *acc = (const struct NetplayAcceptPayload *)payload;
                        s_localPlayerId = acc->assignedId;
                        s_playerCount = acc->playerCount;
                        fprintf(stdout, "[Netplay] Assigned player ID %d (count=%d)\n",
                                s_localPlayerId, s_playerCount);
                        fflush(stdout);
                }
                else if (payload != NULL && payloadSize >= 1)
                {
                        /* Legacy 2-byte accept */
                        s_localPlayerId = payload[0];
                        if (payloadSize >= 2)
                                s_playerCount = payload[1];
                        fprintf(stdout, "[Netplay] Assigned player ID %d (legacy)\n", s_localPlayerId);
                        fflush(stdout);
                }
                return;
        }

        /* ----- Host side: incoming HELLO from a new client ----- */

        /* Version check */
        if (payloadSize < (int)sizeof(struct NetplayHelloPayload) ||
            hello->protocolVersion != NETPLAY_PROTOCOL_VERSION)
        {
                fprintf(stderr, "[Netplay] Rejecting client: protocol mismatch "
                                "(theirs=%u, ours=%u)\n",
                        (hello && payloadSize >= (int)sizeof(*hello)) ? hello->protocolVersion : 0,
                        NETPLAY_PROTOCOL_VERSION);
                Netplay_SendReject(fromAddr, NETPLAY_REJECT_VERSION_MISMATCH);
                return;
        }

        /* Expected player count enforcement */
        if (s_expectedPlayerCount > 0 && s_playerCount >= s_expectedPlayerCount)
        {
                fprintf(stderr, "[Netplay] Rejecting client: lobby full (%d/%d)\n",
                        s_playerCount, s_expectedPlayerCount);
                Netplay_SendReject(fromAddr, NETPLAY_REJECT_SERVER_FULL);
                return;
        }

        if (s_playerCount >= NETPLAY_MAX_PLAYERS)
        {
                fprintf(stderr, "[Netplay] Max players reached\n");
                Netplay_SendReject(fromAddr, NETPLAY_REJECT_SERVER_FULL);
                return;
        }

        /* Already connected? Refresh + resend accept. */
        if (Netplay_FindPeerByAddr(fromAddr) >= 0)
        {
                int existingSlot = Netplay_FindPeerByAddr(fromAddr);
                if (existingSlot >= 0)
                {
                        s_peers[existingSlot].lastSeenMs = Netplay_GetTimestampMs();
                        struct NetplayAcceptPayload acc;
                        acc.assignedId = s_peers[existingSlot].id;
                        acc.playerCount = (u8)s_playerCount;
                        acc.reserved[0] = 0;
                        acc.reserved[1] = 0;
                        Netplay_SendPacket(NETPLAY_PACKET_HELLO, sizeof(acc), &acc, fromAddr);
                }
                Netplay_SendPlayerListToAll();
                return;
        }

        slot = Netplay_FindFreePeerSlot();
        if (slot < 0)
        {
                Netplay_SendReject(fromAddr, NETPLAY_REJECT_SERVER_FULL);
                return;
        }

        newPlayerId = (u8)(slot + 1);
        if (newPlayerId == 0)
                newPlayerId = (u8)(slot + 2);

        s_peers[slot].addr = *fromAddr;
        s_peers[slot].id = newPlayerId;
        s_peers[slot].active = 1;
        s_peers[slot].lastFrameReceived = 0;
        s_peers[slot].lastSeenMs = Netplay_GetTimestampMs();
        s_peers[slot].pingTimestamp = 0;
        s_peers[slot].pingMs = 0;
        s_peers[slot].ready = 0;
        s_peers[slot].loaded = 0;

        s_playerCount++;

        /* Save name */
        memcpy(s_playerNames[newPlayerId], hello->playerName, NETPLAY_PLAYER_NAME_MAX);
        s_playerNames[newPlayerId][NETPLAY_PLAYER_NAME_MAX - 1] = '\0';
        memcpy(s_peers[slot].name, hello->playerName, NETPLAY_PLAYER_NAME_MAX);
        s_peers[slot].name[NETPLAY_PLAYER_NAME_MAX - 1] = '\0';

        fprintf(stdout, "[Netplay] New connection from %s:%d -> player %d (%s)\n",
                inet_ntoa(fromAddr->sin_addr), ntohs(fromAddr->sin_port),
                newPlayerId, s_peers[slot].name);
        fflush(stdout);

        Netplay_ReportJoin(newPlayerId);

        /* Send accept back to the new client */
        {
                struct NetplayAcceptPayload acc;
                acc.assignedId = newPlayerId;
                acc.playerCount = (u8)s_playerCount;
                acc.reserved[0] = 0;
                acc.reserved[1] = 0;
                Netplay_SendPacket(NETPLAY_PACKET_HELLO, sizeof(acc), &acc, fromAddr);
        }

        /* Broadcast updated roster to all clients */
        Netplay_SendPlayerListToAll();
}

internal void Netplay_HandleInput(const struct NetplayPacketHeader *header,
                                  const u8 *payload, int payloadSize)
{
        const struct NetplayInputPayload *input = (const struct NetplayInputPayload *)payload;
        int peerIndex;
        u8 senderId;

        if (payloadSize < (int)sizeof(struct NetplayInputPayload))
                return;

        senderId = header->playerId;
        peerIndex = Netplay_FindPeerById(senderId);
        if (peerIndex >= 0)
        {
                s_peers[peerIndex].lastFrameReceived = input->frameNum;
                s_peers[peerIndex].lastSeenMs = Netplay_GetTimestampMs();
        }

        /* Queue input for game consumption */
        Netplay_QueueInput(senderId, input->frameNum, input->buttonsHeld,
                           input->buttonsTapped, input->buttonsReleased,
                           input->stickLX, input->stickLY,
                           input->stickRX, input->stickRY);

        /* Host: relay to other clients so 3+ player games work */
        if (Netplay_IsHost())
        {
                Netplay_RelayToOtherPeers(senderId, NETPLAY_PACKET_INPUT,
                                           (u16)sizeof(*input), input);
        }
}

internal void Netplay_HandlePing(const struct NetplayPacketHeader *header,
                                 const u8 *payload, int payloadSize,
                                 const struct sockaddr_in *fromAddr)
{
        const struct NetplayPingPongPayload *ping = (const struct NetplayPingPongPayload *)payload;
        (void)header;
        (void)payloadSize;

        if (ping != NULL)
        {
                struct NetplayPingPongPayload pong;
                pong.timestamp = ping->timestamp;
                Netplay_SendPacket(NETPLAY_PACKET_PONG, sizeof(pong), &pong, fromAddr);
        }
}

internal void Netplay_HandlePong(const struct NetplayPacketHeader *header,
                                 const u8 *payload, int payloadSize)
{
        const struct NetplayPingPongPayload *pong = (const struct NetplayPingPongPayload *)payload;
        u32 now;
        int peerIndex;

        (void)payloadSize;
        if (pong == NULL)
                return;

        now = Netplay_GetTimestampMs();
        peerIndex = Netplay_FindPeerById(header->playerId);

        if (peerIndex >= 0 && pong->timestamp >= s_peers[peerIndex].pingTimestamp)
        {
                s_peers[peerIndex].pingMs = (u16)(now - pong->timestamp);
                s_peers[peerIndex].lastSeenMs = now;
        }
}

internal void Netplay_HandleDisconnect(const struct NetplayPacketHeader *header)
{
        fprintf(stdout, "[Netplay] Player %u disconnected\n", header->playerId);
        fflush(stdout);
        Netplay_RemovePeer(header->playerId);
        if (g_NetplayRacing)
                g_NetplayDisconnected = 1;
}

/* Relay-only packets: the host forwards these to other clients so that
 * clients can see each other's actions in 3+ player lobbies. */
internal void Netplay_RelayPacketToOthers(const struct NetplayPacketHeader *header,
                                          const u8 *payload)
{
        if (!Netplay_IsHost())
                return;
        Netplay_RelayToOtherPeers(header->playerId, header->type,
                                  header->payloadSize, payload);
}

internal void Netplay_HandleStartRace(const struct NetplayPacketHeader *header)
{
        fprintf(stdout, "[Netplay] Race starting!\n");
        fflush(stdout);
        g_NetplayRaceStarting = 1;
        (void)header;
}

internal void Netplay_HandleCharacterSelect(const struct NetplayPacketHeader *header,
                                            const u8 *payload, int payloadSize)
{
        if (payloadSize < 1)
                return;
        {
                u8 charId = payload[0];
                if (Netplay_IsHost())
                        g_NetplayClientCharacter = charId;
                else
                        g_NetplayHostCharacter = charId;
                fprintf(stdout, "[Netplay] Player %u chose character %d\n",
                        header->playerId, charId);
                fflush(stdout);
        }
        /* Relay to other clients (host only) */
        Netplay_RelayPacketToOthers(header, payload);
}

internal void Netplay_HandleTrackSelect(const struct NetplayPacketHeader *header,
                                        const u8 *payload, int payloadSize)
{
        if (!Netplay_IsHost() && payloadSize >= 2)
        {
                g_NetplayTrackId = payload[0];
                g_NetplayNumLaps = payload[1];
                g_NetplayRaceStarting = 1;
                fprintf(stdout, "[Netplay] Host chose track %d, %d laps\n",
                        g_NetplayTrackId, g_NetplayNumLaps);
                fflush(stdout);
        }
        /* Track select is host -> clients only; no relay needed. */
        (void)header;
}

internal void Netplay_HandlePause(const struct NetplayPacketHeader *header)
{
        if (g_NetplayRacing)
        {
                fprintf(stdout, "[Netplay] Player %u paused\n", header->playerId);
                fflush(stdout);
                s_remotePausePending = 1;
        }
}

internal void Netplay_HandleUnpause(const struct NetplayPacketHeader *header)
{
        if (g_NetplayRacing)
        {
                fprintf(stdout, "[Netplay] Player %u unpaused\n", header->playerId);
                fflush(stdout);
                s_remoteUnpausePending = 1;
        }
}

internal void Netplay_HandleLoaded(const struct NetplayPacketHeader *header)
{
        fprintf(stdout, "[Netplay] Player %u finished loading\n", header->playerId);
        fflush(stdout);

        /* Mark the peer's loaded flag and the global mask */
        {
                int peerIndex = Netplay_FindPeerById(header->playerId);
                if (peerIndex >= 0)
                {
                        s_peers[peerIndex].loaded = 1;
                        s_loadedMask |= (1u << header->playerId);
                }
        }

        /* Legacy: set the bool too */
        g_NetplayRemoteLoaded = 1;

        /* Host: relay to other clients */
        Netplay_RelayPacketToOthers(header, NULL);
}

internal void Netplay_HandleState(const struct NetplayPacketHeader *header,
                                  const u8 *payload, int payloadSize)
{
        const struct NetplayStatePayload *st = (const struct NetplayStatePayload *)payload;
        if (payloadSize >= (int)sizeof(struct NetplayStatePayload) && st != NULL)
        {
                u8 senderId = header->playerId;
                if ((int)(st->frameNum - s_receivedStateFrame[senderId]) > 0)
                {
                        s_receivedState[senderId] = *st;
                        s_receivedStateFrame[senderId] = st->frameNum;
                }
        }

        /* Host: relay state packets so other clients can see this player */
        Netplay_RelayPacketToOthers(header, payload);
}

internal void Netplay_HandleCrateHit(const struct NetplayPacketHeader *header,
                                     const u8 *payload, int payloadSize)
{
        const struct NetplayCrateHit *hit = (const struct NetplayCrateHit *)payload;
        if (payloadSize >= (int)sizeof(struct NetplayCrateHit) && hit != NULL)
        {
                int tail = s_crateHitQueueTail;
                int next = (tail + 1) % NETPLAY_CRATE_QUEUE_MAX;
                if (next != s_crateHitQueueHead)
                {
                        s_crateHitQueue[tail] = *hit;
                        s_crateHitQueueTail = next;
                }
        }

        /* Host: relay so other clients also destroy this crate */
        Netplay_RelayPacketToOthers(header, payload);
}

internal void Netplay_HandleFinished(const struct NetplayPacketHeader *header,
                                     int payloadSize)
{
        if (payloadSize >= 1)
        {
                u8 finishedPlayerId = header->playerId;
                if (finishedPlayerId < NETPLAY_MAX_PLAYERS)
                        s_remoteFinished[finishedPlayerId] = 1;
                fprintf(stdout, "[Netplay] Player %u finished the race!\n",
                        finishedPlayerId);
                fflush(stdout);
        }

        /* Relay so all clients know who finished */
        Netplay_RelayPacketToOthers(header, NULL);
}

internal void Netplay_HandleChecksum(const struct NetplayPacketHeader *header,
                                     const u8 *payload, int payloadSize)
{
        const struct NetplayChecksumPayload *cp = (const struct NetplayChecksumPayload *)payload;
        if (payloadSize >= (int)sizeof(struct NetplayChecksumPayload) && cp != NULL)
        {
                /* Store the most recent remote checksum for the driver this
                 * checksum is ABOUT (not the sender). The caller in MainMain
                 * compares this against its own simulation of the same driver. */
                (void)header;
                g_NetplayRemoteChecksumFrame = cp->frameNum;
                g_NetplayRemoteChecksumValue = cp->checksum;
        }

        /* Relay so all clients can compare their simulation of the same driver */
        Netplay_RelayPacketToOthers(header, payload);
}

internal void Netplay_HandleStateReq(const struct NetplayPacketHeader *header)
{
        g_NetplayStateRequested = 1;
        /* Relay (host only) so all clients respond with their state */
        Netplay_RelayPacketToOthers(header, NULL);
}

internal void Netplay_HandleChat(const struct NetplayPacketHeader *header,
                                 const u8 *payload, int payloadSize)
{
        const struct NetplayChatPayload *chat = (const struct NetplayChatPayload *)payload;
        if (payloadSize >= (int)sizeof(struct NetplayChatPayload) && chat != NULL)
        {
                int tail = s_chatQueueTail;
                int next = (tail + 1) % NETPLAY_CHAT_QUEUE_MAX;
                if (next != s_chatQueueHead)
                {
                        s_chatQueue[tail] = *chat;
                        s_chatQueue[tail].senderId = header->playerId;
                        s_chatQueueTail = next;
                }

                /* Print to the chat window if it's open */
                if (s_chatWindowOpen)
                {
                        char line[NETPLAY_CHAT_MSG_MAX + 32];
                        snprintf(line, sizeof(line), "[P%d] %s",
                                 (int)header->playerId, chat->message);
                        Netplay_ChatPrint(line);
                }
        }

        /* Relay to other clients (host only) */
        Netplay_RelayPacketToOthers(header, payload);
}

internal void Netplay_HandlePlayerReady(const struct NetplayPacketHeader *header,
                                        const u8 *payload, int payloadSize)
{
        u8 playerId = header->playerId;
        u8 ready = 0;

        if (payloadSize >= 2)
        {
                playerId = payload[0];
                ready = payload[1];
        }
        else if (payloadSize >= 1)
        {
                ready = payload[0];
        }

        /* Mark this peer's ready state */
        {
                int peerIndex = Netplay_FindPeerById(playerId);
                if (peerIndex >= 0)
                        s_peers[peerIndex].ready = ready ? 1 : 0;
        }
        if (ready)
                s_readyMask |= (1u << playerId);
        else
                s_readyMask &= ~(1u << playerId);

        g_NetplayReadyMask = s_readyMask;

        fprintf(stdout, "[Netplay] Player %u %s ready\n",
                playerId, ready ? "is" : "is NOT");
        fflush(stdout);

        /* Relay to other clients */
        Netplay_RelayPacketToOthers(header, payload);
}

internal void Netplay_HandleReturnLobby(const struct NetplayPacketHeader *header)
{
        fprintf(stdout, "[Netplay] Host says: return to lobby\n");
        fflush(stdout);
        s_returnToLobbyPending = 1;
        g_NetplayReturnToLobby = 1;
        (void)header;
}

internal void Netplay_HandleReject(const struct NetplayPacketHeader *header,
                                   const u8 *payload, int payloadSize)
{
        if (payloadSize >= 1)
        {
                s_rejectReason = payload[0];
                fprintf(stderr, "[Netplay] Rejected by host: %s (%d)\n",
                        Netplay_GetRejectReasonString(s_rejectReason), s_rejectReason);
                fflush(stderr);
        }
        (void)header;
}

internal void Netplay_HandlePlayerList(const struct NetplayPacketHeader *header,
                                       const u8 *payload, int payloadSize)
{
        /* Client side: parse player list from host and update local state. */
        const u8 *p = payload;
        const u8 *end;
        int count;
        int i;

        (void)header;
        if (p == NULL || payloadSize < 1)
                return;

        count = *p++;
        end = payload + payloadSize;

        /* Clear existing peer info first (we'll overwrite from the list) */
        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (s_peers[i].active && s_peers[i].id != s_localPlayerId)
                {
                        s_peers[i].active = 0;
                        s_peers[i].id = 0;
                }
        }

        for (i = 0; i < count && p + sizeof(struct NetplayPlayerListEntry) <= end; i++)
        {
                struct NetplayPlayerListEntry e;
                memcpy(&e, p, sizeof(e));
                p += sizeof(e);

                if (e.id == s_localPlayerId)
                {
                        /* That's us, skip */
                        continue;
                }

                {
                        int slot = Netplay_FindPeerById(e.id);
                        if (slot < 0)
                                slot = Netplay_FindFreePeerSlot();
                        if (slot >= 0)
                        {
                                s_peers[slot].id = e.id;
                                s_peers[slot].active = 1;
                                s_peers[slot].ready = e.ready;
                                s_peers[slot].pingMs = e.pingMs;
                                s_peers[slot].lastFrameReceived = 0;
                                s_peers[slot].lastSeenMs = Netplay_GetTimestampMs();
                                memcpy(s_peers[slot].name, e.name, sizeof(s_peers[slot].name));
                                memcpy(s_playerNames[e.id], e.name, sizeof(s_playerNames[e.id]));
                        }
                }
        }
        s_playerCount = (u8)count;
}

/* === Main packet receive loop === */

/* Forward declarations of item/RNG handlers (defined later in the file) */
internal void Netplay_HandleItemPickup(const struct NetplayPacketHeader *header,
                                       const u8 *payload, int payloadSize);
internal void Netplay_HandleItemUse(const struct NetplayPacketHeader *header,
                                    const u8 *payload, int payloadSize);
internal void Netplay_HandleRngSeed(const struct NetplayPacketHeader *header,
                                    const u8 *payload, int payloadSize);

internal int Netplay_ReceivePacket(void)
{
        u8 buffer[NETPLAY_MAX_PACKET_SIZE];
        struct sockaddr_in fromAddr;
        socklen_t fromLen;
        int received;
        struct NetplayPacketHeader *header;
        u8 *payload;
        int payloadSize;

        fromLen = sizeof(fromAddr);
        received = (int)recvfrom(s_netplaySocket, (char *)buffer, sizeof(buffer), 0,
                                 (struct sockaddr *)&fromAddr, &fromLen);

        if (received <= 0)
                return 0;

        if (received < (int)sizeof(struct NetplayPacketHeader))
                return 0;

        header = (struct NetplayPacketHeader *)buffer;

        if (header->magic != NATIVE_NETPLAY_MAGIC)
                return 0;

        if ((u16)received < sizeof(*header) + header->payloadSize)
                return 0;

        payload = (header->payloadSize > 0) ? (buffer + sizeof(*header)) : NULL;
        payloadSize = header->payloadSize;

        switch (header->type)
        {
        case NETPLAY_PACKET_HELLO:
                Netplay_HandleHello(header, &fromAddr, payload, payloadSize);
                break;
        case NETPLAY_PACKET_INPUT:
                Netplay_HandleInput(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_PING:
                Netplay_HandlePing(header, payload, payloadSize, &fromAddr);
                break;
        case NETPLAY_PACKET_PONG:
                Netplay_HandlePong(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_DISCONNECT:
                Netplay_HandleDisconnect(header);
                break;
        case NETPLAY_PACKET_START_RACE:
                Netplay_HandleStartRace(header);
                break;
        case NETPLAY_PACKET_CHARACTER_SELECT:
                Netplay_HandleCharacterSelect(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_TRACK_SELECT:
                Netplay_HandleTrackSelect(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_PAUSE:
                Netplay_HandlePause(header);
                break;
        case NETPLAY_PACKET_UNPAUSE:
                Netplay_HandleUnpause(header);
                break;
        case NETPLAY_PACKET_LOADED:
                Netplay_HandleLoaded(header);
                break;
        case NETPLAY_PACKET_STATE:
                Netplay_HandleState(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_CRATE_HIT:
                Netplay_HandleCrateHit(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_FINISHED:
                Netplay_HandleFinished(header, payloadSize);
                break;
        case NETPLAY_PACKET_CHECKSUM:
                Netplay_HandleChecksum(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_STATE_REQ:
                Netplay_HandleStateReq(header);
                break;
        case NETPLAY_PACKET_CHAT:
                Netplay_HandleChat(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_PLAYER_READY:
                Netplay_HandlePlayerReady(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_RETURN_LOBBY:
                Netplay_HandleReturnLobby(header);
                break;
        case NETPLAY_PACKET_REJECT:
                Netplay_HandleReject(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_PLAYER_LIST:
                Netplay_HandlePlayerList(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_ITEM_PICKUP:
                Netplay_HandleItemPickup(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_ITEM_USE:
                Netplay_HandleItemUse(header, payload, payloadSize);
                break;
        case NETPLAY_PACKET_RNG_SEED:
                Netplay_HandleRngSeed(header, payload, payloadSize);
                break;
        default:
                /* Unknown packet type: ignore (forward-compat) */
                break;
        }

        return 1;
}

/* === Init / Shutdown === */

int Netplay_Init(void)
{
        if (Netplay_IsRunning())
                return 1;

        memset(s_peers, 0, sizeof(s_peers));
        memset(s_inputHistory, 0, sizeof(s_inputHistory));
        memset(s_inputQueue, 0, sizeof(s_inputQueue));
        memset(s_playerNames, 0, sizeof(s_playerNames));
        memset(s_localPlayerName, 0, sizeof(s_localPlayerName));
        memset(s_lastRemoteInput, 0, sizeof(s_lastRemoteInput));
        memset(s_lastRemoteFrame, 0, sizeof(s_lastRemoteFrame));
        memset(s_receivedState, 0, sizeof(s_receivedState));
        memset(s_receivedStateFrame, 0, sizeof(s_receivedStateFrame));
        memset(s_crateHitQueue, 0, sizeof(s_crateHitQueue));
        memset(s_remoteFinished, 0, sizeof(s_remoteFinished));
        memset(s_chatQueue, 0, sizeof(s_chatQueue));
        memset(s_delayBuffer, 0, sizeof(s_delayBuffer));
        s_crateHitQueueHead = 0;
        s_crateHitQueueTail = 0;
        s_chatQueueHead = 0;
        s_chatQueueTail = 0;

        s_inputQueueHead = 0;
        s_inputQueueTail = 0;
        s_inputHistoryWrite = 0;
        s_delayWriteIndex = 0;
        s_netplayState = NETPLAY_STATE_DISCONNECTED;
        s_localPlayerId = 0;
        s_playerCount = 0;
        s_expectedPlayerCount = 0;
        s_peerCount = 0;
        s_nextFrameToSend = 0;
        s_onPlayerJoin = NULL;
        s_onPlayerLeave = NULL;
        s_lastPingTime = Netplay_GetTimestampMs();
        s_loadedMask = 0;
        s_readyMask = 0;
        s_localReady = 0;
        s_remotePausePending = 0;
        s_remoteUnpausePending = 0;
        s_returnToLobbyPending = 0;
        s_rejectReason = NETPLAY_REJECT_NONE;
        s_lastHelloSendMs = 0;
        s_connectStartMs = 0;

        g_NetplayAutoJoin = 0;
        g_NetplayRaceStarting = 0;
        g_NetplayHostCharacter = -1;
        g_NetplayClientCharacter = -1;
        g_NetplayTrackId = 0;
        g_NetplayNumLaps = 3;
        g_NetplayLocalLoaded = 0;
        g_NetplayRemoteLoaded = 0;
        g_NetplayDisconnected = 0;
        g_NetplayStateRequested = 0;
        g_NetplayRemoteChecksumFrame = 0;
        g_NetplayRemoteChecksumValue = 0;
        g_NetplayReadyMask = 0;
        g_NetplayReturnToLobby = 0;

#if defined(_WIN32)
        if (!s_wsaInitialized)
        {
                WSADATA wsaData;
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
                {
                        fprintf(stderr, "[Netplay] WSAStartup failed\n");
                        return 0;
                }
                s_wsaInitialized = 1;
        }
#endif

        fprintf(stdout, "[Netplay] Initialized (protocol v%d, input delay=%d frames)\n",
                NETPLAY_PROTOCOL_VERSION, NETPLAY_INPUT_DELAY_FRAMES);
        fflush(stdout);
        return 1;
}

void Netplay_Shutdown(void)
{
        if (Netplay_IsRunning())
        {
                if (s_netplayState != NETPLAY_STATE_DISCONNECTED)
                        Netplay_Disconnect();

                NETPLAY_CLOSE_SOCKET(s_netplaySocket);
                s_netplaySocket = NETPLAY_SOCKET_INVALID;
        }

#if defined(_WIN32)
        if (s_wsaInitialized)
        {
                WSACleanup();
                s_wsaInitialized = 0;
        }
#endif

        s_netplayState = NETPLAY_STATE_DISCONNECTED;
        fprintf(stdout, "[Netplay] Shutdown\n");
        fflush(stdout);
}

/* === Host / Connect === */

/* Helper used by both host and client to resolve the local IP for display. */
internal void Netplay_ResolveLocalAddress(u16 port)
{
        u32 resolvedIP = 0;

        s_addressString[0] = '\0';

        /* Try interface name first */
        if (s_interfaceName[0] != '\0')
        {
#if defined(_WIN32)
                ULONG bufLen = 0;
                IP_ADAPTER_ADDRESSES *addrs = NULL;
                GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &bufLen);
                addrs = (IP_ADAPTER_ADDRESSES *)malloc(bufLen);
                if (addrs != NULL)
                {
                        if (GetAdaptersAddresses(AF_INET, 0, NULL, addrs, &bufLen) == NO_ERROR)
                        {
                                for (IP_ADAPTER_ADDRESSES *a = addrs; a != NULL; a = a->Next)
                                {
                                        char aname[256];
                                        wcstombs(aname, a->FriendlyName, sizeof(aname));
                                        if (strcmp(aname, s_interfaceName) != 0 &&
                                            strcmp((const char *)a->AdapterName, s_interfaceName) != 0)
                                                continue;
                                        IP_ADAPTER_UNICAST_ADDRESS *ua = a->FirstUnicastAddress;
                                        if (ua != NULL)
                                        {
                                                struct sockaddr_in *sin = (struct sockaddr_in *)ua->Address.lpSockaddr;
                                                if (sin->sin_family == AF_INET)
                                                {
                                                        resolvedIP = ntohl(sin->sin_addr.s_addr);
                                                        break;
                                                }
                                        }
                                }
                        }
                        free(addrs);
                }
#elif defined(__GNUC__) && !defined(_WIN32)
                {
                        struct ifaddrs *ifaddr, *ifa;
                        if (getifaddrs(&ifaddr) == 0)
                        {
                                for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
                                {
                                        if (ifa->ifa_addr == NULL)
                                                continue;
                                        if (ifa->ifa_addr->sa_family != AF_INET)
                                                continue;
                                        if (strcmp(ifa->ifa_name, s_interfaceName) != 0)
                                                continue;
                                        struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
                                        resolvedIP = ntohl(sin->sin_addr.s_addr);
                                        break;
                                }
                                freeifaddrs(ifaddr);
                        }
                }
#endif
        }

        /* Fallback: resolve via hostname */
        if (resolvedIP == 0)
        {
                char hostname[256];
                struct addrinfo hints, *res;

                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;

                if (gethostname(hostname, sizeof(hostname)) == 0 &&
                    getaddrinfo(hostname, NULL, &hints, &res) == 0)
                {
                        struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
                        resolvedIP = ntohl(sin->sin_addr.s_addr);
                        freeaddrinfo(res);
                }
        }

        if (resolvedIP != 0)
        {
                snprintf(s_addressString, sizeof(s_addressString),
                         "%u.%u.%u.%u:%u",
                         (resolvedIP >> 24) & 0xFF, (resolvedIP >> 16) & 0xFF,
                         (resolvedIP >> 8) & 0xFF, resolvedIP & 0xFF,
                         port);
        }
}

int Netplay_Host(u16 port)
{
        struct sockaddr_in bindAddr;

        if (Netplay_IsRunning())
        {
                fprintf(stderr, "[Netplay] Already running\n");
                return 0;
        }

        s_netplaySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s_netplaySocket == NETPLAY_SOCKET_INVALID)
        {
                fprintf(stderr, "[Netplay] Failed to create socket\n");
                return 0;
        }

        memset(&bindAddr, 0, sizeof(bindAddr));
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = INADDR_ANY;
        bindAddr.sin_port = htons(port);

        if (bind(s_netplaySocket, (struct sockaddr *)&bindAddr, sizeof(bindAddr)) == NETPLAY_SOCKET_ERROR)
        {
                fprintf(stderr, "[Netplay] Failed to bind to port %u\n", port);
                NETPLAY_CLOSE_SOCKET(s_netplaySocket);
                s_netplaySocket = NETPLAY_SOCKET_INVALID;
                return 0;
        }

        Netplay_SetNonBlocking(s_netplaySocket);

        s_netplayState = NETPLAY_STATE_HOSTING;
        s_localPlayerId = 0;
        s_playerCount = 1;
        s_expectedPlayerCount = 0;

        /* Host is always player 0 */
        memset(s_playerNames[0], 0, NETPLAY_PLAYER_NAME_MAX);
        strncpy(s_playerNames[0], s_localPlayerName, NETPLAY_PLAYER_NAME_MAX - 1);

        memset(&s_hostAddr, 0, sizeof(s_hostAddr));
        s_hostAddr.sin_family = AF_INET;
        s_hostAddr.sin_addr.s_addr = INADDR_ANY;
        s_hostAddr.sin_port = htons(port);

        Netplay_ResolveLocalAddress(port);

        fprintf(stdout, "[Netplay] Hosting on port %u (player 0)\n", port);
        fflush(stdout);
        return 1;
}

int Netplay_Connect(const char *address, u16 port)
{
        struct sockaddr_in addr;

        if (Netplay_IsRunning())
        {
                fprintf(stderr, "[Netplay] Already running\n");
                return 0;
        }

        s_netplaySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s_netplaySocket == NETPLAY_SOCKET_INVALID)
        {
                fprintf(stderr, "[Netplay] Failed to create socket\n");
                return 0;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, address, &addr.sin_addr) <= 0)
        {
                fprintf(stderr, "[Netplay] Invalid address: %s\n", address);
                NETPLAY_CLOSE_SOCKET(s_netplaySocket);
                s_netplaySocket = NETPLAY_SOCKET_INVALID;
                return 0;
        }

        Netplay_SetNonBlocking(s_netplaySocket);

        s_hostAddr = addr;
        s_netplayState = NETPLAY_STATE_CONNECTING;
        s_localPlayerId = 0; /* will be assigned by host */
        s_playerCount = 1;
        s_expectedPlayerCount = 0;

        /* Send HELLO with protocol version */
        {
                struct NetplayHelloPayload hello;

                memset(&hello, 0, sizeof(hello));
                strncpy((char *)hello.playerName, s_localPlayerName, NETPLAY_PLAYER_NAME_MAX - 1);
                strncpy((char *)hello.gameVersion, "CTR-Native", NETPLAY_VERSION_STRING_MAX - 1);
                hello.protocolVersion = NETPLAY_PROTOCOL_VERSION;

                Netplay_SendPacket(NETPLAY_PACKET_HELLO, sizeof(hello), &hello, &addr);
        }

        s_lastHelloSendMs = Netplay_GetTimestampMs();
        s_connectStartMs = s_lastHelloSendMs;

        fprintf(stdout, "[Netplay] Connecting to %s:%u...\n", address, port);
        fflush(stdout);
        return 1;
}

void Netplay_Disconnect(void)
{
        if (s_netplayState != NETPLAY_STATE_DISCONNECTED)
        {
                /* Send DISCONNECT to peers */
                if (Netplay_IsRunning())
                {
                        if (Netplay_IsHost())
                        {
                                Netplay_BroadcastPacket(NETPLAY_PACKET_DISCONNECT, 0, NULL);
                        }
                        else
                        {
                                Netplay_SendPacket(NETPLAY_PACKET_DISCONNECT, 0, NULL, &s_hostAddr);
                        }
                }

                s_netplayState = NETPLAY_STATE_DISCONNECTED;
                s_localPlayerId = 0;
                s_playerCount = 0;
                g_NetplayRacing = 0;
                g_NetplayLocalLoaded = 0;
                g_NetplayRemoteLoaded = 0;
                g_NetplayStateRequested = 0;
                g_NetplayRemoteChecksumFrame = 0;
                g_NetplayRemoteChecksumValue = 0;
                g_NetplayReturnToLobby = 0;
                memset(s_remoteFinished, 0, sizeof(s_remoteFinished));
                s_crateHitQueueHead = 0;
                s_crateHitQueueTail = 0;
                s_loadedMask = 0;
                s_readyMask = 0;
                s_localReady = 0;
                s_remotePausePending = 0;
                s_remoteUnpausePending = 0;
                s_returnToLobbyPending = 0;
                s_chatQueueHead = 0;
                s_chatQueueTail = 0;

                memset(s_peers, 0, sizeof(s_peers));
                s_inputQueueHead = 0;
                s_inputQueueTail = 0;
        }

        if (Netplay_IsRunning())
        {
                NETPLAY_CLOSE_SOCKET(s_netplaySocket);
                s_netplaySocket = NETPLAY_SOCKET_INVALID;
        }

        fprintf(stdout, "[Netplay] Disconnected\n");
        fflush(stdout);
}

/* === Public state accessors === */

int Netplay_GetState(void)              { return s_netplayState; }
int Netplay_GetLocalPlayerId(void)      { return s_localPlayerId; }
int Netplay_GetPlayerCount(void)        { return s_playerCount; }
int Netplay_GetExpectedPlayerCount(void){ return s_expectedPlayerCount; }

const struct NetplayPlayerInfo *Netplay_GetPlayers(int *count)
{
        static struct NetplayPlayerInfo info[NETPLAY_MAX_PLAYERS];
        int i;

        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (s_peers[i].active)
                {
                        info[i].id = s_peers[i].id;
                        info[i].connected = 1;
                        info[i].ready = s_peers[i].ready;
                        info[i].pingMs = s_peers[i].pingMs;
                        info[i].lastFrameReceived = s_peers[i].lastFrameReceived;
                        memcpy(info[i].name, s_peers[i].name, sizeof(info[i].name));
                }
                else
                {
                        info[i].id = (u8)i;
                        info[i].connected = 0;
                        info[i].ready = 0;
                        info[i].pingMs = 0;
                        info[i].lastFrameReceived = 0;
                        memset(info[i].name, 0, sizeof(info[i].name));
                }
        }

        /* If we're the host, also fill in our own slot (player 0). */
        if (Netplay_IsHost())
        {
                info[0].id = 0;
                info[0].connected = 1;
                info[0].ready = (u8)(s_localReady ? 1 : 0);
                info[0].pingMs = 0;
                info[0].lastFrameReceived = 0;
                memcpy(info[0].name, s_localPlayerName, sizeof(info[0].name));
        }

        if (count != NULL)
                *count = NETPLAY_MAX_PLAYERS;

        return info;
}

const char *Netplay_GetPlayerName(u8 playerId)
{
        if (playerId >= NETPLAY_MAX_PLAYERS)
                return "???";
        if (playerId == s_localPlayerId)
                return s_localPlayerName[0] ? s_localPlayerName : "Me";
        if (s_playerNames[playerId][0])
                return s_playerNames[playerId];
        return "???";
}

const char *Netplay_GetLocalPlayerName(void)
{
        return s_localPlayerName[0] ? s_localPlayerName : "Me";
}

/* === Send / receive inputs === */

void Netplay_SendGamepadState(u32 frameNum, u32 buttonsHeld, u32 buttonsTapped,
                              u32 buttonsReleased, s16 stickLX, s16 stickLY,
                              s16 stickRX, s16 stickRY)
{
        struct NetplayInputPayload input;

        if (s_netplayState != NETPLAY_STATE_HOSTING && s_netplayState != NETPLAY_STATE_CONNECTED)
                return;

        input.frameNum = frameNum;
        input.buttonsHeld = buttonsHeld;
        input.buttonsTapped = buttonsTapped;
        input.buttonsReleased = buttonsReleased;
        input.stickLX = stickLX;
        input.stickLY = stickLY;
        input.stickRX = stickRX;
        input.stickRY = stickRY;

        /* Store in history (kept for future rollback) */
        {
                struct NetplayFrameInput *hist = &s_inputHistory[s_inputHistoryWrite % NETPLAY_FRAME_HISTORY];

                hist->frameNum = frameNum;
                hist->buttonsHeld[s_localPlayerId] = buttonsHeld;
                hist->buttonsTapped[s_localPlayerId] = buttonsTapped;
                hist->buttonsReleased[s_localPlayerId] = buttonsReleased;
                hist->stickLX[s_localPlayerId] = stickLX;
                hist->stickLY[s_localPlayerId] = stickLY;
                hist->stickRX[s_localPlayerId] = stickRX;
                hist->stickRY[s_localPlayerId] = stickRY;
                hist->hasInput[s_localPlayerId] = 1;
                s_inputHistoryWrite++;
        }

        if (Netplay_IsHost())
                Netplay_BroadcastPacket(NETPLAY_PACKET_INPUT, (u16)sizeof(input), &input);
        else
                Netplay_SendPacket(NETPLAY_PACKET_INPUT, (u16)sizeof(input), &input, &s_hostAddr);
}

int Netplay_ReceiveInputs(struct NetplayInput *inputs, int maxInputs)
{
        int count = 0;

        while (s_inputQueueHead != s_inputQueueTail && count < maxInputs)
        {
                inputs[count] = s_inputQueue[s_inputQueueHead];
                s_inputQueueHead = (s_inputQueueHead + 1) % (int)len(s_inputQueue);
                count++;
        }

        return count;
}

int Netplay_ReceiveInputsForFrame(struct NetplayInput *inputs, int maxInputs, u32 expectedFrameNum)
{
        int count = 0;

        while (s_inputQueueHead != s_inputQueueTail)
        {
                struct NetplayInput *in = &s_inputQueue[s_inputQueueHead];

                /* Stop at future inputs (preserve queue order) */
                if ((int)(in->frameNum - expectedFrameNum) > 0)
                        break;

                /* Dequeue this entry */
                s_inputQueueHead = (s_inputQueueHead + 1) % (int)len(s_inputQueue);

                if (in->frameNum == expectedFrameNum)
                {
                        if (count < maxInputs)
                        {
                                /* Deduplicate: if we already have this playerId, replace with newer */
                                int dup = 0;
                                int j;
                                for (j = 0; j < count; j++)
                                {
                                        if (inputs[j].playerId == in->playerId)
                                        {
                                                inputs[j] = *in;
                                                dup = 1;
                                                break;
                                        }
                                }
                                if (!dup)
                                        inputs[count++] = *in;
                        }
                }
                /* else: stale input, silently discard */
        }

        return count;
}

void Netplay_GetLatestRemoteInput(u8 playerId, struct NetplayInput *out)
{
        if (out != NULL)
        {
                out->frameNum = s_lastRemoteFrame[playerId];
                out->playerId = playerId;
                out->buttonsHeld = s_lastRemoteInput[playerId].buttonsHeld;
                /* buttonsTapped / Released are single-frame signals: reusing a
                 * previous frame's value would cause "turbo" effects where the
                 * tap appears to repeat every frame until the next packet
                 * arrives. Leaving them at 0 avoids the turbo effect; the
                 * occasional dropped tap is preferable. */
                out->buttonsTapped = 0;
                out->buttonsReleased = 0;
                out->stickLX = s_lastRemoteInput[playerId].stickLX;
                out->stickLY = s_lastRemoteInput[playerId].stickLY;
                out->stickRX = s_lastRemoteInput[playerId].stickRX;
                out->stickRY = s_lastRemoteInput[playerId].stickRY;
        }
}

/* === State packets === */

void Netplay_SendStatePacket(const struct NetplayStatePayload *state)
{
        if (s_netplayState != NETPLAY_STATE_HOSTING && s_netplayState != NETPLAY_STATE_CONNECTED)
                return;

        if (Netplay_IsHost())
                Netplay_BroadcastPacket(NETPLAY_PACKET_STATE, (u16)sizeof(*state), state);
        else
                Netplay_SendPacket(NETPLAY_PACKET_STATE, (u16)sizeof(*state), state, &s_hostAddr);
}

int Netplay_DequeueState(u8 playerId, struct NetplayStatePayload *out)
{
        if (out != NULL && s_receivedStateFrame[playerId] != 0)
        {
                *out = s_receivedState[playerId];
                return 1;
        }
        return 0;
}

void Netplay_ClearState(u8 playerId)
{
        s_receivedStateFrame[playerId] = 0;
}

/* === Crate hits === */

void Netplay_QueueCrateHit(const struct NetplayCrateHit *hit)
{
        if (hit == NULL) return;
        {
                int tail = s_crateHitQueueTail;
                int next = (tail + 1) % NETPLAY_CRATE_QUEUE_MAX;
                if (next != s_crateHitQueueHead)
                {
                        s_crateHitQueue[tail] = *hit;
                        s_crateHitQueueTail = next;
                }
        }

        /* Broadcast to peers */
        if (s_netplayState == NETPLAY_STATE_HOSTING || s_netplayState == NETPLAY_STATE_CONNECTED)
        {
                if (Netplay_IsHost())
                        Netplay_BroadcastPacket(NETPLAY_PACKET_CRATE_HIT,
                                                (u16)sizeof(*hit), hit);
                else
                        Netplay_SendPacket(NETPLAY_PACKET_CRATE_HIT,
                                           (u16)sizeof(*hit), hit, &s_hostAddr);
        }
}

int Netplay_DequeueCrateHit(struct NetplayCrateHit *out)
{
        if (out != NULL && s_crateHitQueueHead != s_crateHitQueueTail)
        {
                *out = s_crateHitQueue[s_crateHitQueueHead];
                s_crateHitQueueHead = (s_crateHitQueueHead + 1) % NETPLAY_CRATE_QUEUE_MAX;
                return 1;
        }
        return 0;
}

/* === Finished === */

void Netplay_MarkRemoteFinished(u8 playerId)
{
        if (playerId < NETPLAY_MAX_PLAYERS)
                s_remoteFinished[playerId] = 1;
}

void Netplay_IsRemoteFinished(u8 playerId)
{
        /* (declared returning void in the header for ABI compat; use
         * Netplay_AnyRemoteFinished() or read s_remoteFinished[] directly) */
        (void)playerId;
}

int Netplay_AnyRemoteFinished(void)
{
        int i;
        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                if (s_remoteFinished[i])
                        return 1;
        return 0;
}

void Netplay_ClearRemoteFinished(void)
{
        memset(s_remoteFinished, 0, sizeof(s_remoteFinished));
}

/* === Callbacks / name === */

void Netplay_SetJoinCallback(NetplayEventFn cb)   { s_onPlayerJoin = cb; }
void Netplay_SetLeaveCallback(NetplayEventFn cb)  { s_onPlayerLeave = cb; }

void Netplay_SetPlayerName(const char *name)
{
        memset(s_localPlayerName, 0, NETPLAY_PLAYER_NAME_MAX);
        if (name != NULL)
                strncpy(s_localPlayerName, name, NETPLAY_PLAYER_NAME_MAX - 1);
}

/* === Lobby: ready / return-to-lobby === */

void Netplay_SetLocalReady(int ready)
{
        s_localReady = ready ? 1 : 0;

        /* Broadcast to peers */
        if (s_netplayState == NETPLAY_STATE_HOSTING || s_netplayState == NETPLAY_STATE_CONNECTED)
        {
                u8 payload[2];
                payload[0] = s_localPlayerId;
                payload[1] = (u8)(s_localReady ? 1 : 0);
                if (Netplay_IsHost())
                        Netplay_BroadcastPacket(NETPLAY_PACKET_PLAYER_READY, sizeof(payload), payload);
                else
                        Netplay_SendPacket(NETPLAY_PACKET_PLAYER_READY, sizeof(payload), payload, &s_hostAddr);
        }

        fprintf(stdout, "[Netplay] Local player %s ready\n", s_localReady ? "is" : "is NOT");
        fflush(stdout);
}

int Netplay_IsLocalReady(void) { return s_localReady; }

int Netplay_IsPeerReady(u8 playerId)
{
        if (playerId == s_localPlayerId)
                return s_localReady;
        return (s_readyMask & (1u << playerId)) != 0;
}

int Netplay_IsEveryoneReady(void)
{
        int i;
        if (!s_localReady)
                return 0;

        /* If host with expected count: require that many peers ready. Otherwise
         * require all active peers ready. */
        if (Netplay_IsHost() && s_expectedPlayerCount > 0)
        {
                int readyCount = 1; /* local */
                for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                {
                        if (s_peers[i].active && s_peers[i].ready)
                                readyCount++;
                }
                return readyCount >= s_expectedPlayerCount;
        }
        else
        {
                for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                {
                        if (s_peers[i].active && !s_peers[i].ready)
                                return 0;
                }
                return 1;
        }
}

void Netplay_BroadcastReturnToLobby(void)
{
        if (Netplay_IsHost())
                Netplay_BroadcastPacket(NETPLAY_PACKET_RETURN_LOBBY, 0, NULL);
        /* Clients cannot broadcast this; they can only set the local flag for themselves */
}

int Netplay_ConsumeReturnToLobby(void)
{
        if (s_returnToLobbyPending)
        {
                s_returnToLobbyPending = 0;
                g_NetplayReturnToLobby = 0;
                return 1;
        }
        return 0;
}

/* === Lobby: chat === */

void Netplay_SendChat(const char *message)
{
        struct NetplayChatPayload chat;
        if (message == NULL)
                return;

        memset(&chat, 0, sizeof(chat));
        chat.senderId = s_localPlayerId;
        strncpy(chat.message, message, NETPLAY_CHAT_MSG_MAX - 1);
        chat.message[NETPLAY_CHAT_MSG_MAX - 1] = '\0';

        if (s_netplayState == NETPLAY_STATE_HOSTING)
        {
                /* Host: enqueue locally + broadcast to clients */
                int tail = s_chatQueueTail;
                int next = (tail + 1) % NETPLAY_CHAT_QUEUE_MAX;
                if (next != s_chatQueueHead)
                {
                        s_chatQueue[tail] = chat;
                        s_chatQueueTail = next;
                }
                Netplay_BroadcastPacket(NETPLAY_PACKET_CHAT, sizeof(chat), &chat);
        }
        else if (s_netplayState == NETPLAY_STATE_CONNECTED)
        {
                Netplay_SendPacket(NETPLAY_PACKET_CHAT, sizeof(chat), &chat, &s_hostAddr);
        }
}

int Netplay_DequeueChat(struct NetplayChatPayload *out)
{
        if (out != NULL && s_chatQueueHead != s_chatQueueTail)
        {
                *out = s_chatQueue[s_chatQueueHead];
                s_chatQueueHead = (s_chatQueueHead + 1) % NETPLAY_CHAT_QUEUE_MAX;
                return 1;
        }
        return 0;
}

/* === Loaded mask === */

void Netplay_MarkLocalLoaded(void)
{
        g_NetplayLocalLoaded = 1;

        /* Send LOADED to peers */
        if (s_netplayState == NETPLAY_STATE_HOSTING || s_netplayState == NETPLAY_STATE_CONNECTED)
        {
                if (Netplay_IsHost())
                        Netplay_BroadcastPacket(NETPLAY_PACKET_LOADED, 0, NULL);
                else
                        Netplay_SendPacket(NETPLAY_PACKET_LOADED, 0, NULL, &s_hostAddr);
        }
}

void Netplay_ClearLocalLoaded(void)
{
        g_NetplayLocalLoaded = 0;
        s_loadedMask = 0;
        {
                int i;
                for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                        s_peers[i].loaded = 0;
        }
}

int Netplay_IsLocalLoaded(void)
{
        return g_NetplayLocalLoaded;
}

int Netplay_IsEveryoneLoaded(void)
{
        int i;
        if (!g_NetplayLocalLoaded)
                return 0;
        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (s_peers[i].active && !s_peers[i].loaded)
                        return 0;
        }
        return 1;
}

/* === Pause sync === */

void Netplay_BroadcastPause(void)
{
        if (s_netplayState == NETPLAY_STATE_HOSTING)
                Netplay_BroadcastPacket(NETPLAY_PACKET_PAUSE, 0, NULL);
        else if (s_netplayState == NETPLAY_STATE_CONNECTED)
                Netplay_SendPacket(NETPLAY_PACKET_PAUSE, 0, NULL, &s_hostAddr);
}

void Netplay_BroadcastUnpause(void)
{
        if (s_netplayState == NETPLAY_STATE_HOSTING)
                Netplay_BroadcastPacket(NETPLAY_PACKET_UNPAUSE, 0, NULL);
        else if (s_netplayState == NETPLAY_STATE_CONNECTED)
                Netplay_SendPacket(NETPLAY_PACKET_UNPAUSE, 0, NULL, &s_hostAddr);
}

int Netplay_ConsumeRemotePause(void)
{
        if (s_remotePausePending)
        {
                s_remotePausePending = 0;
                return 1;
        }
        return 0;
}

int Netplay_ConsumeRemoteUnpause(void)
{
        if (s_remoteUnpausePending)
        {
                s_remoteUnpausePending = 0;
                return 1;
        }
        return 0;
}

/* === Rejection feedback === */

int Netplay_GetRejectReason(void)
{
        return s_rejectReason;
}

const char *Netplay_GetRejectReasonString(int reason)
{
        switch (reason)
        {
        case NETPLAY_REJECT_NONE:             return "no error";
        case NETPLAY_REJECT_VERSION_MISMATCH: return "protocol version mismatch";
        case NETPLAY_REJECT_SERVER_FULL:      return "server full";
        case NETPLAY_REJECT_BAD_PROTOCOL:     return "bad protocol";
        default:                              return "unknown";
        }
}

/* === Post-race reset === */

void Netplay_ResetRaceState(void)
{
        g_NetplayRacing = 0;
        g_NetplayRaceStarting = 0;
        g_NetplayLocalLoaded = 0;
        g_NetplayRemoteLoaded = 0;
        g_NetplayStateRequested = 0;
        g_NetplayRemoteChecksumFrame = 0;
        g_NetplayRemoteChecksumValue = 0;
        g_NetplayDisconnected = 0;
        g_NetplayReturnToLobby = 0;
        g_NetplayHostCharacter = -1;
        g_NetplayClientCharacter = -1;

        s_loadedMask = 0;
        s_readyMask = 0;
        s_localReady = 0;
        s_remotePausePending = 0;
        s_remoteUnpausePending = 0;
        s_returnToLobbyPending = 0;
        s_chatQueueHead = 0;
        s_chatQueueTail = 0;
        s_crateHitQueueHead = 0;
        s_crateHitQueueTail = 0;
        s_inputQueueHead = 0;
        s_inputQueueTail = 0;

        memset(s_remoteFinished, 0, sizeof(s_remoteFinished));
        memset(s_receivedState, 0, sizeof(s_receivedState));
        memset(s_receivedStateFrame, 0, sizeof(s_receivedStateFrame));
        memset(s_lastRemoteInput, 0, sizeof(s_lastRemoteInput));
        memset(s_lastRemoteFrame, 0, sizeof(s_lastRemoteFrame));
        memset(s_delayBuffer, 0, sizeof(s_delayBuffer));
        s_delayWriteIndex = 0;

        /* Clear per-peer ready/loaded flags but keep the connection alive */
        {
                int i;
                for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                {
                        s_peers[i].ready = 0;
                        s_peers[i].loaded = 0;
                }
        }

        fprintf(stdout, "[Netplay] Race state reset (returning to lobby)\n");
        fflush(stdout);
}

/* === Crate ID helper === */

u32 Netplay_ComputeCrateID(int levelID, int instanceIndex)
{
        /* Simple FNV-1a-style hash; stable across host/client because both run
         * the same level with the same instance enumeration. */
        u32 h = 2166136261u;
        h ^= (u32)levelID;
        h *= 16777619u;
        h ^= (u32)instanceIndex;
        h *= 16777619u;
        return h;
}

/* === Peer timeout config === */

void Netplay_SetPeerTimeoutMs(u32 ms)
{
        if (ms >= 1000)
                s_peerTimeoutMs = ms;
}

/* === Lobby size config === */

void Netplay_SetExpectedPlayerCount(int count)
{
        if (count < 0)
                count = 0;
        if (count > NETPLAY_MAX_PLAYERS)
                count = NETPLAY_MAX_PLAYERS;
        s_expectedPlayerCount = (u8)count;
}

/* === Local-player-count helper === */

int Netplay_GetLocalPlayerCount(void)
{
        /* During a netplay race, each instance is single-player locally.
         * The engine's numPlyrCurrGame is set to 2-8 to represent the
         * connected peers (so the spawn logic in MainInit_Drivers creates
         * the right number of driver slots), but everything RENDER-related
         * (camera, LOD, pushBuffer, JitPool, HUD layout) should treat us
         * as 1P. */
        if (g_NetplayRacing)
                return 1;
        return 0; /* 0 = "use gGT->numPlyrCurrGame" — see inline macro */
}

/* === Item sync handlers === */

internal void Netplay_HandleItemPickup(const struct NetplayPacketHeader *header,
                                       const u8 *payload, int payloadSize)
{
        const struct NetplayItemPayload *ip = (const struct NetplayItemPayload *)payload;
        u8 playerId;

        if (payloadSize < (int)sizeof(struct NetplayItemPayload) || ip == NULL)
                return;

        playerId = ip->playerId;
        if (playerId >= NETPLAY_MAX_PLAYERS)
                return;

        s_itemPickupId[playerId] = ip->itemId;
        s_itemPickupNum[playerId] = ip->numHeldItems;
        s_itemPickupPending[playerId] = 1;

        /* Relay (host only) so other clients see the item pickup too */
        Netplay_RelayPacketToOthers(header, payload);
}

internal void Netplay_HandleItemUse(const struct NetplayPacketHeader *header,
                                    const u8 *payload, int payloadSize)
{
        const struct NetplayItemPayload *ip = (const struct NetplayItemPayload *)payload;
        u8 playerId;

        if (payloadSize < (int)sizeof(struct NetplayItemPayload) || ip == NULL)
                return;

        playerId = ip->playerId;
        if (playerId >= NETPLAY_MAX_PLAYERS)
                return;

        s_itemUseId[playerId] = ip->itemId;
        s_itemUsePending[playerId] = 1;

        /* Relay (host only) so other clients see the item use too */
        Netplay_RelayPacketToOthers(header, payload);
}

internal void Netplay_HandleRngSeed(const struct NetplayPacketHeader *header,
                                    const u8 *payload, int payloadSize)
{
        const struct NetplayRngSeedPayload *rs = (const struct NetplayRngSeedPayload *)payload;
        (void)header;
        if (payloadSize < (int)sizeof(struct NetplayRngSeedPayload) || rs == NULL)
                return;

        s_rngSeedPending = rs->seed;
        s_rngSeedFrame = rs->frameNum;
        s_rngSeedReady = 1;

        fprintf(stdout, "[Netplay] Received RNG seed 0x%08x (frame %u)\n",
                rs->seed, rs->frameNum);
        fflush(stdout);
}

/* === Item sync public API === */

void Netplay_BroadcastItemPickup(u8 playerId, u8 itemId, u8 numHeldItems, u32 frameNum)
{
        struct NetplayItemPayload ip;
        if (s_netplayState != NETPLAY_STATE_HOSTING && s_netplayState != NETPLAY_STATE_CONNECTED)
                return;

        memset(&ip, 0, sizeof(ip));
        ip.playerId = playerId;
        ip.itemId = itemId;
        ip.numHeldItems = numHeldItems;
        ip.reserved = 0;
        ip.frameNum = frameNum;

        if (Netplay_IsHost())
                Netplay_BroadcastPacket(NETPLAY_PACKET_ITEM_PICKUP, sizeof(ip), &ip);
        else
                Netplay_SendPacket(NETPLAY_PACKET_ITEM_PICKUP, sizeof(ip), &ip, &s_hostAddr);
}

void Netplay_BroadcastItemUse(u8 playerId, u8 itemId, u32 frameNum)
{
        struct NetplayItemPayload ip;
        if (s_netplayState != NETPLAY_STATE_HOSTING && s_netplayState != NETPLAY_STATE_CONNECTED)
                return;

        memset(&ip, 0, sizeof(ip));
        ip.playerId = playerId;
        ip.itemId = itemId;
        ip.numHeldItems = 0;
        ip.reserved = 0;
        ip.frameNum = frameNum;

        if (Netplay_IsHost())
                Netplay_BroadcastPacket(NETPLAY_PACKET_ITEM_USE, sizeof(ip), &ip);
        else
                Netplay_SendPacket(NETPLAY_PACKET_ITEM_USE, sizeof(ip), &ip, &s_hostAddr);
}

int Netplay_DequeueItemPickup(u8 playerId, u8 *outItemId, u8 *outNumItems)
{
        if (playerId >= NETPLAY_MAX_PLAYERS)
                return 0;
        if (!s_itemPickupPending[playerId])
                return 0;

        if (outItemId) *outItemId = s_itemPickupId[playerId];
        if (outNumItems) *outNumItems = s_itemPickupNum[playerId];
        s_itemPickupPending[playerId] = 0;
        return 1;
}

int Netplay_DequeueItemUse(u8 playerId, u8 *outItemId)
{
        if (playerId >= NETPLAY_MAX_PLAYERS)
                return 0;
        if (!s_itemUsePending[playerId])
                return 0;

        if (outItemId) *outItemId = s_itemUseId[playerId];
        s_itemUsePending[playerId] = 0;
        return 1;
}

/* === RNG seed API === */

void Netplay_BroadcastRngSeed(u32 seed, u32 frameNum)
{
        struct NetplayRngSeedPayload rs;
        if (s_netplayState != NETPLAY_STATE_HOSTING)
                return;

        rs.seed = seed;
        rs.frameNum = frameNum;
        Netplay_BroadcastPacket(NETPLAY_PACKET_RNG_SEED, sizeof(rs), &rs);

        fprintf(stdout, "[Netplay] Sent RNG seed 0x%08x (frame %u)\n", seed, frameNum);
        fflush(stdout);
}

int Netplay_ConsumeRngSeed(u32 *outSeed, u32 *outFrameNum)
{
        if (!s_rngSeedReady)
                return 0;
        s_rngSeedReady = 0;
        if (outSeed) *outSeed = s_rngSeedPending;
        if (outFrameNum) *outFrameNum = s_rngSeedFrame;
        return 1;
}

/* === Chat window (separate OS window, SDL-based) === */

#if defined(CTR_NATIVE)
#include <SDL3/SDL.h>
#endif

#define NETPLAY_CHAT_HISTORY_MAX 50
#define NETPLAY_CHAT_LINE_MAX    80

global_variable SDL_Window *s_chatWindow;
global_variable char s_chatHistory[NETPLAY_CHAT_HISTORY_MAX][NETPLAY_CHAT_LINE_MAX];
global_variable int s_chatHistoryCount;
global_variable int s_chatScrollOffset;

void Netplay_ChatPrint(const char *text)
{
        if (!s_chatWindowOpen || text == NULL)
                return;

        /* Shift history up if full */
        if (s_chatHistoryCount >= NETPLAY_CHAT_HISTORY_MAX)
        {
                int i;
                for (i = 1; i < NETPLAY_CHAT_HISTORY_MAX; i++)
                        memcpy(s_chatHistory[i - 1], s_chatHistory[i], NETPLAY_CHAT_LINE_MAX);
                s_chatHistoryCount = NETPLAY_CHAT_HISTORY_MAX - 1;
        }

        /* Add new line, truncated to fit */
        snprintf(s_chatHistory[s_chatHistoryCount], NETPLAY_CHAT_LINE_MAX, "%.79s", text);
        s_chatHistoryCount++;
        s_chatScrollOffset = 0; /* auto-scroll to bottom on new message */

        /* Also print to the terminal so the player can see messages even
         * though the chat window itself has no renderer (we deliberately
         * don't create one to avoid OpenGL context conflicts with the
         * game's main renderer — see Netplay_OpenChatWindow for details). */
        fprintf(stdout, "[Chat] %s\n", text);
        fflush(stdout);
}

int Netplay_IsChatWindowOpen(void)
{
        return s_chatWindowOpen;
}

int Netplay_OpenChatWindow(void)
{
        if (s_chatWindowOpen)
                return 1;

        s_chatLineLen = 0;
        s_chatLineBuf[0] = '\0';
        s_chatHistoryCount = 0;
        s_chatScrollOffset = 0;

        /* Create a dedicated SDL window for chat. We deliberately do NOT
         * create an SDL renderer for this window because the game already
         * has an OpenGL context on the main window, and creating a second
         * GL context on a different window causes Mesa/Intel drivers to
         * crash inside SDL_RenderPresent (the contexts share state and
         * corrupt each other's command queues).
         *
         * Instead, this window exists ONLY to capture keyboard input
         * (SDL_StartTextInput + SDL_EVENT_TEXT_INPUT). The chat content
         * is rendered by the game itself in its own HUD (the small
         * notification line in the lobby). Messages received are also
         * logged to stdout/stderr so they're visible in the terminal
         * that launched the game. */
        s_chatWindow = SDL_CreateWindow("CTR Netplay Chat (type here)", 200, 40, SDL_WINDOW_HIDDEN);
        if (s_chatWindow == NULL)
        {
                fprintf(stderr, "[Netplay] Failed to create chat window: %s\n", SDL_GetError());
                return 0;
        }

        /* Start accepting text input. SDL will send SDL_EVENT_TEXT_INPUT
         * events for each character typed, and SDL_EVENT_KEY_DOWN for
         * special keys (Enter, Backspace, etc.). These are processed
         * via Netplay_HandleSDLEvent() from the main event loop. */
        SDL_StartTextInput(s_chatWindow);

        /* Show the window now that text input is active */
        SDL_ShowWindow(s_chatWindow);

        s_chatWindowOpen = 1;

        Netplay_ChatPrint("=== CTR Netplay Chat ===");
        Netplay_ChatPrint("Type a message and press Enter to send.");
        Netplay_ChatPrint("(ESC to close, messages also print to terminal)");

        return 1;
}

void Netplay_CloseChatWindow(void)
{
        if (!s_chatWindowOpen)
                return;

        Netplay_ChatPrint("=== Chat closed ===");

        if (s_chatWindow)
        {
                SDL_StopTextInput(s_chatWindow);
                SDL_HideWindow(s_chatWindow);
                SDL_DestroyWindow(s_chatWindow);
                s_chatWindow = NULL;
        }

        s_chatWindowOpen = 0;
        s_chatLineLen = 0;
        s_chatLineBuf[0] = '\0';
        s_chatHistoryCount = 0;
}

/* Update the chat window title to show what the user is typing.
 * Since we can't render text inside the window (no SDL renderer to avoid
 * OpenGL context conflicts), we use the window TITLE as a simple text
 * display. SDL_SetWindowTitle is cheap and works on all platforms. */
internal void Netplay_RenderChatWindow(void)
{
        static int s_lastUpdateFrame = -1;
        static char s_lastTitle[128];
        char title[128];

        if (!s_chatWindowOpen || s_chatWindow == NULL)
                return;

        /* Only update when the input buffer changes or for cursor blink.
         * Uses SDL_GetTicks for the blink timer. */
        {
                Uint32 ticks = SDL_GetTicks();
                int blink = (ticks / 500) & 1; /* blink every 500ms */
                snprintf(title, sizeof(title), "%s%s",
                         s_chatLineBuf[0] ? s_chatLineBuf : "(type message...)",
                         blink ? "_" : " ");

                if (strcmp(title, s_lastTitle) != 0)
                {
                        strncpy(s_lastTitle, title, sizeof(s_lastTitle) - 1);
                        s_lastTitle[sizeof(s_lastTitle) - 1] = '\0';
                        SDL_SetWindowTitle(s_chatWindow, title);
                }
        }
}

/* Process an SDL event for the chat window. Returns 1 if consumed. */
int Netplay_HandleSDLEvent(const void *sdlEvent)
{
        const SDL_Event *event = (const SDL_Event *)sdlEvent;

        if (!s_chatWindowOpen || s_chatWindow == NULL)
                return 0;

        /* Filter to events for our chat window. Most SDL events have
         * windowID in the same place (it's the first field of most
         * event sub-structs). We check the common ones explicitly. */
        {
                Uint32 eventWindowID = 0;
                switch (event->type)
                {
                case SDL_EVENT_TEXT_INPUT:
                        eventWindowID = event->text.windowID;
                        break;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                        eventWindowID = event->key.windowID;
                        break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                case SDL_EVENT_WINDOW_RESIZED:
                        eventWindowID = event->window.windowID;
                        break;
                default:
                        /* For other event types, check if they have a windowID.
                         * SDL_EVENT_TEXT_EDITING, mouse events, etc. all have
                         * windowID as first field. We use a safe cast. */
                        if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST)
                                eventWindowID = event->window.windowID;
                        else
                                return 0; /* not a window event */
                        break;
                }

                if (eventWindowID != SDL_GetWindowID(s_chatWindow))
                        return 0;
        }

        /* This event is for our chat window — process it */
        switch (event->type)
        {
        case SDL_EVENT_TEXT_INPUT:
        {
                const char *text = event->text.text;
                int textLen = (int)strlen(text);
                int i;
                for (i = 0; i < textLen; i++)
                {
                        char c = text[i];
                        if (c >= 32 && c < 127)
                        {
                                if (s_chatLineLen < (int)sizeof(s_chatLineBuf) - 1)
                                {
                                        s_chatLineBuf[s_chatLineLen++] = c;
                                        s_chatLineBuf[s_chatLineLen] = '\0';
                                }
                        }
                }
                return 1;
        }

        case SDL_EVENT_KEY_DOWN:
        {
                SDL_Keycode key = event->key.key;
                if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                {
                        if (s_chatLineLen > 0)
                        {
                                s_chatLineBuf[s_chatLineLen] = '\0';
                                Netplay_SendChat(s_chatLineBuf);
                                {
                                        char echo[NETPLAY_CHAT_MSG_MAX + 16];
                                        snprintf(echo, sizeof(echo), "> %s (you)", s_chatLineBuf);
                                        Netplay_ChatPrint(echo);
                                }
                                s_chatLineLen = 0;
                                s_chatLineBuf[0] = '\0';
                        }
                }
                else if (key == SDLK_BACKSPACE)
                {
                        if (s_chatLineLen > 0)
                        {
                                s_chatLineLen--;
                                s_chatLineBuf[s_chatLineLen] = '\0';
                        }
                }
                else if (key == SDLK_ESCAPE)
                {
                        Netplay_CloseChatWindow();
                }
                else if (key == SDLK_PAGEUP)
                {
                        s_chatScrollOffset += 5;
                        if (s_chatScrollOffset > s_chatHistoryCount)
                                s_chatScrollOffset = s_chatHistoryCount;
                }
                else if (key == SDLK_PAGEDOWN)
                {
                        s_chatScrollOffset -= 5;
                        if (s_chatScrollOffset < 0)
                                s_chatScrollOffset = 0;
                }
                return 1;
        }

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                Netplay_CloseChatWindow();
                return 1;

        case SDL_EVENT_QUIT:
                /* Don't let the chat window's close quit the whole app.
                 * Return 1 to consume the event. */
                return 1;
        }

        return 0;
}

/* Render the chat window. Called from Netplay_Poll every frame.
 * Does NOT poll SDL events — those come from the game's main loop via
 * Netplay_HandleSDLEvent(). */
internal void Netplay_PollChatInput(void)
{
        if (!s_chatWindowOpen || s_chatWindow == NULL)
                return;

        /* Just render; events are handled via Netplay_HandleSDLEvent */
        Netplay_RenderChatWindow();
}

/* === Network interface enumeration (structured, for in-game UI) === */

int Netplay_GetInterfaceList(struct NetplayInterface *out, int maxEntries)
{
        int count = 0;

        /* Always reset cache */
        s_ifaceCount = 0;
        s_ifaceSelected = -1;
        memset(s_ifaceList, 0, sizeof(s_ifaceList));

        if (out == NULL || maxEntries <= 0)
                return 0;

#if defined(_WIN32)
        {
                ULONG bufLen = 0;
                IP_ADAPTER_ADDRESSES *addrs = NULL;
                IP_ADAPTER_ADDRESSES *a;

                GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &bufLen);
                if (bufLen == 0)
                        return 0;
                addrs = (IP_ADAPTER_ADDRESSES *)malloc(bufLen);
                if (addrs == NULL)
                        return 0;
                if (GetAdaptersAddresses(AF_INET, 0, NULL, addrs, &bufLen) != NO_ERROR)
                {
                        free(addrs);
                        return 0;
                }

                for (a = addrs; a != NULL && count < maxEntries && count < NETPLAY_IFACE_LIST_MAX; a = a->Next)
                {
                        IP_ADAPTER_UNICAST_ADDRESS *ua;
                        if (a->OperStatus != IfOperStatusUp)
                                continue;
                        ua = a->FirstUnicastAddress;
                        if (ua == NULL)
                                continue;
                        {
                                struct sockaddr_in *sin = (struct sockaddr_in *)ua->Address.lpSockaddr;
                                u32 ip;
                                char cleanName[NETPLAY_IFACE_NAME_MAX];
                                if (sin->sin_family != AF_INET)
                                        continue;
                                ip = ntohl(sin->sin_addr.s_addr);
                                if ((ip & 0xFF000000) == 0x7F000000)
                                        continue;

                                memset(&s_ifaceList[count], 0, sizeof(s_ifaceList[count]));

                                /* Get the clean friendly name (without IP) using wcstombs
                                 * which is C89-standard and available on all Windows versions
                                 * (WideCharToMultiByte has had issues on older SDKs). */
                                cleanName[0] = '\0';
                                if (a->FriendlyName != NULL)
                                {
                                        size_t wn = wcstombs(cleanName, a->FriendlyName,
                                                              sizeof(cleanName) - 1);
                                        if (wn == (size_t)-1)
                                                cleanName[0] = '\0';
                                        else
                                                cleanName[wn] = '\0';
                                }
                                if (cleanName[0] == '\0')
                                {
                                        /* Fall back to adapter GUID name */
                                        snprintf(cleanName, sizeof(cleanName), "adapter%d", count);
                                }

                                /* Clean name (for SetInterfaceName) */
                                snprintf(s_ifaceList[count].ifaceName,
                                         sizeof(s_ifaceList[count].ifaceName),
                                         "%s", cleanName);

                                /* Display name with IP appended */
                                snprintf(s_ifaceList[count].name,
                                         sizeof(s_ifaceList[count].name),
                                         "%s (%u.%u.%u.%u)",
                                         cleanName,
                                         (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                                         (ip >> 8) & 0xFF, ip & 0xFF);

                                s_ifaceList[count].ip = ip;
                                snprintf(s_ifaceList[count].ipString,
                                         sizeof(s_ifaceList[count].ipString),
                                         "%u.%u.%u.%u",
                                         (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                                         (ip >> 8) & 0xFF, ip & 0xFF);

                                out[count] = s_ifaceList[count];
                                count++;
                        }
                }
                free(addrs);
        }
#elif defined(__GNUC__) && !defined(_WIN32)
        {
                struct ifaddrs *ifaddr, *ifa;

                if (getifaddrs(&ifaddr) == -1)
                        return 0;

                for (ifa = ifaddr; ifa != NULL && count < maxEntries && count < NETPLAY_IFACE_LIST_MAX; ifa = ifa->ifa_next)
                {
                        u32 ip;
                        struct sockaddr_in *sin;
                        if (ifa->ifa_addr == NULL)
                                continue;
                        if (ifa->ifa_addr->sa_family != AF_INET)
                                continue;
                        sin = (struct sockaddr_in *)ifa->ifa_addr;
                        ip = ntohl(sin->sin_addr.s_addr);
                        if ((ip & 0xFF000000) == 0x7F000000)
                                continue;

                        memset(&s_ifaceList[count], 0, sizeof(s_ifaceList[count]));

                        /* Clean interface name (e.g. "eth0", "wlan0") */
                        snprintf(s_ifaceList[count].ifaceName,
                                 sizeof(s_ifaceList[count].ifaceName),
                                 "%s", ifa->ifa_name);

                        /* Display name with IP appended */
                        snprintf(s_ifaceList[count].name,
                                 sizeof(s_ifaceList[count].name),
                                 "%s (%u.%u.%u.%u)",
                                 ifa->ifa_name,
                                 (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                                 (ip >> 8) & 0xFF, ip & 0xFF);

                        s_ifaceList[count].ip = ip;
                        snprintf(s_ifaceList[count].ipString,
                                 sizeof(s_ifaceList[count].ipString),
                                 "%u.%u.%u.%u",
                                 (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                                 (ip >> 8) & 0xFF, ip & 0xFF);

                        out[count] = s_ifaceList[count];
                        count++;
                }
                freeifaddrs(ifaddr);
        }
#else
        return 0;
#endif

        s_ifaceCount = count;
        return count;
}

const char *Netplay_GetSelectedInterfaceIP(void)
{
        if (s_ifaceSelected < 0 || s_ifaceSelected >= s_ifaceCount)
                return NULL;
        return s_ifaceList[s_ifaceSelected].ipString;
}

const char *Netplay_GetInterfaceIPByIndex(int index)
{
        if (index < 0 || index >= s_ifaceCount)
                return NULL;
        return s_ifaceList[index].ipString;
}

void Netplay_SetAddressString(const char *ipString, u16 port)
{
        if (ipString == NULL || ipString[0] == '\0')
        {
                s_addressString[0] = '\0';
                return;
        }
        snprintf(s_addressString, sizeof(s_addressString), "%s:%u", ipString, port);
}

/* === Poll loop === */

internal void Netplay_CheckPeerTimeouts(void)
{
        u32 now = Netplay_GetTimestampMs();
        int i;

        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                if (!s_peers[i].active)
                        continue;

                if (now - s_peers[i].lastSeenMs > s_peerTimeoutMs)
                {
                        fprintf(stdout, "[Netplay] Peer %u timed out (no traffic for %u ms)\n",
                                s_peers[i].id, now - s_peers[i].lastSeenMs);
                        fflush(stdout);
                        Netplay_RemovePeer(s_peers[i].id);
                        if (g_NetplayRacing)
                                g_NetplayDisconnected = 1;
                }
        }
}

internal void Netplay_CheckHelloResend(void)
{
        /* Client side: resend HELLO every NETPLAY_HELLO_RESEND_MS until we get
         * an accept or until NETPLAY_HELLO_TIMEOUT_MS elapses. */
        if (s_netplayState != NETPLAY_STATE_CONNECTING)
                return;

        {
                u32 now = Netplay_GetTimestampMs();

                if (now - s_connectStartMs > NETPLAY_HELLO_TIMEOUT_MS)
                {
                        fprintf(stderr, "[Netplay] Connect timeout, giving up\n");
                        fflush(stderr);
                        s_netplayState = NETPLAY_STATE_DISCONNECTED;
                        s_rejectReason = NETPLAY_REJECT_BAD_PROTOCOL;
                        if (Netplay_IsRunning())
                        {
                                NETPLAY_CLOSE_SOCKET(s_netplaySocket);
                                s_netplaySocket = NETPLAY_SOCKET_INVALID;
                        }
                        return;
                }

                if (now - s_lastHelloSendMs >= NETPLAY_HELLO_RESEND_MS)
                {
                        struct NetplayHelloPayload hello;

                        s_lastHelloSendMs = now;
                        memset(&hello, 0, sizeof(hello));
                        strncpy((char *)hello.playerName, s_localPlayerName, NETPLAY_PLAYER_NAME_MAX - 1);
                        strncpy((char *)hello.gameVersion, "CTR-Native", NETPLAY_VERSION_STRING_MAX - 1);
                        hello.protocolVersion = NETPLAY_PROTOCOL_VERSION;
                        Netplay_SendPacket(NETPLAY_PACKET_HELLO, sizeof(hello), &hello, &s_hostAddr);
                }
        }
}

internal void Netplay_CheckConnectedTransition(void)
{
        /* If connecting, check if we got a HELLO back and became connected */
        if (s_netplayState == NETPLAY_STATE_CONNECTING)
        {
                int i;
                int hasHost = 0;

                for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                {
                        if (s_peers[i].active)
                        {
                                hasHost = 1;
                                if (s_localPlayerId == 0 && s_peers[i].id != 0)
                                {
                                        /* We got our player ID from the host */
                                        s_localPlayerId = s_peers[i].id;
                                }
                        }
                }

                /* If we have at least one peer and a non-zero ID, consider
                 * ourselves connected. */
                if (hasHost && s_localPlayerId != 0)
                {
                        s_netplayState = NETPLAY_STATE_CONNECTED;
                        fprintf(stdout, "[Netplay] Connected as player %d\n", s_localPlayerId);
                        fflush(stdout);
                }
        }
}

internal void Netplay_SendPeriodicPings(void)
{
        u32 now = Netplay_GetTimestampMs();

        if (now - s_lastPingTime < NETPLAY_PING_INTERVAL_MS)
                return;

        s_lastPingTime = now;

        if (s_netplayState == NETPLAY_STATE_HOSTING || s_netplayState == NETPLAY_STATE_CONNECTED)
        {
                int i;

                for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                {
                        if (s_peers[i].active)
                        {
                                struct NetplayPingPongPayload ping;

                                ping.timestamp = now;
                                s_peers[i].pingTimestamp = now;
                                Netplay_SendPacket(NETPLAY_PACKET_PING, sizeof(ping), &ping, &s_peers[i].addr);
                        }
                }
        }
}

void Netplay_Poll(void)
{
        /* Process all pending packets */
        while (Netplay_ReceivePacket())
        {
        }

        /* Poll the chat window for input (non-blocking) */
        Netplay_PollChatInput();

        /* Client: resend HELLO if needed */
        Netplay_CheckHelloResend();

        /* Both: check if connection completed */
        Netplay_CheckConnectedTransition();

        /* Drop dead peers */
        Netplay_CheckPeerTimeouts();

        /* Periodic ping */
        Netplay_SendPeriodicPings();

        /* Keep g_NetplayReadyMask in sync */
        g_NetplayReadyMask = s_readyMask;

        /* Keep g_NetplayRemoteLoaded for legacy code: 1 only if every active peer is loaded */
        {
                int i;
                int allLoaded = 1;
                int anyActive = 0;
                for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
                {
                        if (s_peers[i].active)
                        {
                                anyActive = 1;
                                if (!s_peers[i].loaded)
                                {
                                        allLoaded = 0;
                                        break;
                                }
                        }
                }
                g_NetplayRemoteLoaded = anyActive ? allLoaded : 0;
        }
}

