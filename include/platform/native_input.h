#ifndef PLATFORM_NATIVE_INPUT_H
#define PLATFORM_NATIVE_INPUT_H

#include <macros.h>

#define PLATFORM_INPUT_PAD_COUNT 4

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

// Key binding API
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

int  Platform_InputGetKeyBinding(int actionIndex, int *scancode);
int  Platform_InputSetKeyBinding(int actionIndex, int scancode);
const char *Platform_InputGetActionName(int actionIndex);
void Platform_InputResetKeyboardMappings(void);

// Raw keyboard state for keybinding UI
int Platform_InputIsKeyDown(int scancode);
int Platform_InputGetScancodeCount(void);
const char *Platform_InputGetScancodeName(int scancode);

#endif
