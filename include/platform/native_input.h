#ifndef PLATFORM_NATIVE_INPUT_H
#define PLATFORM_NATIVE_INPUT_H

#include <macros.h>

#define PLATFORM_INPUT_PAD_COUNT 4
#define PLATFORM_INPUT_PLAYER_COUNT 4

struct PlatformInputPadSnapshot
{
	u8 status;
	u8 id;
	u8 buttons[2];
	u8 analog[4];
	u8 connected;
	u8 reserved[3];
};

int Platform_InputInit(void);
void Platform_InputShutdown(void);
void Platform_InputUpdate(void);
void Platform_InputControllerAdded(int deviceIndex);
void Platform_InputControllerRemoved(int instanceId);
int Platform_InputCycleKeyboardController(void);

void Platform_InputPadInit(int slot, unsigned char *padData);
int Platform_InputPadGetState(int port);
void Platform_InputPadVibrate(int port, unsigned char *table, int len);
int Platform_InputCapturePadSnapshots(struct PlatformInputPadSnapshot *dst, int count);
int Platform_InputInstallPadSnapshots(const struct PlatformInputPadSnapshot *src, int count);
void Platform_InputClearInstalledPadSnapshots(void);
int Platform_InputGetStateSize(void);
int Platform_InputCaptureState(void *dst, int dstSize);
int Platform_InputRestoreState(const void *src, int srcSize);

// Binding constants (shared between keyboard and gamepad)
#define PLATFORM_INPUT_BINDING_COUNT 16

#define PLATFORM_INPUT_BINDING_SQUARE  0
#define PLATFORM_INPUT_BINDING_CIRCLE  1
#define PLATFORM_INPUT_BINDING_TRIANGLE 2
#define PLATFORM_INPUT_BINDING_CROSS   3
#define PLATFORM_INPUT_BINDING_L1      4
#define PLATFORM_INPUT_BINDING_L2      5
#define PLATFORM_INPUT_BINDING_L3      6
#define PLATFORM_INPUT_BINDING_R1      7
#define PLATFORM_INPUT_BINDING_R2      8
#define PLATFORM_INPUT_BINDING_R3      9
#define PLATFORM_INPUT_BINDING_START   10
#define PLATFORM_INPUT_BINDING_SELECT  11
#define PLATFORM_INPUT_BINDING_DPAD_UP    12
#define PLATFORM_INPUT_BINDING_DPAD_DOWN  13
#define PLATFORM_INPUT_BINDING_DPAD_LEFT  14
#define PLATFORM_INPUT_BINDING_DPAD_RIGHT 15

// Keyboard binding API (per-player)
int  Platform_InputGetKeyBinding(int playerIndex, int actionIndex, int *scancode);
int  Platform_InputSetKeyBinding(int playerIndex, int actionIndex, int scancode);
void Platform_InputResetKeyboardMappings(int playerIndex);
const char *Platform_InputGetActionName(int actionIndex);

// Gamepad binding API (per-player)
// Returns the SDL button/axis value for an action (with NATIVE_INPUT_MAP_FLAG_AXIS etc)
int  Platform_InputGetGamepadBinding(int playerIndex, int actionIndex, int *binding);
int  Platform_InputSetGamepadBinding(int playerIndex, int actionIndex, int binding);
void Platform_InputResetGamepadMappings(int playerIndex);
const char *Platform_InputGetGamepadActionName(int actionIndex);

// Raw keyboard state for keybinding UI
int Platform_InputIsKeyDown(int scancode);
int Platform_InputGetScancodeCount(void);
const char *Platform_InputGetScancodeName(int scancode);

// Gamepad state for gamepad binding UI
int Platform_InputIsGamepadButtonDown(int playerIndex, int sdlButton);
int Platform_InputIsGamepadAxisActive(int playerIndex, int sdlAxis, int threshold);
int Platform_InputGetGamepadCount(void);
const char *Platform_InputGetGamepadName(int slot);

// Device assignment API
// keyboardSlot: which player slot the keyboard feeds (0-3), or -1 for none
void Platform_InputSetKeyboardSlot(int keyboardSlot);
int  Platform_InputGetKeyboardSlot(void);
// instanceId: SDL_JoystickID from SDL_GetGamepads(), playerSlot: which player (0-3), or -1 to disconnect
void Platform_InputSetGamepadToPlayer(int instanceId, int playerSlot);
int  Platform_InputGetGamepadPlayer(int instanceId);
// Disconnect all devices from a player slot
void Platform_InputClearPlayerSlot(int playerSlot);

// Max active players for multitap gating (call when numPlyrNextGame changes)
void Platform_InputSetMaxPlayers(int maxPlayers);

// Refresh gamepad-to-slot links (call every frame)
void Platform_InputRefreshGamepadLinks(void);

// Utility: convert device list index (0..N-1) to SDL_JoystickID, or -1 if invalid
int  Platform_InputGetGamepadDeviceId(int deviceIndex);
// Count of connected SDL gamepads
int  Platform_InputGetGamepadDeviceCount(void);
// Get name of a gamepad by SDL_JoystickID
const char *Platform_InputGetGamepadDeviceName(int instanceId);

#endif
