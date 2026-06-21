#include <platform/native_netplay.h>

#include <macros.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__GNUC__) && !defined(_WIN32)
#include <time.h>
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
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
#define NETPLAY_SOCKET_ERROR       (-1)
#define NETPLAY_SOCKET_INVALID     (-1)
#define NETPLAY_SOCKET             int
#define NETPLAY_CLOSE_SOCKET(s)    close(s)
#endif

// TODO(aalhendi): Use the project's own fourcc macros
#define NATIVE_NETPLAY_MAGIC      0x5054454e // 'NETP' little-endian

#define NETPLAY_MAX_PACKET_SIZE   1400
#define NETPLAY_PLAYER_NAME_MAX   32
#define NETPLAY_VERSION_STRING_MAX 16

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
	u32 lastFrameReceived;
	u32 pingTimestamp;
	u16 pingMs;
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

global_variable NETPLAY_SOCKET s_netplaySocket = NETPLAY_SOCKET_INVALID;
global_variable struct sockaddr_in s_hostAddr;
global_variable int s_netplayState;
global_variable u8  s_localPlayerId;
global_variable u8  s_playerCount;
global_variable u8 s_expectedPlayerCount;
global_variable u32 s_nextFrameToSend;

// Host state
global_variable struct NetplayPeer s_peers[NETPLAY_MAX_PLAYERS];
global_variable int s_peerCount;

// Input history ring buffer (for host: all inputs, for client: received inputs)
global_variable struct NetplayFrameInput s_inputHistory[NETPLAY_FRAME_HISTORY];
global_variable int s_inputHistoryWrite;

// Received inputs queue
global_variable struct NetplayInput s_inputQueue[NETPLAY_FRAME_HISTORY * NETPLAY_MAX_PLAYERS];
global_variable int s_inputQueueHead;
global_variable int s_inputQueueTail;

// Callbacks
global_variable NetplayEventFn s_onPlayerJoin;
global_variable NetplayEventFn s_onPlayerLeave;

// Player names
global_variable char s_playerNames[NETPLAY_MAX_PLAYERS][NETPLAY_PLAYER_NAME_MAX];
global_variable char s_localPlayerName[NETPLAY_PLAYER_NAME_MAX];
global_variable char s_addressString[64];
int g_NetplayAutoJoin;
int g_NetplayRaceStarting;
int g_NetplayRacing;
int g_NetplayHostCharacter;
int g_NetplayClientCharacter;
int g_NetplayTrackId;
int g_NetplayNumLaps;

// Windows-specific WSA init tracking
#if defined(_WIN32)
global_variable int s_wsaInitialized;
#endif

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

// Get OS timestamp in milliseconds for ping calculation
internal u32 Netplay_GetTimestampMs(void)
{
#if defined(_WIN32)
	return (u32)GetTickCount();
#elif defined(__GNUC__)
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (u32)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
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

internal void Netplay_QueueInput(u8 playerId, u32 frameNum, u32 buttonsHeld,
                                 u32 buttonsTapped, u32 buttonsReleased,
                                 s16 stickLX, s16 stickLY, s16 stickRX, s16 stickRY)
{
	int tail = s_inputQueueTail;
	int next = (tail + 1) % (int)len(s_inputQueue);

	if (next == s_inputQueueHead)
	{
		fprintf(stderr, "[Netplay] Input queue overflow, dropping input for frame %u player %d\n", frameNum, playerId);
		return;
	}

	s_inputQueue[tail].frameNum = frameNum;
	s_inputQueue[tail].playerId = playerId;
	s_inputQueue[tail].buttonsHeld = buttonsHeld;
	s_inputQueue[tail].buttonsTapped = buttonsTapped;
	s_inputQueue[tail].buttonsReleased = buttonsReleased;
	s_inputQueue[tail].stickLX = stickLX;
	s_inputQueue[tail].stickLY = stickLY;
	s_inputQueue[tail].stickRX = stickRX;
	s_inputQueue[tail].stickRY = stickRY;
	s_inputQueueTail = next;
}

const char *Netplay_GetAddressString(void)
{
	if (s_addressString[0] == '\0')
		return "unknown";
	return s_addressString;
}

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

// Find a free peer slot, returns -1 if full
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

// Find peer by address
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

// Find peer by player ID
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
	memset(&s_peers[index].addr, 0, sizeof(s_peers[index].addr));

	if (s_playerCount > 0)
		s_playerCount--;

	Netplay_ReportLeave(playerId);
}

internal void Netplay_HandleHello(const struct NetplayPacketHeader *header,
                                  const struct sockaddr_in *fromAddr,
                                  const u8 *payload, int payloadSize)
{
	const struct NetplayHelloPayload *hello = (const struct NetplayHelloPayload *)payload;
	int slot;
	u8 newPlayerId = 0;

	(void)payloadSize;

	if (!Netplay_IsHost())
	{
		// Client received HELLO from host — register host as a peer
		// and grab assigned player ID from payload
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
				s_peers[slot].pingTimestamp = 0;
				s_peers[slot].pingMs = 0;
				s_playerCount++;
			}
		}

		// Payload[0] = assigned player ID from host
		if (payload != NULL && payloadSize >= 1)
		{
			s_localPlayerId = payload[0];
			fprintf(stdout, "[Netplay] Assigned player ID %d\n", s_localPlayerId);
			fflush(stdout);
		}

		return;
	}

	if (Netplay_FindPeerByAddr(fromAddr) >= 0)
	{
		// Already connected, resend the current player list
		goto send_accept;
	}

	slot = Netplay_FindFreePeerSlot();
	if (slot < 0)
	{
		fprintf(stderr, "[Netplay] Server full, rejecting connection\n");
		return;
	}

	// Assign player ID (never assign 0, that's the host)
	newPlayerId = (u8)(slot + 1);
	if (newPlayerId == 0)
		newPlayerId = (u8)(slot + 2);

	// Verify we don't exceed max players
	if (s_playerCount >= NETPLAY_MAX_PLAYERS)
	{
		fprintf(stderr, "[Netplay] Max players reached\n");
		return;
	}

	s_peers[slot].addr = *fromAddr;
	s_peers[slot].id = newPlayerId;
	s_peers[slot].active = 1;
	s_peers[slot].lastFrameReceived = 0;
	s_peers[slot].pingTimestamp = 0;
	s_peers[slot].pingMs = 0;

	s_playerCount++;

	if (hello != NULL && payloadSize >= (int)sizeof(struct NetplayHelloPayload))
	{
		memcpy(s_playerNames[newPlayerId], hello->playerName, NETPLAY_PLAYER_NAME_MAX);
		s_playerNames[newPlayerId][NETPLAY_PLAYER_NAME_MAX - 1] = '\0';
	}

	fprintf(stdout, "[Netplay] New connection from %s:%d → player %d\n",
	        inet_ntoa(fromAddr->sin_addr), ntohs(fromAddr->sin_port), newPlayerId);
	fflush(stdout);

	Netplay_ReportJoin(newPlayerId);

send_accept:
	// Send accept with assigned player ID in payload
	{
		struct sockaddr_in clientAddr = *fromAddr;
		u8 acceptData[2];
		int existingSlot = Netplay_FindPeerByAddr(fromAddr);

		if (existingSlot >= 0)
			acceptData[0] = s_peers[existingSlot].id;
		else
			acceptData[0] = newPlayerId;
		acceptData[1] = (u8)s_playerCount;

		Netplay_SendPacket(NETPLAY_PACKET_HELLO, sizeof(acceptData), acceptData, &clientAddr);
	}

	// Broadcast updated player list to everyone
	{
		u8 playerList[NETPLAY_MAX_PLAYERS];
		int i;

		memset(playerList, 0, sizeof(playerList));
		playerList[0] = 1; // host is always player 0
		for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
		{
			if (s_peers[i].active && s_peers[i].id < NETPLAY_MAX_PLAYERS)
				playerList[s_peers[i].id] = 1;
		}

		Netplay_BroadcastPacket(NETPLAY_PACKET_HELLO, sizeof(playerList), playerList);
		// Also send to the host's local state
		s_playerCount = 0;
		for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
		{
			if (playerList[i])
				s_playerCount++;
		}
	}
}

internal void Netplay_HandleInput(const struct NetplayPacketHeader *header,
                                  const u8 *payload, int payloadSize)
{
	const struct NetplayInputPayload *input = (const struct NetplayInputPayload *)payload;
	int peerIndex;
	u8 senderId;

	(void)payloadSize;

	if (payloadSize < (int)sizeof(struct NetplayInputPayload))
		return;

	senderId = header->playerId;
	peerIndex = Netplay_FindPeerById(senderId);
	if (peerIndex >= 0)
		s_peers[peerIndex].lastFrameReceived = input->frameNum;

	// Queue the input for the game to consume
	Netplay_QueueInput(senderId, input->frameNum, input->buttonsHeld,
	                   input->buttonsTapped, input->buttonsReleased,
	                   input->stickLX, input->stickLY,
	                   input->stickRX, input->stickRY);

	// If we're the host, relay to all other peers (but not back to sender)
	if (Netplay_IsHost())
	{
		int i;

		for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
		{
			if (s_peers[i].active && s_peers[i].id != senderId)
			{
				// Build packet to forward — could include multiple inputs for efficiency
				Netplay_SendPacket(NETPLAY_PACKET_INPUT, (u16)sizeof(*input),
				                   input, &s_peers[i].addr);
			}
		}
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
		// Echo back timestamp
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
	}
}

internal void Netplay_HandleDisconnect(const struct NetplayPacketHeader *header)
{
	Netplay_RemovePeer(header->playerId);
}

internal int Netplay_ReceivePacket(void)
{
	u8 buffer[NETPLAY_MAX_PACKET_SIZE];
	struct sockaddr_in fromAddr;
	socklen_t fromLen;
	int received;
	struct NetplayPacketHeader *header;

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

	switch (header->type)
	{
	case NETPLAY_PACKET_HELLO:
	{
		const u8 *payload = (header->payloadSize > 0) ? (buffer + sizeof(*header)) : NULL;
		Netplay_HandleHello(header, &fromAddr, payload, header->payloadSize);
		break;
	}

	case NETPLAY_PACKET_INPUT:
		Netplay_HandleInput(header, buffer + sizeof(*header), header->payloadSize);
		break;

	case NETPLAY_PACKET_PING:
		Netplay_HandlePing(header, buffer + sizeof(*header), header->payloadSize, &fromAddr);
		break;

	case NETPLAY_PACKET_PONG:
		Netplay_HandlePong(header, buffer + sizeof(*header), header->payloadSize);
		break;

	case NETPLAY_PACKET_DISCONNECT:
		Netplay_HandleDisconnect(header);
		break;

	case NETPLAY_PACKET_START_RACE:
		// Client receives race start signal from host
		fprintf(stdout, "[Netplay] Race starting!\n");
		fflush(stdout);
		g_NetplayRaceStarting = 1;
		break;

	case NETPLAY_PACKET_CHARACTER_SELECT:
		if (Netplay_IsHost() && header->payloadSize >= 1)
		{
			g_NetplayClientCharacter = buffer[sizeof(*header)];
			fprintf(stdout, "[Netplay] Client chose character %d\n", g_NetplayClientCharacter);
			fflush(stdout);
		}
		break;

	case NETPLAY_PACKET_TRACK_SELECT:
		if (!Netplay_IsHost() && header->payloadSize >= 2)
		{
			const u8 *p = buffer + sizeof(*header);
			g_NetplayTrackId = p[0];
			g_NetplayNumLaps = p[1];
			g_NetplayRaceStarting = 1;
			fprintf(stdout, "[Netplay] Host chose track %d, %d laps\n", g_NetplayTrackId, g_NetplayNumLaps);
			fflush(stdout);
		}
		break;
	}

	return 1;
}

int Netplay_Init(void)
{
	if (Netplay_IsRunning())
		return 1;

	memset(s_peers, 0, sizeof(s_peers));
	memset(s_inputHistory, 0, sizeof(s_inputHistory));
	memset(s_inputQueue, 0, sizeof(s_inputQueue));
	memset(s_playerNames, 0, sizeof(s_playerNames));
	memset(s_localPlayerName, 0, sizeof(s_localPlayerName));

	s_inputQueueHead = 0;
	s_inputQueueTail = 0;
	s_inputHistoryWrite = 0;
	s_netplayState = NETPLAY_STATE_DISCONNECTED;
	s_localPlayerId = 0;
	s_playerCount = 0;
	s_peerCount = 0;
	s_nextFrameToSend = 0;
	s_onPlayerJoin = NULL;
	s_onPlayerLeave = NULL;
	g_NetplayAutoJoin = 0;
	g_NetplayRaceStarting = 0;
	g_NetplayHostCharacter = 0;
	g_NetplayClientCharacter = 0;
	g_NetplayTrackId = 0;
	g_NetplayNumLaps = 3;

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

	fprintf(stdout, "[Netplay] Initialized\n");
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

	// Host is always player 0
	memset(s_playerNames[0], 0, NETPLAY_PLAYER_NAME_MAX);
	strncpy(s_playerNames[0], s_localPlayerName, NETPLAY_PLAYER_NAME_MAX - 1);

	memset(&s_hostAddr, 0, sizeof(s_hostAddr));
	s_hostAddr.sin_family = AF_INET;
	s_hostAddr.sin_addr.s_addr = INADDR_ANY;
	s_hostAddr.sin_port = htons(port);

	// Resolve local IP for display
	{
		char hostname[256];
		struct addrinfo hints, *res;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		s_addressString[0] = '\0';

		if (gethostname(hostname, sizeof(hostname)) == 0 &&
		    getaddrinfo(hostname, NULL, &hints, &res) == 0)
		{
			struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
			u32 ip = ntohl(sin->sin_addr.s_addr);

			// Skip loopback (127.x.x.x)
			if ((ip & 0xFF000000) != 0x7F000000)
			{
				snprintf(s_addressString, sizeof(s_addressString),
				         "%u.%u.%u.%u:%u",
				         (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
				         (ip >> 8) & 0xFF, ip & 0xFF,
				         port);
			}
			else
			{
				// Fall back to any address
				snprintf(s_addressString, sizeof(s_addressString),
				         "%u.%u.%u.%u:%u",
				         (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
				         (ip >> 8) & 0xFF, ip & 0xFF,
				         port);
			}

			freeaddrinfo(res);
		}
	}

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
	s_localPlayerId = 0; // will be assigned by host
	s_playerCount = 1;
	s_expectedPlayerCount = 0;

	// Send HELLO to host
	{
		struct NetplayHelloPayload hello;

		memset(&hello, 0, sizeof(hello));
                strncpy((char *)hello.playerName, s_localPlayerName, NETPLAY_PLAYER_NAME_MAX - 1);
                strncpy((char *)hello.gameVersion, "CTR-Native", NETPLAY_VERSION_STRING_MAX - 1);

		Netplay_SendPacket(NETPLAY_PACKET_HELLO, sizeof(hello), &hello, &addr);
	}

	fprintf(stdout, "[Netplay] Connecting to %s:%u...\n", address, port);
	fflush(stdout);
	return 1;
}

void Netplay_Disconnect(void)
{
	if (s_netplayState != NETPLAY_STATE_DISCONNECTED)
	{
		// Send DISCONNECT to peers
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

int Netplay_GetState(void)
{
	return s_netplayState;
}

int Netplay_GetLocalPlayerId(void)
{
	return s_localPlayerId;
}

int Netplay_GetPlayerCount(void)
{
	return s_playerCount;
}

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
			info[i].pingMs = s_peers[i].pingMs;
			info[i].lastFrameReceived = s_peers[i].lastFrameReceived;
		}
		else
		{
			info[i].id = (u8)i;
			info[i].connected = 0;
			info[i].pingMs = 0;
			info[i].lastFrameReceived = 0;
		}
	}

	if (count != NULL)
		*count = NETPLAY_MAX_PLAYERS;

	return info;
}

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

	// Also record locally
	Netplay_QueueInput(s_localPlayerId, frameNum, buttonsHeld, buttonsTapped,
	                   buttonsReleased, stickLX, stickLY, stickRX, stickRY);

	// Store in history
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
	{
		// Host: broadcast to all peers
		Netplay_BroadcastPacket(NETPLAY_PACKET_INPUT, (u16)sizeof(input), &input);
	}
	else
	{
		// Client: send to host
		Netplay_SendPacket(NETPLAY_PACKET_INPUT, (u16)sizeof(input), &input, &s_hostAddr);
	}
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

void Netplay_SetJoinCallback(NetplayEventFn cb)
{
	s_onPlayerJoin = cb;
}

void Netplay_SetLeaveCallback(NetplayEventFn cb)
{
	s_onPlayerLeave = cb;
}

void Netplay_SetPlayerName(const char *name)
{
	memset(s_localPlayerName, 0, NETPLAY_PLAYER_NAME_MAX);
	if (name != NULL)
		strncpy(s_localPlayerName, name, NETPLAY_PLAYER_NAME_MAX - 1);
}

void Netplay_Poll(void)
{
	// Process all pending packets
	while (Netplay_ReceivePacket())
	{
	}

	// If connecting, check if we got a HELLO back and became connected
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
					// We got our player ID from the host
					s_localPlayerId = s_peers[i].id;
				}
			}
		}

		// If we have at least one peer, consider ourselves connected
		if (hasHost && s_localPlayerId != 0)
		{
			s_netplayState = NETPLAY_STATE_CONNECTED;
			fprintf(stdout, "[Netplay] Connected as player %d\n", s_localPlayerId);
			fflush(stdout);
		}
	}

	// Periodically send pings to measure latency (every 60 frames ~1 second)
	// TODO: implement periodic ping
}
