#include <common.h>
#include <platform/native_netplay.h>

#define ONLINE_MENU_CENTER_X   0x100
#define ONLINE_MENU_TITLE_Y    0x20
#define ONLINE_MENU_BODY_Y     0x40
#define ONLINE_MENU_ROW_HEIGHT 0x10
#define ONLINE_MENU_HELP_Y     0xB4

enum OnlinePhase
{
        PHASE_PICK_ROLE,             /* Host or Connect? */
        PHASE_PICK_INTERFACE,        /* (host) which network interface to bind */
        PHASE_ENTER_HOST_IP,         /* (client) type the host's IP */
        PHASE_CONNECTING,            /* (client) waiting for HELLO accept */
        PHASE_LOBBY,
        PHASE_PICKING_CHARACTER,
        PHASE_PICKING_ENGINE,
        PHASE_WAITING_FOR_PLAYERS,
        PHASE_HOST_PICKING_TRACK,
        PHASE_CLIENT_WAITING_FOR_TRACK,
        PHASE_COUNT
};

static int s_onlinePhase;
static int s_onlineCursor;
static int s_onlineScroll;
static int s_chatInputActive;
static int s_chatInputLen;
static char s_chatInputBuf[NETPLAY_CHAT_MSG_MAX];
/* Up to 4 most recent chat lines displayed in the lobby */
static char s_chatLines[4][NETPLAY_CHAT_MSG_MAX + 16];
static int s_chatLineCount;

/* In-game host/connect state (replaces --host/--connect CLI args) */
static int s_roleIsHost;             /* 1 = host, 0 = client */
static int s_ifaceListCount;         /* number of interfaces found */
static struct NetplayInterface s_ifaceList[NETPLAY_IFACE_LIST_MAX];
static int s_ifaceSelected;          /* index into s_ifaceList */
static char s_hostIPInput[32];       /* typed host IP for client mode */
static int s_hostIPLen;
static int s_ipInputActive;          /* 1 when typing the host IP */
static u16 s_netplayPort = NETPLAY_DEFAULT_PORT;  /* default 14200, editable */

/* Chat window tracking: we open it when entering the lobby and close it
 * when leaving the online menu entirely. The chat itself is typed in the
 * separate OS window, not ingame. */
static int s_chatWindowOpened;

static const char *g_charNames[16];
static int g_charNamesInited;

static const char *g_engineNames[] = {
    "Balanced",
    "Acceleration",
    "Speed",
    "Turning",
    "Unlimited"
};
#define NUM_ENGINES 5
#define ENGINE_UNLIMITED 4

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

/* Pull any incoming chat messages into the local display buffer. */
static void Online_PollChat(void)
{
        struct NetplayChatPayload chat;
        while (Netplay_DequeueChat(&chat))
        {
                int i;
                /* Shift the 4-line buffer up */
                for (i = 1; i < 4; i++)
                        memcpy(s_chatLines[i - 1], s_chatLines[i], sizeof(s_chatLines[0]));
                /* Build "Pxx: message" string */
                snprintf(s_chatLines[3], sizeof(s_chatLines[3]), "P%d: %s",
                         (int)chat.senderId, chat.message);
                if (s_chatLineCount < 4)
                        s_chatLineCount++;
        }
}

static void Online_StartRace(struct GameTracker *gGT, int trackId, int numLaps)
{
        int playerCount = Netplay_GetPlayerCount();
        g_NetplayRacing = 1;

        int i;
        for (i = 0; i < playerCount; i++)
        {
                int charId = g_NetplayCharacters[i];
                if (charId >= 0 && charId < 16)
                {
                        data.MetaDataCharacters[charId].engineID = g_NetplayEngines[i] >= 0
                                ? g_NetplayEngines[i] : 0;
                }
        }

        for (i = 0; i < NETPLAY_MAX_PLAYERS; i++)
        {
                int charId = (i < playerCount) ? g_NetplayCharacters[i] : -1;
                data.characterIDs[i] = (charId >= 0) ? (s16)charId : 0;
        }

        gGT->numPlyrCurrGame = playerCount;
        gGT->numPlyrNextGame = playerCount;
        gGT->numLaps = numLaps;
        gGT->currLEV = (s16)trackId;
        gGT->gameMode1 &= ~(ADVENTURE_MODE | TIME_TRIAL | BATTLE_MODE | MAIN_MENU);
        gGT->gameMode1 |= ARCADE_MODE;

        MM_Characters_BackupIDs();

        /* Host: broadcast RNG seed so clients start with the same random state.
         * This makes item rolls deterministic AT THE START. The seed is the
         * current value of sdata->randomNumber, which the host has been
         * advancing naturally during the lobby phase. Clients will catch up
         * as they consume RNG in sync with the host (mostly — particles and
         * other non-deterministic consumers will still drift, which is why
         * we also broadcast item pickups explicitly via ITEM_PICKUP). */
#ifdef CTR_NATIVE
        if (Netplay_GetState() == NETPLAY_STATE_HOSTING)
        {
                u32 seed = (u32)(sdata->randomNumber & 0xFFFF);
                /* Mix in the frame counter for extra entropy */
                seed |= ((u32)sdata->frameCounter << 16);
                Netplay_BroadcastRngSeed(seed, (u32)sdata->frameCounter);
        }
#endif

        sdata->ptrDesiredMenu = &data.menuQueueLoadTrack;
}

void MM_Online_Init(void)
{
        g_NetplayRacing = 0;
        /* Start at the role picker. If g_NetplayAutoJoin was set (we launched
         * with --host or --connect), skip directly to the lobby. */
        s_onlinePhase = (g_NetplayAutoJoin && Netplay_GetState() != NETPLAY_STATE_DISCONNECTED)
                        ? PHASE_LOBBY : PHASE_PICK_ROLE;
        s_onlineCursor = 0;
        s_onlineScroll = 0;

        /* If we jumped straight to the lobby (auto-join), open the chat
         * window immediately. */
        if (s_onlinePhase == PHASE_LOBBY && !s_chatWindowOpened)
        {
                if (Netplay_OpenChatWindow())
                        s_chatWindowOpened = 1;
        }
        g_charNamesInited = 0;
        memset(g_NetplayCharacters, -1, sizeof(g_NetplayCharacters));
        memset(g_NetplayEngines, -1, sizeof(g_NetplayEngines));
        g_NetplayTrackId = 0;
        g_NetplayNumLaps = 3;

        s_chatInputActive = 0;
        s_chatInputLen = 0;
        memset(s_chatInputBuf, 0, sizeof(s_chatInputBuf));
        memset(s_chatLines, 0, sizeof(s_chatLines));
        s_chatLineCount = 0;

        s_roleIsHost = 0;
        s_ifaceListCount = 0;
        s_ifaceSelected = 0;
        s_hostIPInput[0] = '\0';
        s_hostIPLen = 0;
        s_ipInputActive = 0;
        s_netplayPort = NETPLAY_DEFAULT_PORT;

        s_chatWindowOpened = 0;
}

static int Online_GetPlayerChar(int id)
{
        if (id >= 0 && id < NETPLAY_MAX_PLAYERS)
                return g_NetplayCharacters[id];
        return 0;
}

/* Call this whenever we ENTER the lobby phase. Opens the chat window
 * if it's not already open. */
static void Online_EnterLobby(void)
{
        s_onlinePhase = PHASE_LOBBY;
        s_onlineCursor = 0;
        s_onlineScroll = 0;
        if (!s_chatWindowOpened)
        {
                if (Netplay_OpenChatWindow())
                        s_chatWindowOpened = 1;
        }
}

/* Call this whenever we LEAVE the online menu entirely (back to title).
 * Closes the chat window and disconnects. */
static void Online_LeaveToTitle(void)
{
        if (s_chatWindowOpened)
        {
                Netplay_CloseChatWindow();
                s_chatWindowOpened = 0;
        }
        Netplay_Disconnect();
        OtherFX_Play(2, 1);
        MM_JumpTo_Title_Returning();
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
        Online_PollChat();

        /* Check if host sent a RETURN_LOBBY packet while we're returning from
         * a race (the racing flag may already be 0 by then). */
        if (Netplay_ConsumeReturnToLobby())
        {
                Netplay_ResetRaceState();
                /* Re-init lobby state */
                s_onlinePhase = PHASE_LOBBY;
                s_onlineCursor = 0;
                s_onlineScroll = 0;
                memset(g_NetplayCharacters, -1, sizeof(g_NetplayCharacters));
                memset(g_NetplayEngines, -1, sizeof(g_NetplayEngines));
                RECTMENU_ClearInput();
                return;
        }

        /* If we just exited a race (g_NetplayRacing was 1 and the game sent us
         * back here), reset all per-race state. */
        {
                static int s_wasRacing = 0;
                if (s_wasRacing && !g_NetplayRacing)
                {
                        s_wasRacing = 0;
                        s_onlinePhase = PHASE_LOBBY;
                        s_onlineCursor = 0;
                        s_onlineScroll = 0;
                        memset(g_NetplayCharacters, -1, sizeof(g_NetplayCharacters));
                        memset(g_NetplayEngines, -1, sizeof(g_NetplayEngines));
                }
                else if (g_NetplayRacing)
                {
                        s_wasRacing = 1;
                }
        }

        if (menu->rowSelected == -1)
        {
                if (menu->ptrPrevBox_InHierarchy != NULL)
                        menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);

                Online_LeaveToTitle();
                return;
        }

        /* ---- Handle START_RACE from host: everyone picks char simultaneously ---- */
        if (g_NetplayRaceStarting && s_onlinePhase != PHASE_CLIENT_WAITING_FOR_TRACK)
        {
                g_NetplayRaceStarting = 0;
                RECTMENU_ClearInput();
                memset(g_NetplayCharacters, -1, sizeof(g_NetplayCharacters));
                memset(g_NetplayEngines, -1, sizeof(g_NetplayEngines));
                s_onlinePhase = PHASE_PICKING_CHARACTER;
                s_onlineCursor = 0;
                s_onlineScroll = 0;
        }

        /* ====================================================================
         * DRAW
         * ====================================================================
         */

        /* ---- Title ---- */
        DecalFont_DrawLineOT(
                "ONLINE",
                ONLINE_MENU_CENTER_X, ONLINE_MENU_TITLE_Y,
                FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

        switch (s_onlinePhase)
        {
        int y;
        int numItems;
        int i;

        /* ==================== PICK ROLE ==================== */
        case PHASE_PICK_ROLE:
        {
                y = ONLINE_MENU_BODY_Y;

                DecalFont_DrawLineOT("Choose your role:", ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                y += 0x14;

                {
                        const char *roles[2] = { "Host a new game", "Connect to a host" };
                        int numRoles = 2;
                        int r;
                        for (r = 0; r < numRoles; r++)
                        {
                                int isSelected = (r == s_onlineCursor);
                                if (isSelected)
                                {
                                        RECT hl;
                                        hl.x = 0xA0; hl.y = y - 1;
                                        hl.w = 0xC0; hl.h = ONLINE_MENU_ROW_HEIGHT + 1;
                                        CTR_Box_DrawClearBox(&hl, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
                                }
                                DecalFont_DrawLineOT((char *)roles[r], ONLINE_MENU_CENTER_X, y,
                                                     FONT_SMALL, isSelected ? WHITE : ORANGE, ot);
                                y += ONLINE_MENU_ROW_HEIGHT + 4;
                        }
                }

                y += 6;
                snprintf(buf, sizeof(buf), "Port: %u   (L1/R1 to change)", s_netplayPort);
                DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);

                DecalFont_DrawLineOT("UP/DN: Select   X: Confirm   TRI: Back",
                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }

        /* ==================== PICK INTERFACE (host) ==================== */
        case PHASE_PICK_INTERFACE:
        {
                y = ONLINE_MENU_BODY_Y;

                DecalFont_DrawLineOT("Select network interface:", ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                y += 0x14;

                /* Refresh interface list every time we enter this phase, but
                 * only if we haven't already loaded it. */
                if (s_ifaceListCount == 0)
                {
                        s_ifaceListCount = Netplay_GetInterfaceList(s_ifaceList, NETPLAY_IFACE_LIST_MAX);
                }

                if (s_ifaceListCount == 0)
                {
                        DecalFont_DrawLineOT("No interfaces found", ONLINE_MENU_CENTER_X, y,
                                             FONT_SMALL, JUSTIFY_CENTER | 0x80, ot);
                }
                else
                {
                        int visible = 8;
                        if (s_onlineCursor < 0) s_onlineCursor = 0;
                        if (s_onlineCursor >= s_ifaceListCount) s_onlineCursor = s_ifaceListCount - 1;
                        if (s_onlineCursor < s_onlineScroll)
                                s_onlineScroll = s_onlineCursor;
                        if (s_onlineCursor >= s_onlineScroll + visible)
                                s_onlineScroll = s_onlineCursor - visible + 1;
                        if (s_onlineScroll < 0) s_onlineScroll = 0;

                        for (i = s_onlineScroll; i < s_onlineScroll + visible && i < s_ifaceListCount; i++)
                        {
                                int isSelected = (i == s_onlineCursor);
                                /* Narrower highlight bar that hugs the text */
                                if (isSelected)
                                {
                                        RECT hl;
                                        hl.x = 0x90; hl.y = y - 1;
                                        hl.w = 0xE0; hl.h = ONLINE_MENU_ROW_HEIGHT + 1;
                                        CTR_Box_DrawClearBox(&hl, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
                                }
                                /* Truncate the display name to fit the panel width */
                                {
                                        char displayName[40];
                                        const char *src = s_ifaceList[i].name;
                                        int maxChars = 28; /* approx 28 chars fit in 0xE0 px at FONT_SMALL */
                                        int slen = (int)strlen(src);
                                        if (slen > maxChars)
                                        {
                                                memcpy(displayName, src, maxChars - 2);
                                                displayName[maxChars - 2] = '.';
                                                displayName[maxChars - 1] = '.';
                                                displayName[maxChars] = '\0';
                                        }
                                        else
                                        {
                                                snprintf(displayName, sizeof(displayName), "%s", src);
                                        }
                                        DecalFont_DrawLineOT(displayName, ONLINE_MENU_CENTER_X, y,
                                                             FONT_SMALL, isSelected ? WHITE : ORANGE, ot);
                                }
                                y += ONLINE_MENU_ROW_HEIGHT;
                        }
                }

                DecalFont_DrawLineOT("UP/DN: Select   X: Host   TRI: Back",
                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }

        /* ==================== ENTER HOST IP (client) ==================== */
        case PHASE_ENTER_HOST_IP:
        {
                /* Virtual keyboard layout:
                 *
                 *   [0] [1] [2] [3] [4]
                 *   [5] [6] [7] [8] [9]
                 *   [.] [⌫] [Clear]
                 *
                 * The currently-highlighted key is drawn in WHITE and bigger.
                 * The IP being typed is shown ABOVE the keyboard with a
                 * blinking cursor at the end. This solves the "no preview"
                 * problem — you always see what you're about to type and
                 * what you've typed so far.
                 */
                static const char *kkeyLabels[13] = {
                        "0", "1", "2", "3", "4",
                        "5", "6", "7", "8", "9",
                        ".", "BKSP", "CLR"
                };
                int kW = 5;  /* keys per row */
                int kH = 3;  /* rows */
                int kSize = 0x18;
                int kPad = 4;
                int totalW = kW * kSize + (kW - 1) * kPad;
                int startX = ONLINE_MENU_CENTER_X - totalW / 2;

                y = ONLINE_MENU_BODY_Y;

                DecalFont_DrawLineOT("Enter host IP address:", ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                y += 0x14;

                /* IP being typed, with cursor */
                {
                        char ipDisplay[40];
                        /* Blink cursor ~ every 30 frames */
                        int blink = (sdata->frameCounter / 30) & 1;
                        snprintf(ipDisplay, sizeof(ipDisplay), "%s%c",
                                 s_hostIPInput, blink ? '|' : ' ');
                        DecalFont_DrawLineOT(ipDisplay, ONLINE_MENU_CENTER_X, y,
                                             FONT_BIG, JUSTIFY_CENTER | WHITE, ot);
                }
                y += 0x20;

                /* Port below the IP */
                snprintf(buf, sizeof(buf), "Port: %u   (L1/R1 to change)", s_netplayPort);
                DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
                y += 0x10;

                /* Keyboard grid */
                {
                        int kx, ky;
                        int idx = 0;
                        for (ky = 0; ky < kH; ky++)
                        {
                                int colsThisRow = (ky == kH - 1) ? 3 : kW;
                                /* Center the last row (3 keys) under the grid */
                                int rowStartX = startX + ((kW - colsThisRow) * (kSize + kPad)) / 2;
                                for (kx = 0; kx < colsThisRow && idx < 13; kx++, idx++)
                                {
                                        int isSpecial = (idx >= 10); /* BKSP, CLR */
                                        int isSelected = (idx == s_onlineCursor);
                                        int cellX = rowStartX + kx * (kSize + kPad);
                                        int cellY = y + ky * (kSize + kPad);
                                        int color = isSelected ? WHITE : (isSpecial ? 0x80 : ORANGE);
                                        const char *label = kkeyLabels[idx];
                                        int textY = cellY + (kSize - 8) / 2;

                                        /* Highlight box */
                                        if (isSelected)
                                        {
                                                RECT hl;
                                                hl.x = cellX;
                                                hl.y = cellY;
                                                hl.w = kSize;
                                                hl.h = kSize;
                                                CTR_Box_DrawClearBox(&hl, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
                                        }

                                        /* Label centered in the cell */
                                        DecalFont_DrawLineOT((char *)label,
                                                             cellX + kSize / 2, textY,
                                                             FONT_SMALL,
                                                             JUSTIFY_CENTER | color, ot);
                                }
                        }
                }

                DecalFont_DrawLineOT("UP/DN/LF/RT: Move   X: Type   START: Connect   TRI: Back",
                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }

        /* ==================== CONNECTING (client) ==================== */
        case PHASE_CONNECTING:
        {
                int reject;
                y = ONLINE_MENU_BODY_Y + 0x14;

                reject = Netplay_GetRejectReason();
                if (reject != NETPLAY_REJECT_NONE)
                {
                        snprintf(buf, sizeof(buf), "REJECTED: %s",
                                 Netplay_GetRejectReasonString(reject));
                        DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                             FONT_SMALL, JUSTIFY_CENTER | 0x80, ot);
                        y += 0x14;
                        DecalFont_DrawLineOT("Press TRI to go back", ONLINE_MENU_CENTER_X, y,
                                             FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                }
                else
                {
                        DecalFont_DrawLineOT("Connecting...", ONLINE_MENU_CENTER_X, y,
                                             FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

                        /* Auto-advance to lobby once connected */
                        if (Netplay_GetState() == NETPLAY_STATE_CONNECTED)
                        {
                                Online_EnterLobby();
                                RECTMENU_ClearInput();
                                return;
                        }
                        /* Auto-back to IP entry on timeout */
                        if (Netplay_GetState() == NETPLAY_STATE_DISCONNECTED)
                        {
                                s_onlinePhase = PHASE_ENTER_HOST_IP;
                                RECTMENU_ClearInput();
                                return;
                        }
                }

                DecalFont_DrawLineOT("TRI: Cancel",
                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }

        /* ==================== LOBBY ==================== */
        case PHASE_LOBBY:
        {
                int netCount;
                const struct NetplayPlayerInfo *players;
                int reject = Netplay_GetRejectReason();

                y = ONLINE_MENU_BODY_Y;

                /* Show rejection reason if any */
                if (reject != NETPLAY_REJECT_NONE)
                {
                        snprintf(buf, sizeof(buf), "REJECTED: %s",
                                 Netplay_GetRejectReasonString(reject));
                        DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                             FONT_SMALL, JUSTIFY_CENTER | 0x80, ot);
                        y += 0x10;
                }

                /* Status + IP on one line for compactness */
                if (isHost)
                        snprintf(buf, sizeof(buf), "Hosting  |  %s  |  %d player%s",
                                 Netplay_GetAddressString(), playerCount,
                                 playerCount == 1 ? "" : "s");
                else
                        snprintf(buf, sizeof(buf), "Connected  |  %d player%s",
                                 playerCount, playerCount == 1 ? "" : "s");
                DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y, FONT_SMALL,
                                     JUSTIFY_CENTER | (isHost ? TINY_GREEN : WHITE), ot);
                y += 0x12;

                /* Separator */
                DecalFont_DrawLineOT("--", ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
                y += 0x10;

                /* Player list with names, ping, character, ready status */
                players = Netplay_GetPlayers(&netCount);
                if (state == NETPLAY_STATE_HOSTING || state == NETPLAY_STATE_CONNECTED)
                {
                        for (i = 0; i < netCount && i < NETPLAY_MAX_PLAYERS; i++)
                        {
                                if (!players[i].connected)
                                        continue;

                                int color = (i == localId) ? TINY_GREEN : WHITE;
                                char safeName[24];
                                const char *src;
                                if (players[i].name[0])
                                        src = players[i].name;
                                else if (i == localId)
                                        src = Netplay_GetLocalPlayerName();
                                else
                                        src = "???";
                                snprintf(safeName, sizeof(safeName), "%.16s", src);

                                const char *readyIcon = players[i].ready ? "\x86" : "\x85";
                                int pChar = g_NetplayCharacters[i];
                                const char *charName = (pChar >= 0 && pChar < 16) ? g_charNames[pChar] : "---";
                                snprintf(buf, sizeof(buf), "P%d %s %s %s %dms",
                                         i, safeName, readyIcon, charName, (int)players[i].pingMs);
                                DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                                     FONT_SMALL, JUSTIFY_CENTER | color, ot);
                                y += ONLINE_MENU_ROW_HEIGHT;
                        }

                        /* Ready status line */
                        y += 1;
                        if (Netplay_IsLocalReady())
                                DecalFont_DrawLineOT("You: READY \x86  (SQR: unready)",
                                                     ONLINE_MENU_CENTER_X, y,
                                                     FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
                        else
                                DecalFont_DrawLineOT("You: not ready \x85  (SQR: ready)",
                                                     ONLINE_MENU_CENTER_X, y,
                                                     FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
                        y += 0x11;
                }

                /* Recent chat line */
                if (s_chatLineCount > 0)
                {
                        DecalFont_DrawLineOT(s_chatLines[s_chatLineCount - 1],
                                             ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y - 0x18,
                                             FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
                }

                /* Action bar */
                {
                        if (isHost && Netplay_IsEveryoneReady() && playerCount >= 2)
                                DecalFont_DrawLineOT("START: Race  |  SQR: Ready  |  TRI: Back",
                                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                        else
                                DecalFont_DrawLineOT("SQR: Ready  |  TRI: Back",
                                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);

                        if (Netplay_IsChatWindowOpen())
                                DecalFont_DrawLineOT("(Chat: type in the console window)",
                                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y + 0x0E,
                                                     FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
                }
                break;
        }

        /* ==================== PICKING CHARACTER ==================== */
        case PHASE_PICKING_CHARACTER:
        {
                y = ONLINE_MENU_BODY_Y;

                snprintf(buf, sizeof(buf), "Pick character  (Player %d)", localId);
                DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                y += 0x14;

                numItems = 16;

                if (s_onlineCursor < 0) s_onlineCursor = 0;
                if (s_onlineCursor >= numItems) s_onlineCursor = numItems - 1;

                {
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
                                int isTaken = 0;
                                int j;
                                for (j = 0; j < NETPLAY_MAX_PLAYERS; j++)
                                {
                                        if (j == localId) continue;
                                        if (g_NetplayCharacters[j] == i)
                                        {
                                                isTaken = 1;
                                                break;
                                        }
                                }

                                if (isSelected)
                                {
                                        RECT hl;
                                        hl.x = 0x80;
                                        hl.y = y - 1;
                                        hl.w = 0x100;
                                        hl.h = ONLINE_MENU_ROW_HEIGHT + 1;
                                        CTR_Box_DrawClearBox(&hl, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
                                }

                                int color;
                                if (isTaken)
                                        color = 0x80;
                                else if (isSelected)
                                        color = WHITE;
                                else
                                        color = ORANGE;

                                DecalFont_DrawLineOT((char *)g_charNames[i], ONLINE_MENU_CENTER_X, y,
                                                     FONT_SMALL, color, ot);
                                y += ONLINE_MENU_ROW_HEIGHT;
                        }
                }

                DecalFont_DrawLineOT("UP/DN: Select   X: Confirm   TRI: Back",
                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }

        /* ==================== PICKING ENGINE ==================== */
        case PHASE_PICKING_ENGINE:
        {
                int localChar = g_NetplayCharacters[localId];
                y = ONLINE_MENU_BODY_Y;

                snprintf(buf, sizeof(buf), "Choose your engine  (P%d)", localId);
                DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                y += 0x14;

                if (localChar >= 0 && localChar < 16)
                {
                        snprintf(buf, sizeof(buf), "Character: %s", g_charNames[localChar]);
                        DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                             FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
                }
                y += 0x10;

                {
                        int e;
                        for (e = 0; e < NUM_ENGINES; e++)
                        {
                                int isSelected = (e == s_onlineCursor);

                                if (isSelected)
                                {
                                        RECT hl;
                                        hl.x = 0x90; hl.y = y - 1;
                                        hl.w = 0xE0; hl.h = ONLINE_MENU_ROW_HEIGHT + 1;
                                        CTR_Box_DrawClearBox(&hl, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
                                }

                                DecalFont_DrawLineOT((char *)g_engineNames[e], ONLINE_MENU_CENTER_X, y,
                                                     FONT_SMALL, isSelected ? WHITE : ORANGE, ot);
                                y += ONLINE_MENU_ROW_HEIGHT;
                        }
                }

                DecalFont_DrawLineOT("UP/DN: Select   X: Confirm   TRI: Back",
                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }

        /* ==================== WAITING FOR PLAYERS ==================== */
        case PHASE_WAITING_FOR_PLAYERS:
        {
                y = ONLINE_MENU_BODY_Y + 0x0E;

                DecalFont_DrawLineOT("Waiting for players to pick...", ONLINE_MENU_CENTER_X, y,
                                     FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);
                y += 0x16;

                /* Show who is still missing */
                {
                        int n;
                        int pCount;
                        const struct NetplayPlayerInfo *pInfo = Netplay_GetPlayers(&pCount);
                        for (n = 0; n < pCount && n < NETPLAY_MAX_PLAYERS; n++)
                        {
                                if (!pInfo[n].connected) continue;
                                const char *pName = Netplay_GetPlayerName((u8)n);
                                if (g_NetplayCharacters[n] < 0)
                                {
                                        snprintf(buf, sizeof(buf), "P%d %s: picking character", n, pName);
                                        DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                                             FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
                                        y += 0x10;
                                }
                                else if (g_NetplayEngines[n] < 0)
                                {
                                        snprintf(buf, sizeof(buf), "P%d %s: picking engine", n, pName);
                                        DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                                             FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
                                        y += 0x10;
                                }
                        }
                }

                DecalFont_DrawLineOT("TRI: Back", ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }

        /* ==================== HOST PICKING TRACK ==================== */
        case PHASE_HOST_PICKING_TRACK:
        {
                y = ONLINE_MENU_BODY_Y;

                DecalFont_DrawLineOT("Choose a track:", ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                y += 0x14;

                numItems = 18;

                if (s_onlineCursor < 0) s_onlineCursor = 0;
                if (s_onlineCursor >= numItems) s_onlineCursor = numItems - 1;

                {
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
                }

                snprintf(buf, sizeof(buf), "Laps: %d", g_NetplayNumLaps);
                DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                     FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);

                DecalFont_DrawLineOT("UP/DN: Select   X: Confirm   L1/R1: Laps   TRI: Back",
                                     ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }

        /* ==================== CLIENT WAITING FOR TRACK ==================== */
        case PHASE_CLIENT_WAITING_FOR_TRACK:
        {
                y = ONLINE_MENU_BODY_Y + 0x0E;

                if (g_NetplayCharacters[0] >= 0)
                {
                        int hostChar = g_NetplayCharacters[0];
                        snprintf(buf, sizeof(buf), "Host: %s", hostChar >= 0 && hostChar < 16
                                 ? g_charNames[hostChar] : "???");
                        DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                             FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
                        y += 0x10;
                }

                int localChar = g_NetplayCharacters[localId];
                int localEngine = g_NetplayEngines[localId];
                if (localChar >= 0 && localChar < 16)
                {
                        snprintf(buf, sizeof(buf), "You: %s", g_charNames[localChar]);
                        DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                             FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
                        y += 0x10;
                }
                if (localEngine >= 0 && localEngine < NUM_ENGINES)
                {
                        snprintf(buf, sizeof(buf), "Engine: %s", g_engineNames[localEngine]);
                        DecalFont_DrawLineOT(buf, ONLINE_MENU_CENTER_X, y,
                                             FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
                        y += 0x10;
                }

                DecalFont_DrawLineOT("Host is choosing track...", ONLINE_MENU_CENTER_X, y,
                                     FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

                /* Check if host sent track select */
                if (g_NetplayRaceStarting)
                {
                        g_NetplayRaceStarting = 0;
                        OtherFX_Play(1, 1);
                        RECTMENU_ClearInput();
                        Online_StartRace(gGT, g_NetplayTrackId, g_NetplayNumLaps);
                        return;
                }

                DecalFont_DrawLineOT("TRI: Back", ONLINE_MENU_CENTER_X, ONLINE_MENU_HELP_Y,
                                     FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
                break;
        }
        }

        /* ---- Panel border (centered, slightly tighter) ---- */
        border.x = 0x50;
        border.y = ONLINE_MENU_TITLE_Y - 0x0C;
        border.w = 0x1E0;
        border.h = ONLINE_MENU_HELP_Y - ONLINE_MENU_TITLE_Y + 0x20;
        RECTMENU_DrawInnerRect(&border, 0, ot);

        /* ====================================================================
         * INPUT HANDLING
         * ====================================================================
         */

        /* Note: chat is no longer typed ingame. It's typed in the separate
         * chat window (opened when entering the lobby). The input handling
         * for that window happens in Netplay_PollChatInput(), called from
         * Netplay_Poll(). Here we only handle gamepad input for menu
         * navigation. */

        switch (s_onlinePhase)
        {
        case PHASE_PICK_ROLE:
                if (sdata->buttonTapPerPlayer[0] & BTN_UP)
                {
                        s_onlineCursor = (s_onlineCursor - 1 + 2) % 2;
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                }
                else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
                {
                        s_onlineCursor = (s_onlineCursor + 1) % 2;
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                }
                else if (sdata->buttonTapPerPlayer[0] & BTN_L1)
                {
                        if (s_netplayPort > 1024) s_netplayPort--;
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                }
                else if (sdata->buttonTapPerPlayer[0] & BTN_R1)
                {
                        if (s_netplayPort < 65535) s_netplayPort++;
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                }
                else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
                {
                        OtherFX_Play(1, 1);
                        RECTMENU_ClearInput();
                        s_roleIsHost = (s_onlineCursor == 0);
                        if (s_roleIsHost)
                        {
                                /* Go to interface picker */
                                s_onlinePhase = PHASE_PICK_INTERFACE;
                                s_onlineCursor = 0;
                                s_onlineScroll = 0;
                                s_ifaceListCount = 0; /* force refresh */
                        }
                        else
                        {
                                /* Go to IP entry */
                                s_onlinePhase = PHASE_ENTER_HOST_IP;
                                s_onlineCursor = 0;
                                s_hostIPInput[0] = '\0';
                                s_hostIPLen = 0;
                        }
                }
                else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
                {
                        Online_LeaveToTitle();
                }
                break;

        case PHASE_PICK_INTERFACE:
                if (s_ifaceListCount > 0)
                {
                        if (sdata->buttonTapPerPlayer[0] & BTN_UP)
                        {
                                s_onlineCursor = (s_onlineCursor - 1 + s_ifaceListCount) % s_ifaceListCount;
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
                        {
                                s_onlineCursor = (s_onlineCursor + 1) % s_ifaceListCount;
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
                        {
                                /* Start hosting on the selected interface */
                                OtherFX_Play(1, 1);
                                RECTMENU_ClearInput();

                                /* Pass the CLEAN interface name (without IP) to
                                 * SetInterfaceName. Before this was passing the
                                 * display name "enp150 (192.168.10.6)" which
                                 * never matched ifa->ifa_name in ResolveLocalAddress,
                                 * causing the IP to fall back to gethostname()
                                 * which on Linux yields 127.0.1.1. */
                                Netplay_SetInterfaceName(s_ifaceList[s_onlineCursor].ifaceName);

                                /* Set the player name to something reasonable */
                                if (!Netplay_GetLocalPlayerName() ||
                                    Netplay_GetLocalPlayerName()[0] == '\0' ||
                                    strcmp(Netplay_GetLocalPlayerName(), "Me") == 0)
                                        Netplay_SetPlayerName("Host");

                                Netplay_Init();
                                if (Netplay_Host(s_netplayPort))
                                {
                                        /* Override the address string with the
                                         * actual interface IP. This bypasses
                                         * the DNS/hostname fallback entirely. */
                                        Netplay_SetAddressString(
                                                s_ifaceList[s_onlineCursor].ipString,
                                                s_netplayPort);
                                        Online_EnterLobby();
                                }
                                else
                                {
                                        fprintf(stderr, "[Online] Failed to host on port %u\n", s_netplayPort);
                                        fflush(stderr);
                                }
                        }
                }
                if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
                {
                        OtherFX_Play(2, 1);
                        RECTMENU_ClearInput();
                        s_onlinePhase = PHASE_PICK_ROLE;
                        s_onlineCursor = 0;
                }
                break;

        case PHASE_ENTER_HOST_IP:
                /* Virtual keyboard navigation. The grid is 5 cols x 3 rows:
                 *   row 0: 0 1 2 3 4
                 *   row 1: 5 6 7 8 9
                 *   row 2: . BKSP CLR (and 2 empty cells, but we wrap so
                 *          the cursor never lands on them)
                 * Total 13 valid keys (indices 0..12).
                 */
                {
                        int kW = 5;
                        int kTotal = 13;

                        if (s_onlineCursor < 0) s_onlineCursor = 0;
                        if (s_onlineCursor >= kTotal) s_onlineCursor = kTotal - 1;

                        if (sdata->buttonTapPerPlayer[0] & BTN_UP)
                        {
                                s_onlineCursor -= kW;
                                if (s_onlineCursor < 0) s_onlineCursor += kTotal;
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
                        {
                                s_onlineCursor += kW;
                                if (s_onlineCursor >= kTotal) s_onlineCursor -= kTotal;
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_LEFT)
                        {
                                s_onlineCursor = (s_onlineCursor - 1 + kTotal) % kTotal;
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_RIGHT)
                        {
                                s_onlineCursor = (s_onlineCursor + 1) % kTotal;
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_L1)
                        {
                                if (s_netplayPort > 1024) s_netplayPort--;
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_R1)
                        {
                                if (s_netplayPort < 65535) s_netplayPort++;
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
                        {
                                /* Type the selected key */
                                if (s_onlineCursor < 10)
                                {
                                        /* Digit 0-9 */
                                        if (s_hostIPLen < (int)sizeof(s_hostIPInput) - 1)
                                        {
                                                s_hostIPInput[s_hostIPLen++] = (char)('0' + s_onlineCursor);
                                                s_hostIPInput[s_hostIPLen] = '\0';
                                        }
                                }
                                else if (s_onlineCursor == 10)
                                {
                                        /* Dot */
                                        if (s_hostIPLen < (int)sizeof(s_hostIPInput) - 1)
                                        {
                                                s_hostIPInput[s_hostIPLen++] = '.';
                                                s_hostIPInput[s_hostIPLen] = '\0';
                                        }
                                }
                                else if (s_onlineCursor == 11)
                                {
                                        /* Backspace */
                                        if (s_hostIPLen > 0)
                                        {
                                                s_hostIPLen--;
                                                s_hostIPInput[s_hostIPLen] = '\0';
                                        }
                                }
                                else if (s_onlineCursor == 12)
                                {
                                        /* Clear */
                                        s_hostIPLen = 0;
                                        s_hostIPInput[0] = '\0';
                                }
                                OtherFX_Play(0, 1);
                                RECTMENU_ClearInput();
                        }
                        else if (sdata->buttonTapPerPlayer[0] & (BTN_SQUARE | BTN_START))
                        {
                                /* SQUARE or START = Connect (if we have an IP) */
                                if (s_hostIPLen > 0)
                                {
                                        OtherFX_Play(1, 1);
                                        RECTMENU_ClearInput();

                                        if (!Netplay_GetLocalPlayerName() ||
                                            Netplay_GetLocalPlayerName()[0] == '\0' ||
                                            strcmp(Netplay_GetLocalPlayerName(), "Me") == 0)
                                                Netplay_SetPlayerName("Player");

                                        Netplay_Init();
                                        if (Netplay_Connect(s_hostIPInput, s_netplayPort))
                                        {
                                                s_onlinePhase = PHASE_CONNECTING;
                                        }
                                        else
                                        {
                                                fprintf(stderr, "[Online] Failed to connect to %s:%u\n",
                                                        s_hostIPInput, s_netplayPort);
                                                fflush(stderr);
                                        }
                                }
                                else
                                {
                                        OtherFX_Play(2, 1);
                                        RECTMENU_ClearInput();
                                }
                        }
                        else if (sdata->buttonTapPerPlayer[0] & BTN_TRIANGLE)
                        {
                                OtherFX_Play(2, 1);
                                RECTMENU_ClearInput();
                                s_onlinePhase = PHASE_PICK_ROLE;
                                s_onlineCursor = 0;
                        }
                }
                break;

        case PHASE_CONNECTING:
                if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
                {
                        OtherFX_Play(2, 1);
                        RECTMENU_ClearInput();
                        Netplay_Disconnect();
                        s_onlinePhase = PHASE_ENTER_HOST_IP;
                }
                break;

        case PHASE_LOBBY:
                /* Square = toggle ready (chat is now in the separate window) */
                if (sdata->buttonTapPerPlayer[0] & BTN_SQUARE)
                {
                        Netplay_SetLocalReady(!Netplay_IsLocalReady());
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                        return;
                }
                if (sdata->buttonTapPerPlayer[0] & BTN_START)
                {
                        if (isHost && playerCount >= 2 && Netplay_IsEveryoneReady())
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

                        /* Block if another player already picked this character */
                        int conflict = 0;
                        int p;
                        for (p = 0; p < NETPLAY_MAX_PLAYERS; p++)
                        {
                                if (p == localId) continue;
                                if (g_NetplayCharacters[p] == chosenChar)
                                {
                                        conflict = 1;
                                        break;
                                }
                        }

                        if (conflict)
                        {
                                OtherFX_Play(2, 1);
                                RECTMENU_ClearInput();
                        }
                        else
                        {
                                OtherFX_Play(1, 1);
                                RECTMENU_ClearInput();

                                g_NetplayCharacters[localId] = chosenChar;
                                {
                                        u8 payload = (u8)chosenChar;
                                        if (isHost)
                                                Netplay_BroadcastPacket(NETPLAY_PACKET_CHARACTER_SELECT,
                                                                        sizeof(payload), &payload);
                                        else
                                                Netplay_BroadcastPacket(NETPLAY_PACKET_CHARACTER_SELECT,
                                                                        sizeof(payload), &payload);
                                }

                                s_onlinePhase = PHASE_PICKING_ENGINE;
                                s_onlineCursor = 0;
                                s_onlineScroll = 0;
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

        case PHASE_PICKING_ENGINE:
                if (sdata->buttonTapPerPlayer[0] & BTN_UP)
                {
                        s_onlineCursor = (s_onlineCursor - 1 + NUM_ENGINES) % NUM_ENGINES;
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                }
                else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
                {
                        s_onlineCursor = (s_onlineCursor + 1) % NUM_ENGINES;
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                }
                else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
                {
                        int chosenEngine = s_onlineCursor;
                        OtherFX_Play(1, 1);
                        RECTMENU_ClearInput();

                        g_NetplayEngines[localId] = chosenEngine;
                        {
                                u8 payload = (u8)chosenEngine;
                                Netplay_BroadcastPacket(NETPLAY_PACKET_ENGINE_SELECT,
                                                        sizeof(payload), &payload);
                        }

                        if (isHost)
                        {
                                /* Check if all connected players have character + engine */
                                int allPlayersReady = 1;
                                int p;
                                int pCount;
                                const struct NetplayPlayerInfo *pInfo = Netplay_GetPlayers(&pCount);
                                for (p = 0; p < pCount && p < NETPLAY_MAX_PLAYERS; p++)
                                {
                                        if (!pInfo[p].connected) continue;
                                        if (g_NetplayCharacters[p] < 0 || g_NetplayEngines[p] < 0)
                                        {
                                                allPlayersReady = 0;
                                                break;
                                        }
                                }
                                if (allPlayersReady)
                                {
                                        s_onlinePhase = PHASE_HOST_PICKING_TRACK;
                                        s_onlineCursor = 0;
                                        s_onlineScroll = 0;
                                }
                                else
                                {
                                        s_onlinePhase = PHASE_WAITING_FOR_PLAYERS;
                                        s_onlineCursor = 0;
                                        s_onlineScroll = 0;
                                }
                        }
                        else
                        {
                                s_onlinePhase = PHASE_CLIENT_WAITING_FOR_TRACK;
                        }
                }
                else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
                {
                        OtherFX_Play(2, 1);
                        RECTMENU_ClearInput();
                        s_onlinePhase = PHASE_PICKING_CHARACTER;
                        s_onlineCursor = g_NetplayCharacters[localId];
                        if (s_onlineCursor < 0) s_onlineCursor = 0;
                }
                break;

        case PHASE_WAITING_FOR_PLAYERS:
                /* Host: check if all players have sent char + engine */
                if (isHost)
                {
                        int allReady = 1;
                        int p;
                        int pCount;
                        const struct NetplayPlayerInfo *pInfo = Netplay_GetPlayers(&pCount);
                        for (p = 0; p < pCount && p < NETPLAY_MAX_PLAYERS; p++)
                        {
                                if (!pInfo[p].connected) continue;
                                if (g_NetplayCharacters[p] < 0 || g_NetplayEngines[p] < 0)
                                {
                                        allReady = 0;
                                        break;
                                }
                        }
                        if (allReady)
                        {
                                s_onlinePhase = PHASE_HOST_PICKING_TRACK;
                                s_onlineCursor = 0;
                                s_onlineScroll = 0;
                                RECTMENU_ClearInput();
                        }
                }
                if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
                {
                        OtherFX_Play(2, 1);
                        RECTMENU_ClearInput();
                        s_onlinePhase = PHASE_LOBBY;
                        s_onlineCursor = 0;
                        s_onlineScroll = 0;
                        g_NetplayCharacters[localId] = -1;
                        g_NetplayEngines[localId] = -1;
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
                else if (sdata->buttonTapPerPlayer[0] & BTN_L1)
                {
                        g_NetplayNumLaps = (g_NetplayNumLaps > 1) ? g_NetplayNumLaps - 1 : 1;
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                }
                else if (sdata->buttonTapPerPlayer[0] & BTN_R1)
                {
                        g_NetplayNumLaps = (g_NetplayNumLaps < 9) ? g_NetplayNumLaps + 1 : 9;
                        OtherFX_Play(0, 1);
                        RECTMENU_ClearInput();
                }
                else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
                {
                        s16 levID = D230.arcadeTracks[s_onlineCursor].levID;

                        OtherFX_Play(1, 1);
                        RECTMENU_ClearInput();

                        g_NetplayTrackId = (int)levID;

                        {
                                /* Include character/engine arrays so clients
                                 * don't miss a player's choice when the
                                 * CHARACTER_SELECT relay arrives late. */
                                s8 payload[3 + NETPLAY_MAX_PLAYERS * 2];
                                payload[0] = (s8)levID;
                                payload[1] = (s8)g_NetplayNumLaps;
                                payload[2] = (s8)Netplay_GetPlayerCount();
                                int pi;
                                for (pi = 0; pi < NETPLAY_MAX_PLAYERS; pi++)
                                {
                                        payload[3 + pi] = (s8)g_NetplayCharacters[pi];
                                        payload[3 + NETPLAY_MAX_PLAYERS + pi] = (s8)g_NetplayEngines[pi];
                                }
                                Netplay_BroadcastPacket(NETPLAY_PACKET_TRACK_SELECT,
                                                        sizeof(payload), payload);
                        }

                        Online_StartRace(gGT, g_NetplayTrackId, g_NetplayNumLaps);
                        return;
                }
                else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
                {
                        OtherFX_Play(2, 1);
                        RECTMENU_ClearInput();
                        s_onlinePhase = PHASE_PICKING_CHARACTER;
                        s_onlineCursor = g_NetplayCharacters[0];
                        if (s_onlineCursor < 0) s_onlineCursor = 0;
                }
                break;

        case PHASE_CLIENT_WAITING_FOR_TRACK:
                if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
                {
                        OtherFX_Play(2, 1);
                        RECTMENU_ClearInput();
                        s_onlinePhase = PHASE_PICKING_CHARACTER;
                        s_onlineCursor = g_NetplayCharacters[localId];
                        if (s_onlineCursor < 0) s_onlineCursor = 0;
                }
                break;
        }
}

#ifdef CTR_NATIVE
/* ---- PC keyboard helpers for the IP input field ---- */
int MM_Online_IsIpActive(void)
{
        return (s_onlinePhase == PHASE_ENTER_HOST_IP) ? 1 : 0;
}

void MM_Online_IpTypeChar(char c)
{
        if (c < ' ' || c > '~')
                return;
        if (s_hostIPLen < (int)sizeof(s_hostIPInput) - 1)
        {
                s_hostIPInput[s_hostIPLen++] = c;
                s_hostIPInput[s_hostIPLen] = '\0';
                OtherFX_Play(0, 1);
                RECTMENU_ClearInput();
        }
}

void MM_Online_IpBackspace(void)
{
        if (s_hostIPLen > 0)
        {
                s_hostIPLen--;
                s_hostIPInput[s_hostIPLen] = '\0';
                OtherFX_Play(0, 1);
                RECTMENU_ClearInput();
        }
}

void MM_Online_IpClear(void)
{
        s_hostIPLen = 0;
        s_hostIPInput[0] = '\0';
        OtherFX_Play(0, 1);
        RECTMENU_ClearInput();
}

void MM_Online_IpConfirm(void)
{
        if (s_hostIPLen > 0)
        {
                OtherFX_Play(1, 1);
                RECTMENU_ClearInput();

                if (!Netplay_GetLocalPlayerName() ||
                    Netplay_GetLocalPlayerName()[0] == '\0' ||
                    strcmp(Netplay_GetLocalPlayerName(), "Me") == 0)
                        Netplay_SetPlayerName("Player");

                Netplay_Init();
                if (Netplay_Connect(s_hostIPInput, s_netplayPort))
                        s_onlinePhase = PHASE_CONNECTING;
                else
                        fprintf(stderr, "[Online] Failed to connect to %s:%u\n",
                                s_hostIPInput, s_netplayPort);
        }
        else
        {
                OtherFX_Play(2, 1);
                RECTMENU_ClearInput();
        }
}

void MM_Online_IpCancel(void)
{
        OtherFX_Play(2, 1);
        RECTMENU_ClearInput();
        s_onlinePhase = PHASE_PICK_ROLE;
        s_onlineCursor = 0;
}
#endif
