#ifndef PLATFORM_NATIVE_NETPLAY_H
#define PLATFORM_NATIVE_NETPLAY_H

#include <macros.h>

#define NETPLAY_MAX_PLAYERS      8
#define NETPLAY_DEFAULT_PORT     14200
#define NETPLAY_FRAME_HISTORY    120

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
};

enum NetplayState
{
	NETPLAY_STATE_DISCONNECTED,
	NETPLAY_STATE_HOSTING,
	NETPLAY_STATE_CONNECTING,
	NETPLAY_STATE_CONNECTED,
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
	u16 pingMs;
	u32 lastFrameReceived;
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
const struct NetplayPlayerInfo *Netplay_GetPlayers(int *count);

void Netplay_SendGamepadState(u32 frameNum, u32 buttonsHeld, u32 buttonsTapped,
                              u32 buttonsReleased, s16 stickLX, s16 stickLY,
                              s16 stickRX, s16 stickRY);
int  Netplay_ReceiveInputs(struct NetplayInput *inputs, int maxInputs);

void Netplay_SetJoinCallback(NetplayEventFn cb);
void Netplay_SetLeaveCallback(NetplayEventFn cb);
void Netplay_SetPlayerName(const char *name);

void Netplay_Poll(void);

void Netplay_BroadcastPacket(u16 type, u16 payloadSize, const void *payload);

const char *Netplay_GetAddressString(void);

extern int g_NetplayAutoJoin;
extern int g_NetplayRaceStarting;
extern int g_NetplayRacing;
extern int g_NetplayHostCharacter;
extern int g_NetplayClientCharacter;
extern int g_NetplayTrackId;
extern int g_NetplayNumLaps;

#endif
