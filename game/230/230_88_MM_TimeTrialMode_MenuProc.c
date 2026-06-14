#include <common.h>
#include <stdio.h>
#include <platform/native_memcard.h>
#include <platform/native_assets.h>

#define TTMODE_MENU_VISIBLE_ROWS 2
#define TTMODE_MENU_CENTER_X     0x100
#define TTMODE_MENU_PANEL_W      0x1C0
#define TTMODE_MENU_TITLE_Y      0x1C
#define TTMODE_MENU_LIST_TOP_Y   0x3C
#define TTMODE_MENU_ROW_HEIGHT   0x10
#define TTMODE_MENU_LEFT_X       (TTMODE_MENU_CENTER_X - TTMODE_MENU_PANEL_W / 2)
#define TTMODE_MENU_RIGHT_X      (TTMODE_MENU_CENTER_X + TTMODE_MENU_PANEL_W / 2)
#define TTMODE_MENU_NAME_X       (TTMODE_MENU_LEFT_X + 0x10)
#define TTMODE_MENU_VALUE_X      (TTMODE_MENU_RIGHT_X - 0x60)
#define TTMODE_MENU_HIGHLIGHT_W  (TTMODE_MENU_PANEL_W - 0x20)

#define TTMODE_ROW_CLASSIC   0
#define TTMODE_ROW_VS_NYPER  1
#define TTMODE_ROW_COUNT     2

static int s_ttModeSelectedIndex = 0;

int MM_TimeTrialMode_LoadDevGhost(s16 levID);

void MM_TimeTrialMode_Init(void)
{
    s_ttModeSelectedIndex = 0;
}

void MM_TimeTrialMode_MenuProc(struct RectMenu *menu)
{
    struct GameTracker *gGT = sdata->gGT;
    u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
    int y;
    RECT highlight, borders;
    int selected = s_ttModeSelectedIndex;

    if (menu->rowSelected == -1)
    {
        if (menu->ptrPrevBox_InHierarchy != NULL)
            menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
        s_ttModeSelectedIndex = 0;
        OtherFX_Play(2, 1);
        MM_JumpTo_Title_Returning();
        return;
    }

    DecalFont_DrawLineOT(
        "TIME TRIAL MODE",
        TTMODE_MENU_CENTER_X, TTMODE_MENU_TITLE_Y,
        FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

    highlight.x = TTMODE_MENU_LEFT_X + 4;
    highlight.y = TTMODE_MENU_LIST_TOP_Y + (selected * TTMODE_MENU_ROW_HEIGHT) - 1;
    highlight.w = TTMODE_MENU_HIGHLIGHT_W;
    highlight.h = TTMODE_MENU_ROW_HEIGHT + 1;
    CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);

    y = TTMODE_MENU_LIST_TOP_Y;

    {
        DecalFont_DrawLineOT("Classic", TTMODE_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        DecalFont_DrawLineOT("Normal Time Trial", TTMODE_MENU_VALUE_X, y, FONT_SMALL, selected == TTMODE_ROW_CLASSIC ? TINY_GREEN : WHITE, ot);
        y += TTMODE_MENU_ROW_HEIGHT;
    }

    {
        DecalFont_DrawLineOT("Vs Nyper", TTMODE_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        DecalFont_DrawLineOT("Race vs Dev Ghosts", TTMODE_MENU_VALUE_X, y, FONT_SMALL, selected == TTMODE_ROW_VS_NYPER ? TINY_GREEN : WHITE, ot);
        y += TTMODE_MENU_ROW_HEIGHT;
    }

    int listHeight = TTMODE_MENU_ROW_HEIGHT * TTMODE_MENU_VISIBLE_ROWS;
    borders.x = TTMODE_MENU_CENTER_X - TTMODE_MENU_PANEL_W / 2 - 6;
    borders.y = TTMODE_MENU_TITLE_Y - 8;
    borders.w = TTMODE_MENU_PANEL_W + 12;
    borders.h = listHeight + (TTMODE_MENU_LIST_TOP_Y - TTMODE_MENU_TITLE_Y) + 0x30;
    RECTMENU_DrawInnerRect(&borders, 0, ot);

    if (sdata->buttonTapPerPlayer[0] & BTN_UP)
    {
        s_ttModeSelectedIndex = (s_ttModeSelectedIndex - 1 + TTMODE_ROW_COUNT) % TTMODE_ROW_COUNT;
        OtherFX_Play(0, 1);
        RECTMENU_ClearInput();
    }
    else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
    {
        s_ttModeSelectedIndex = (s_ttModeSelectedIndex + 1) % TTMODE_ROW_COUNT;
        OtherFX_Play(0, 1);
        RECTMENU_ClearInput();
    }

    if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
    {
        OtherFX_Play(1, 1);
        RECTMENU_ClearInput();

        if (s_ttModeSelectedIndex == TTMODE_ROW_CLASSIC)
        {
            SelectProfile_ToggleMode(0x30);
            sdata->ptrDesiredMenu = &data.menuGhostSelection;
        }
        else if (s_ttModeSelectedIndex == TTMODE_ROW_VS_NYPER)
        {
            // Load Dev Ghost for selected track
            struct MainMenu_LevelRow *selectMenu = &D230.arcadeTracks[0];
            int currTrack = sdata->trackSelBackup;
            s16 levID = selectMenu[currTrack].levID;

            // Try to load Dev Ghost from DevGhost/track_XX.ghost
            if (MM_TimeTrialMode_LoadDevGhost(levID))
            {
                sdata->boolReplayHumanGhost = 1;
                sdata->ptrDesiredMenu = &data.menuQueueLoadTrack;
            }
            else
            {
                // Fallback: no ghost
                sdata->boolReplayHumanGhost = 0;
                sdata->ptrDesiredMenu = &data.menuQueueLoadTrack;
            }
        }
        return;
    }

    if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
    {
        OtherFX_Play(2, 1);
        RECTMENU_ClearInput();
        MM_JumpTo_Title_Returning();
    }
}

int MM_TimeTrialMode_LoadDevGhost(s16 levID)
{
    char path[1024];
    void *buffer = sdata->ptrGhostTapePlaying;
    struct GameTracker *gGT = sdata->gGT;
    enum NativeMemcardResult result;

    // Build path relative to executable directory (like assets/memcards/mods)
    snprintf(path, sizeof(path), "DevGhost/track_%02d.ghost", levID);
    if (!NativeAssets_BuildPathFromBase(path, path, sizeof(path)))
    {
        printf("[DevGhost] Failed to build path for levID %d\n", levID);
        return 0;
    }

    printf("[DevGhost] Trying to load: %s\n", path);
    result = NativeMemcard_ReadFileDirect(path, buffer, 0x3E00, 0);
    printf("[DevGhost] ReadFileDirect result: %d\n", result);

    if (result != NATIVE_MEMCARD_OK)
    {
        // Fallback: try standard memcard location
        snprintf(path, sizeof(path), "memcards/slot0/track_%02d.ghost", levID);
        if (!NativeAssets_BuildPathFromBase(path, path, sizeof(path)))
        {
            printf("[DevGhost] Failed to build fallback path\n");
            return 0;
        }
        printf("[DevGhost] Trying fallback: %s\n", path);
        result = NativeMemcard_ReadFileDirect(path, buffer, 0x3E00, 0);
        printf("[DevGhost] Fallback result: %d\n", result);
    }

    if (result != NATIVE_MEMCARD_OK)
    {
        printf("[DevGhost] Failed to load ghost for levID %d\n", levID);
        return 0;
    }

    struct GhostHeader *gh = (struct GhostHeader *)buffer;
    printf("[DevGhost] Ghost header: version=%d, size=%d, levelID=%d, charID=%d, time=%d\n",
           gh->version, gh->size, gh->levelID, gh->characterID, gh->timeElapsedInRace);

    if (gh->version != -4)
    {
        printf("[DevGhost] Invalid ghost version: %d (expected -4)\n", gh->version);
        return 0;
    }

    gGT->timeToBeatInTimeTrial_ForCurrentEvent = gh->timeElapsedInRace;
    sdata->boolReplayHumanGhost = 1;
    printf("[DevGhost] Successfully loaded ghost, timeToBeat=%d\n", gh->timeElapsedInRace);
    return 1;
}