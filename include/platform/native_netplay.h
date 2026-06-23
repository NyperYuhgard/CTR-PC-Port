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
	NETPLAY_PACKET_PAUSE           = 0x09,
	NETPLAY_PACKET_UNPAUSE         = 0x0A,
	NETPLAY_PACKET_LOADED          = 0x0B,
	NETPLAY_PACKET_STATE           = 0x0C,
	NETPLAY_PACKET_CRATE_HIT       = 0x0D,
	NETPLAY_PACKET_FINISHED        = 0x0E,
	NETPLAY_PACKET_CHECKSUM        = 0x0F,
	NETPLAY_PACKET_STATE_REQ       = 0x10,
};

struct NetplayChecksumPayload
{
	u32 frameNum;
	u32 checksum;
};

#define NETPLAY_CRATE_QUEUE_MAX 16

struct NetplayCrateHit
{
	s16 posX, posY, posZ;
	u8  modelID;
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
int  Netplay_IsRemoteFinished(u8 playerId);

const char *Netplay_GetAddressString(void);

extern int g_NetplayAutoJoin;
extern int g_NetplayRaceStarting;
extern int g_NetplayRacing;
extern int g_NetplayHostCharacter;
extern int g_NetplayClientCharacter;
extern int g_NetplayTrackId;
extern int g_NetplayNumLaps;
extern int g_NetplayLocalLoaded;
extern int g_NetplayRemoteLoaded;
extern int g_NetplayDisconnected;
extern int g_NetplayStateRequested;
extern u32 g_NetplayRemoteChecksumFrame;
extern u32 g_NetplayRemoteChecksumValue;

#endif
