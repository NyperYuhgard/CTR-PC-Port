#include <platform/native_config.h>
#include <platform/native_input.h>
#include <platform/native_assets.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NATIVE_CONFIG_FILE "ctr_native.cfg"
#define NATIVE_CONFIG_LINE_MAX 256

extern int g_cfg_bilinearFiltering;
extern int g_cfg_60fpsMode;
extern int g_cfg_aspectMode;
extern int g_cfg_fullscreen;
extern int g_cfg_resolutionScale;

// Gameplay Tweaks
extern int g_cfg_specialItems;
extern int g_cfg_cpuAllItems;
extern int g_cfg_itemChaos;
extern int g_cfg_cpuItemChaos;
extern int g_cfg_chaosRng;

static void NativeConfig_BuildPath(char *buf, size_t size)
{
	const char *base = NativeAssets_GetBaseDir();
	if (base == NULL)
		base = ".";
	snprintf(buf, size, "%s/%s", base, NATIVE_CONFIG_FILE);
}

static void NativeConfig_Trim(char *str)
{
	char *end;
	while (*str == ' ' || *str == '\t')
		str++;
	if (*str == '\0')
		return;
	end = str + strlen(str) - 1;
	while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
		end--;
	*(end + 1) = '\0';
}

static int NativeConfig_ParseActionKey(const char *key)
{
	if (strcmp(key, "square") == 0) return PLATFORM_INPUT_BINDING_SQUARE;
	if (strcmp(key, "circle") == 0) return PLATFORM_INPUT_BINDING_CIRCLE;
	if (strcmp(key, "triangle") == 0) return PLATFORM_INPUT_BINDING_TRIANGLE;
	if (strcmp(key, "cross") == 0) return PLATFORM_INPUT_BINDING_CROSS;
	if (strcmp(key, "l1") == 0) return PLATFORM_INPUT_BINDING_L1;
	if (strcmp(key, "l2") == 0) return PLATFORM_INPUT_BINDING_L2;
	if (strcmp(key, "l3") == 0) return PLATFORM_INPUT_BINDING_L3;
	if (strcmp(key, "r1") == 0) return PLATFORM_INPUT_BINDING_R1;
	if (strcmp(key, "r2") == 0) return PLATFORM_INPUT_BINDING_R2;
	if (strcmp(key, "r3") == 0) return PLATFORM_INPUT_BINDING_R3;
	if (strcmp(key, "start") == 0) return PLATFORM_INPUT_BINDING_START;
	if (strcmp(key, "select") == 0) return PLATFORM_INPUT_BINDING_SELECT;
	if (strcmp(key, "dpad_up") == 0) return PLATFORM_INPUT_BINDING_DPAD_UP;
	if (strcmp(key, "dpad_down") == 0) return PLATFORM_INPUT_BINDING_DPAD_DOWN;
	if (strcmp(key, "dpad_left") == 0) return PLATFORM_INPUT_BINDING_DPAD_LEFT;
	if (strcmp(key, "dpad_right") == 0) return PLATFORM_INPUT_BINDING_DPAD_RIGHT;
	return -1;
}

void NativeConfig_Load(void)
{
	char path[512];
	FILE *f;
	char line[NATIVE_CONFIG_LINE_MAX];
	char section[64] = "";
	int lineNum = 0;

	NativeConfig_BuildPath(path, sizeof(path));
	f = fopen(path, "r");
	if (f == NULL)
		return;

	while (fgets(line, sizeof(line), f) != NULL)
	{
		char *p;
		char key[64];
		char value[64];
		int intVal;

		lineNum++;
		NativeConfig_Trim(line);
		if (line[0] == '\0' || line[0] == '#')
			continue;

		if (line[0] == '[')
		{
			p = strchr(line + 1, ']');
			if (p != NULL)
			{
				*p = '\0';
				strncpy(section, line + 1, sizeof(section) - 1);
				section[sizeof(section) - 1] = '\0';
			}
			continue;
		}

		p = strchr(line, '=');
		if (p == NULL)
			continue;

		*p = '\0';
		strncpy(key, line, sizeof(key) - 1);
		key[sizeof(key) - 1] = '\0';
		NativeConfig_Trim(key);

		strncpy(value, p + 1, sizeof(value) - 1);
		value[sizeof(value) - 1] = '\0';
		NativeConfig_Trim(value);

		intVal = atoi(value);

		if (strcmp(section, "display") == 0)
		{
			if (strcmp(key, "aspect_mode") == 0 && intVal >= 0 && intVal <= 3)
				g_cfg_aspectMode = intVal;
			else if (strcmp(key, "fps_60") == 0)
				g_cfg_60fpsMode = intVal != 0 ? 1 : 0;
			else if (strcmp(key, "bilinear_filter") == 0)
				g_cfg_bilinearFiltering = intVal != 0 ? 1 : 0;
			else if (strcmp(key, "fullscreen") == 0)
				g_cfg_fullscreen = intVal != 0 ? 1 : 0;
			else if (strcmp(key, "resolution_scale") == 0 && intVal >= 1 && intVal <= 4)
				g_cfg_resolutionScale = intVal;
		}
		else if (strcmp(section, "gameplay") == 0)
		{
			if (strcmp(key, "special_items") == 0)
				g_cfg_specialItems = intVal != 0 ? 1 : 0;
			else if (strcmp(key, "cpu_all_items") == 0)
				g_cfg_cpuAllItems = intVal != 0 ? 1 : 0;
			else if (strcmp(key, "item_chaos") == 0)
				g_cfg_itemChaos = intVal != 0 ? 1 : 0;
			else if (strcmp(key, "cpu_item_chaos") == 0)
				g_cfg_cpuItemChaos = intVal != 0 ? 1 : 0;
			else if (strcmp(key, "chaos_rng") == 0)
				g_cfg_chaosRng = intVal != 0 ? 1 : 0;
		}
		else if (strcmp(section, "controls") == 0)
		{
			if (strcmp(key, "keyboard_slot") == 0)
				Platform_InputSetKeyboardSlot(intVal);
			else if (strncmp(key, "gamepad", 7) == 0 && key[7] >= '0' && key[7] <= '3' && strcmp(key + 8, "_slot") == 0)
			{
				int devIdx = key[7] - '0';
				int joyId = Platform_InputGetGamepadDeviceId(devIdx);
				if (joyId >= 0)
					Platform_InputSetGamepadToPlayer(joyId, intVal);
			}
		}
		else
		{
			// Parse per-player keyboard sections: keyboard, keyboard_p0..keyboard_p3
			// Also legacy "keyboard" section maps to player 0
			int targetPlayer = -1;
			int isKeyboard = 0;
			int isGamepad = 0;

			if (strcmp(section, "keyboard") == 0)
			{
				targetPlayer = 0;
				isKeyboard = 1;
			}
			else
			{
				char *endp;
				if (strncmp(section, "keyboard_p", 10) == 0)
				{
					targetPlayer = strtol(section + 10, &endp, 10);
					if (*endp != '\0' || targetPlayer < 0 || targetPlayer >= PLATFORM_INPUT_PLAYER_COUNT)
						targetPlayer = -1;
					isKeyboard = 1;
				}
				else if (strncmp(section, "gamepad_p", 9) == 0)
				{
					targetPlayer = strtol(section + 9, &endp, 10);
					if (*endp != '\0' || targetPlayer < 0 || targetPlayer >= PLATFORM_INPUT_PLAYER_COUNT)
						targetPlayer = -1;
					isGamepad = 1;
				}
			}

			if (targetPlayer >= 0)
			{
				int actionIndex = NativeConfig_ParseActionKey(key);
				if (actionIndex >= 0)
				{
					if (isKeyboard)
						Platform_InputSetKeyBinding(targetPlayer, actionIndex, intVal);
					else if (isGamepad)
						Platform_InputSetGamepadBinding(targetPlayer, actionIndex, intVal);
				}
			}
		}
	}

	fclose(f);
}

static void NativeConfig_WriteLine(FILE *f, const char *section)
{
	fprintf(f, "\n[%s]\n", section);
}

void NativeConfig_Save(void)
{
	char path[512];
	FILE *f;
	int bindings[16];
	int i, p;

	NativeConfig_BuildPath(path, sizeof(path));
	f = fopen(path, "w");
	if (f == NULL)
	{
		fprintf(stderr, "[CTR Native] Failed to write config: %s\n", path);
		return;
	}

	fprintf(f, "# CTR Native configuration file\n");
	fprintf(f, "# Generated automatically - edit while game is closed\n");

	NativeConfig_WriteLine(f, "display");
	fprintf(f, "aspect_mode=%d\n", g_cfg_aspectMode);
	fprintf(f, "fps_60=%d\n", g_cfg_60fpsMode);
	fprintf(f, "bilinear_filter=%d\n", g_cfg_bilinearFiltering);
	fprintf(f, "fullscreen=%d\n", g_cfg_fullscreen);
	fprintf(f, "resolution_scale=%d\n", g_cfg_resolutionScale);

	NativeConfig_WriteLine(f, "gameplay");
	fprintf(f, "special_items=%d\n", g_cfg_specialItems);
	fprintf(f, "cpu_all_items=%d\n", g_cfg_cpuAllItems);
	fprintf(f, "item_chaos=%d\n", g_cfg_itemChaos);
	fprintf(f, "cpu_item_chaos=%d\n", g_cfg_cpuItemChaos);
	fprintf(f, "chaos_rng=%d\n", g_cfg_chaosRng);

	// Device assignment
	NativeConfig_WriteLine(f, "controls");
	fprintf(f, "keyboard_slot=%d\n", Platform_InputGetKeyboardSlot());
	{
		int gpCount = Platform_InputGetGamepadDeviceCount();
		for (i = 0; i < gpCount && i < 4; i++)
		{
			int joyId = Platform_InputGetGamepadDeviceId(i);
			int slot = (joyId >= 0) ? Platform_InputGetGamepadPlayer(joyId) : -1;
			fprintf(f, "gamepad%d_slot=%d\n", i, slot);
		}
	}

	// Per-player keyboard bindings
	for (p = 0; p < PLATFORM_INPUT_PLAYER_COUNT; p++)
	{
		char sectionName[32];
		snprintf(sectionName, sizeof(sectionName), "keyboard_p%d", p);
		NativeConfig_WriteLine(f, sectionName);
		for (i = 0; i < PLATFORM_INPUT_BINDING_COUNT; i++)
		{
			const char *keyName = Platform_InputGetActionName(i);
			if (keyName == NULL)
				continue;
			Platform_InputGetKeyBinding(p, i, &bindings[i]);
			fprintf(f, "%s=%d\n", keyName, bindings[i]);
		}
	}

	// Per-player gamepad bindings
	for (p = 0; p < PLATFORM_INPUT_PLAYER_COUNT; p++)
	{
		char sectionName[32];
		snprintf(sectionName, sizeof(sectionName), "gamepad_p%d", p);
		NativeConfig_WriteLine(f, sectionName);
		for (i = 0; i < PLATFORM_INPUT_BINDING_COUNT; i++)
		{
			const char *keyName = Platform_InputGetGamepadActionName(i);
			if (keyName == NULL)
				continue;
			Platform_InputGetGamepadBinding(p, i, &bindings[i]);
			fprintf(f, "%s=%d\n", keyName, bindings[i]);
		}
	}

	fclose(f);
}
