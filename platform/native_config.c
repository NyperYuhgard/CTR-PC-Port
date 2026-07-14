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
		else if (strcmp(section, "keyboard") == 0)
		{
			int actionIndex = -1;
			if (strcmp(key, "square") == 0) actionIndex = 0;
			else if (strcmp(key, "circle") == 0) actionIndex = 1;
			else if (strcmp(key, "triangle") == 0) actionIndex = 2;
			else if (strcmp(key, "cross") == 0) actionIndex = 3;
			else if (strcmp(key, "l1") == 0) actionIndex = 4;
			else if (strcmp(key, "l2") == 0) actionIndex = 5;
			else if (strcmp(key, "l3") == 0) actionIndex = 6;
			else if (strcmp(key, "r1") == 0) actionIndex = 7;
			else if (strcmp(key, "r2") == 0) actionIndex = 8;
			else if (strcmp(key, "r3") == 0) actionIndex = 9;
			else if (strcmp(key, "start") == 0) actionIndex = 10;
			else if (strcmp(key, "select") == 0) actionIndex = 11;
			else if (strcmp(key, "dpad_up") == 0) actionIndex = 12;
			else if (strcmp(key, "dpad_down") == 0) actionIndex = 13;
			else if (strcmp(key, "dpad_left") == 0) actionIndex = 14;
			else if (strcmp(key, "dpad_right") == 0) actionIndex = 15;

			if (actionIndex >= 0)
				Platform_InputSetKeyBinding(actionIndex, intVal);
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
	int i;

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

	NativeConfig_WriteLine(f, "keyboard");
	for (i = 0; i < 16; i++)
	{
		const char *keyName = Platform_InputGetActionName(i);
		if (keyName == NULL)
			continue;

		Platform_InputGetKeyBinding(i, &bindings[i]);
		fprintf(f, "%s=%d\n", keyName, bindings[i]);
	}

	fclose(f);
}
