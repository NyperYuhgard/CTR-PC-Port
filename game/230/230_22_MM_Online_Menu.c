#include <common.h>
#include <platform/native_netplay.h>

#define ONLINE_MENU_CENTER_X   0x100
#define ONLINE_MENU_TITLE_Y    0x20
#define ONLINE_MENU_BODY_Y     0x40
#define ONLINE_MENU_ROW_HEIGHT 0x10
#define ONLINE_MENU_HELP_Y     0xB4

enum OnlinePhase
{
	PHASE_LOBBY,
	PHASE_PICKING_CHARACTER,
	PHASE_WAITING_FOR_OTHER_CHAR,
	PHASE_HOST_PICKING_TRACK,
	PHASE_CLIENT_WAITING_FOR_TRACK,
	PHASE_COUNT
};

static int s_onlinePhase;
static int s_onlineCursor;
static int s_onlineScroll;

static const char *g_charNames[16];
static int g_charNamesInited;

static void Online_InitCharNames(void)
{
	if (g_charNamesInited)
		return;
	g_charNamesInited = 1;
	g_charNames[0]  = "Crash";
	g_charNames[1]  = "Cortex";
	g_charNames[2]  = "Tiny";
	g_charNames[3]  = "Coco";
	g_charNames[4]  = "N. Gin";
	g_charNames[5]  = "Dingodile";
	g_charNames[6]  = "Polar";
	g_charNames[7]  = "Pura";
	g_charNames[8]  = "Pinstripe";
	g_charNames[9]  = "Papu Papu";
	g_charNames[10] = "Ripper Roo";
	g_charNames[11] = "Komodo Joe";
	g_charNames[12] = "N. Tropy";
	g_charNames[13] = "Penta";
	g_charNames[14] = "Fake Crash";
	g_charNames[15] = "Oxide";
}

static const char *OnlineMenu_StateText(void)
{
	switch (Netplay_GetState())
	{
	case NETPLAY_STATE_DISCONNECTED: return "Disconnected";
	case NETPLAY_STATE_HOSTING:      return "Hosting";
	case NETPLAY_STATE_CONNECTING:   return "Connecting...";
	case NETPLAY_STATE_CONNECTED:    return "Connected";
	default:                         return "Unknown";
	}
}

static void Online_StartRace(struct GameTracker *gGT, int playerCount, int hostChar, int clientChar, int trackId, int numLaps)
{
	g_NetplayRacing = 1;

	// ---- Set up race config ----
	data.characterIDs[0] = (s16)hostChar;
	data.characterIDs[1] = (s16)clientChar;

	gGT->numPlyrCurrGame = playerCount;
	gGT->numLaps = numLaps;
	gGT->currLEV = (s16)trackId;
	gGT->gameMode1 &= ~(ADVENTURE_MODE | TIME_TRIAL | BATTLE_MODE);
	gGT->gameMode1 |= ARCADE_MODE;

	// necessary for character select backup
	MM_Characters_BackupIDs();

	// Load track
	sdata->ptrDesiredMenu = &data.menuQueueLoadTrack;
}

void MM_Online_Init(void)
{
	g_NetplayRacing = 0;
	s_onlinePhase = PHASE_LOBBY;
	s_onlineCursor = 0;
	s_onlineScroll = 0;
	g_charNamesInited = 0;
	g_NetplayHostCharacter = 0;
	g_NetplayClientCharacter = 0;
	g_NetplayTrackId = 0;
	g_NetplayNumLaps = 3;
}

void MM_Online_MenuProc(struct RectMenu *menu)
{
	struct GameTracker *gGT = sdata->gGT;
	u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
	int state = Netplay_GetState();
	int playerCount = Netplay_GetPlayerCount();
	int isHost = (state == NETPLAY_STATE_HOSTING);
	int localId = Netplay_GetLocalPlayerId();
	char buf[128];
	RECT border;

	Online_InitCharNames();

	if (menu->rowSelected == -1)
	{
		if (menu->ptrPrevBox_InHierarchy != NULL)
			menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);

		Netplay_Disconnect();
		OtherFX_Play(2, 1);
		MM_JumpTo_Title_Returning();
		return;
	}

	// ---- Handle START_RACE from host (old path, for backward compat) ----
	if (g_NetplayRaceStarting && s_onlinePhase != PHASE_CLIENT_WAITING_FOR_TRACK)
	{
		g_NetplayRaceStarting = 0;
		s_onlinePhase = PHASE_PICKING_CHARACTER;
		RECTMENU_ClearInput();
	}

	// ===================================================================
	// DRAW
	// ===================================================================

	// ---- Title ----
	DecalFont_DrawLineOT(
		"ONLINE",
		ONLINE_MENU_CENTER_X, ONLINE_MENU_TITLE_Y,
		FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

	switch (s_onlinePhase)
	{
	int y;
	int numItems;
	int i;

	// ==================== LOBBY ====================
	case PHASE_LOBBY:
	{
		int y = ONLINE_MENU_BODY_Y;

		snprintf(buf, sizeof(buf), "Status: %s", OnlineMenu_StateText());
		DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y, FONT_BIG, JUSTIFY_CENTER | WHITE, ot);
		y += 0x14;

		if (isHost)
		{
			snprintf(buf, sizeof(buf), "IP: %s", Netplay_GetAddressString());
			DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y, FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
			y += 0x10;
		}

		if (state == NETPLAY_STATE_HOSTING || state == NETPLAY_STATE_CONNECTED)
		{
			snprintf(buf, sizeof(buf), "Players: %d", playerCount);
			DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y, FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
			y += 0x10;

			for (i = 0; i < playerCount; i++)
			{
				int color = (i == localId) ? TINY_GREEN : WHITE;
				snprintf(buf, sizeof(buf), "Player %d", i);
				DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y, FONT_SMALL, JUSTIFY_CENTER | color, ot);
				y += ONLINE_MENU_ROW_HEIGHT;
			}
		}

		if (isHost && playerCount >= 2)
		{
			DecalFont_DrawLineOT("START: Start Race", ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
			                     FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
		}
		else
		{
			DecalFont_DrawLineOT("TRI: Back", ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
			                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
		}
		break;
	}

	// ==================== PICKING CHARACTER ====================
	case PHASE_PICKING_CHARACTER:
	{
		y = ONLINE_MENU_BODY_Y;

		DecalFont_DrawLineOT("Choose your character:", ONLINE_MENU_CENTER_X, y,
		                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
		y += 0x14;

		numItems = 16;

		// Clamp cursor
		if (s_onlineCursor < 0) s_onlineCursor = 0;
		if (s_onlineCursor >= numItems) s_onlineCursor = numItems - 1;

		// Scroll logic
		int visible = 8;
		if (s_onlineCursor < s_onlineScroll)
			s_onlineScroll = s_onlineCursor;
		if (s_onlineCursor >= s_onlineScroll + visible)
			s_onlineScroll = s_onlineCursor - visible + 1;
		if (s_onlineScroll < 0) s_onlineScroll = 0;
		if (s_onlineScroll > numItems - visible) s_onlineScroll = numItems - visible;

		for (i = s_onlineScroll; i < s_onlineScroll + visible && i < numItems; i++)
		{
			int isSelected = (i == s_onlineCursor);

			if (isSelected)
			{
				RECT hl;
				hl.x = 0x80;
				hl.y = y - 1;
				hl.w = 0x100;
				hl.h = ONLINE_MENU_ROW_HEIGHT + 1;
				CTR_Box_DrawClearBox(&hl, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
			}

			DecalFont_DrawLineOT((char *)g_charNames[i], ONLINE_MENU_CENTER_X, y,
			                     FONT_SMALL, isSelected ? WHITE : ORANGE, ot);
			y += ONLINE_MENU_ROW_HEIGHT;
		}

		DecalFont_DrawLineOT("UP/DN: Select   X: Confirm   TRI: Back",
		                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
		                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
		break;
	}

	// ==================== WAITING FOR OTHER CHAR ====================
	case PHASE_WAITING_FOR_OTHER_CHAR:
	{
		y = ONLINE_MENU_BODY_Y + 0x14;

		DecalFont_DrawLineOT("Waiting for other player", ONLINE_MENU_CENTER_X, y,
		                     FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);
		y += 0x14;

		snprintf(buf, sizeof(buf), "Your pick: %s", g_charNames[isHost ? g_NetplayHostCharacter : g_NetplayClientCharacter]);
		DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
		                     FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);

		DecalFont_DrawLineOT("TRI: Back", ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
		                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
		break;
	}

	// ==================== HOST PICKING TRACK ====================
	case PHASE_HOST_PICKING_TRACK:
	{
		y = ONLINE_MENU_BODY_Y;

		DecalFont_DrawLineOT("Choose a track:", ONLINE_MENU_CENTER_X, y,
		                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
		y += 0x14;

		numItems = 18;

		if (s_onlineCursor < 0) s_onlineCursor = 0;
		if (s_onlineCursor >= numItems) s_onlineCursor = numItems - 1;

		int visible = 8;
		if (s_onlineCursor < s_onlineScroll)
			s_onlineScroll = s_onlineCursor;
		if (s_onlineCursor >= s_onlineScroll + visible)
			s_onlineScroll = s_onlineCursor - visible + 1;
		if (s_onlineScroll < 0) s_onlineScroll = 0;
		if (s_onlineScroll > numItems - visible) s_onlineScroll = numItems - visible;

		for (i = s_onlineScroll; i < s_onlineScroll + visible && i < numItems; i++)
		{
			s16 levID = D230.arcadeTracks[i].levID;
			const char *name = sdata->lngStrings[data.metaDataLEV[levID].name_LNG];
			int isSelected = (i == s_onlineCursor);

			if (isSelected)
			{
				RECT hl;
				hl.x = 0x80;
				hl.y = y - 1;
				hl.w = 0x100;
				hl.h = ONLINE_MENU_ROW_HEIGHT + 1;
				CTR_Box_DrawClearBox(&hl, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
			}

			DecalFont_DrawLineOT((char *)name, ONLINE_MENU_CENTER_X, y,
			                     FONT_SMALL, isSelected ? WHITE : ORANGE, ot);
			y += ONLINE_MENU_ROW_HEIGHT;
		}

		snprintf(buf, sizeof(buf), "Laps: %d", g_NetplayNumLaps);
		DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
		                     FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);

		DecalFont_DrawLineOT("UP/DN: Select   X: Confirm   TRI: Back",
		                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
		                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
		break;
	}

	// ==================== CLIENT WAITING FOR TRACK ====================
	case PHASE_CLIENT_WAITING_FOR_TRACK:
	{
		y = ONLINE_MENU_BODY_Y + 0x14;

		DecalFont_DrawLineOT("El host esta eligiendo la pista...", ONLINE_MENU_CENTER_X, y,
		                     FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);
		y += 0x14;

		snprintf(buf, sizeof(buf), "Your character: %s", g_charNames[g_NetplayClientCharacter]);
		DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
		                     FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);

		// Check if host sent track select
		if (g_NetplayRaceStarting)
		{
			g_NetplayRaceStarting = 0;
			OtherFX_Play(1, 1);
			RECTMENU_ClearInput();
			Online_StartRace(gGT, playerCount,
			                 g_NetplayHostCharacter,
			                 g_NetplayClientCharacter,
			                 g_NetplayTrackId,
			                 g_NetplayNumLaps);
			return;
		}

		DecalFont_DrawLineOT("TRI: Back", ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
		                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
		break;
	}
	}

	// ---- Panel border ----
	border.x = 0x60;
	border.y = ONLINE_MENU_TITLE_Y - 8;
	border.w = 0x1C0;
	border.h = ONLINE_MENU_HELP_Y - ONLINE_MENU_TITLE_Y + 0x18;
	RECTMENU_DrawInnerRect(&border, 0, ot);

	// ===================================================================
	// INPUT HANDLING
	// ===================================================================

	switch (s_onlinePhase)
	{
	case PHASE_LOBBY:
		if (sdata->buttonTapPerPlayer[0] & BTN_START)
		{
			if (isHost && playerCount >= 2)
			{
				OtherFX_Play(1, 1);
				RECTMENU_ClearInput();
				s_onlinePhase = PHASE_PICKING_CHARACTER;
				s_onlineCursor = 0;
				s_onlineScroll = 0;
				Netplay_BroadcastPacket(NETPLAY_PACKET_START_RACE, 0, NULL);
			}
			else
			{
				RECTMENU_ClearInput();
			}
		}
		else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
		{
			OtherFX_Play(2, 1);
			RECTMENU_ClearInput();
			Netplay_Disconnect();
			MM_JumpTo_Title_Returning();
		}
		break;

	case PHASE_PICKING_CHARACTER:
		if (sdata->buttonTapPerPlayer[0] & BTN_UP)
		{
			s_onlineCursor = (s_onlineCursor - 1 + 16) % 16;
			OtherFX_Play(0, 1);
			RECTMENU_ClearInput();
		}
		else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
		{
			s_onlineCursor = (s_onlineCursor + 1) % 16;
			OtherFX_Play(0, 1);
			RECTMENU_ClearInput();
		}
		else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
		{
			int chosenChar = s_onlineCursor;

			OtherFX_Play(1, 1);
			RECTMENU_ClearInput();

			if (isHost)
			{
				g_NetplayHostCharacter = chosenChar;
				// If client already picked, go to track select
				if (g_NetplayClientCharacter >= 0)
				{
					s_onlinePhase = PHASE_HOST_PICKING_TRACK;
					s_onlineCursor = 0;
					s_onlineScroll = 0;
				}
				else
				{
					s_onlinePhase = PHASE_WAITING_FOR_OTHER_CHAR;
				}
			}
			else
			{
				g_NetplayClientCharacter = chosenChar;
				// Send character choice to host
				{
					u8 payload = (u8)chosenChar;
					Netplay_BroadcastPacket(NETPLAY_PACKET_CHARACTER_SELECT, sizeof(payload), &payload);
				}
				s_onlinePhase = PHASE_CLIENT_WAITING_FOR_TRACK;
			}
		}
		else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
		{
			OtherFX_Play(2, 1);
			RECTMENU_ClearInput();
			s_onlinePhase = PHASE_LOBBY;
			s_onlineCursor = 0;
			s_onlineScroll = 0;
		}
		break;

	case PHASE_WAITING_FOR_OTHER_CHAR:
		// Host: check if client sent their character
		if (isHost && g_NetplayClientCharacter >= 0)
		{
			s_onlinePhase = PHASE_HOST_PICKING_TRACK;
			s_onlineCursor = 0;
			s_onlineScroll = 0;
		}
		// TRI to go back
		if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
		{
			OtherFX_Play(2, 1);
			RECTMENU_ClearInput();
			s_onlinePhase = PHASE_LOBBY;
			s_onlineCursor = 0;
			g_NetplayClientCharacter = 0;
		}
		break;

	case PHASE_HOST_PICKING_TRACK:
		if (sdata->buttonTapPerPlayer[0] & BTN_UP)
		{
			s_onlineCursor = (s_onlineCursor - 1 + 18) % 18;
			OtherFX_Play(0, 1);
			RECTMENU_ClearInput();
		}
		else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
		{
			s_onlineCursor = (s_onlineCursor + 1) % 18;
			OtherFX_Play(0, 1);
			RECTMENU_ClearInput();
		}
		else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
		{
			s16 levID = D230.arcadeTracks[s_onlineCursor].levID;

			OtherFX_Play(1, 1);
			RECTMENU_ClearInput();

			g_NetplayTrackId = (int)levID;

			// Broadcast track to client
			{
				u8 payload[2];
				payload[0] = (u8)levID;
				payload[1] = (u8)g_NetplayNumLaps;
				Netplay_BroadcastPacket(NETPLAY_PACKET_TRACK_SELECT, sizeof(payload), payload);
			}

			Online_StartRace(gGT, playerCount,
			                 g_NetplayHostCharacter,
			                 g_NetplayClientCharacter,
			                 g_NetplayTrackId,
			                 g_NetplayNumLaps);
			return;
		}
		else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
		{
			OtherFX_Play(2, 1);
			RECTMENU_ClearInput();
			s_onlinePhase = PHASE_PICKING_CHARACTER;
			s_onlineCursor = g_NetplayHostCharacter;
		}
		break;

	case PHASE_CLIENT_WAITING_FOR_TRACK:
		if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
		{
			OtherFX_Play(2, 1);
			RECTMENU_ClearInput();
			s_onlinePhase = PHASE_PICKING_CHARACTER;
			s_onlineCursor = g_NetplayClientCharacter;
		}
		break;
	}
}
