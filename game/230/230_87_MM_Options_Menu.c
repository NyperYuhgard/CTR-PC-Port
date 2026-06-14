#include <common.h>
#include <platform/native_mods.h>

#define OPTIONS_MENU_VISIBLE_ROWS 7
#define OPTIONS_MENU_CENTER_X     0x100
#define OPTIONS_MENU_PANEL_W      0x1C0
#define OPTIONS_MENU_TITLE_Y      0x1C
#define OPTIONS_MENU_LIST_TOP_Y   0x3C
#define OPTIONS_MENU_ROW_HEIGHT   0x10
#define OPTIONS_MENU_LEFT_X       (OPTIONS_MENU_CENTER_X - OPTIONS_MENU_PANEL_W / 2)
#define OPTIONS_MENU_RIGHT_X      (OPTIONS_MENU_CENTER_X + OPTIONS_MENU_PANEL_W / 2)
#define OPTIONS_MENU_NAME_X       (OPTIONS_MENU_LEFT_X + 0x10)
#define OPTIONS_MENU_VALUE_X      (OPTIONS_MENU_RIGHT_X - 0x60)
#define OPTIONS_MENU_HIGHLIGHT_W  (OPTIONS_MENU_PANEL_W - 0x20)

#define OPTION_ROW_FX        0
#define OPTION_ROW_MUSIC     1
#define OPTION_ROW_VOICE     2
#define OPTION_ROW_MODE      3
#define OPTION_ROW_ASPECT    4
#define OPTION_ROW_FPS       5
#define OPTION_ROW_FILTER    6
#define OPTION_ROW_COUNT     7

#define ASPECT_4_3     0
#define ASPECT_16_9    1
#define ASPECT_STRETCH 2

static int s_optionsSelectedIndex = 0;

extern int g_cfg_bilinearFiltering;
extern int g_cfg_aspectMode;
extern void NativeRenderer_UpdateSwapIntervalState(int swapInterval);
extern void NativeRenderer_SetAspectMode(int mode);

void MM_Options_Init(void)
{
    s_optionsSelectedIndex = 0;
}

void MM_Options_MenuProc(struct RectMenu *menu)
{
    struct GameTracker *gGT = sdata->gGT;
    u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
    int y;
    RECT highlight, borders;
    int selected = s_optionsSelectedIndex;

    if (menu->rowSelected == -1)
    {
        if (menu->ptrPrevBox_InHierarchy != NULL)
            menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
        s_optionsSelectedIndex = 0;
        OtherFX_Play(2, 1);
        MM_JumpTo_Title_Returning();
        return;
    }

    DecalFont_DrawLineOT(
        sdata->lngStrings[LNG_OPTIONS],
        OPTIONS_MENU_CENTER_X, OPTIONS_MENU_TITLE_Y,
        FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

    highlight.x = OPTIONS_MENU_LEFT_X + 4;
    highlight.y = OPTIONS_MENU_LIST_TOP_Y + (selected * OPTIONS_MENU_ROW_HEIGHT) - 1;
    highlight.w = OPTIONS_MENU_HIGHLIGHT_W;
    highlight.h = OPTIONS_MENU_ROW_HEIGHT + 1;
    CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);

    y = OPTIONS_MENU_LIST_TOP_Y;

    {
        int vol = howl_VolumeGet(0) & 0xff;
        DecalFont_DrawLineOT(sdata->lngStrings[LNG_FX], OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", vol);
        DecalFont_DrawLineOT(buf, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
        y += OPTIONS_MENU_ROW_HEIGHT;
    }

    {
        int vol = howl_VolumeGet(1) & 0xff;
        DecalFont_DrawLineOT(sdata->lngStrings[LNG_MUSIC], OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", vol);
        DecalFont_DrawLineOT(buf, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
        y += OPTIONS_MENU_ROW_HEIGHT;
    }

    {
        int vol = howl_VolumeGet(2) & 0xff;
        DecalFont_DrawLineOT(sdata->lngStrings[LNG_VOICE], OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", vol);
        DecalFont_DrawLineOT(buf, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
        y += OPTIONS_MENU_ROW_HEIGHT;
    }

    {
        int mode = howl_ModeGet();
        DecalFont_DrawLineOT("Sound Type", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        DecalFont_DrawLineOT(sdata->lngStrings[mode ? LNG_STEREO : LNG_MONO], OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
        y += OPTIONS_MENU_ROW_HEIGHT;
    }

    {
        char *label = g_cfg_aspectMode == ASPECT_STRETCH ? "STRETCH" :
                      g_cfg_aspectMode == ASPECT_16_9 ? "16:9" : "4:3";
        int color = g_cfg_aspectMode == ASPECT_4_3 ? WHITE : TINY_GREEN;
        DecalFont_DrawLineOT("ASPECT", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        DecalFont_DrawLineOT(label, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, color, ot);
        y += OPTIONS_MENU_ROW_HEIGHT;
    }

    {
        char *fpsLabels[] = {"30", "60", "60I"};
        int fpsColor = g_cfg_60fpsMode == 2 ? TINY_GREEN : g_cfg_60fpsMode == 1 ? TINY_GREEN : WHITE;
        DecalFont_DrawLineOT("FPS", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        DecalFont_DrawLineOT(fpsLabels[g_cfg_60fpsMode], OPTIONS_MENU_VALUE_X, y, FONT_SMALL, fpsColor, ot);
        y += OPTIONS_MENU_ROW_HEIGHT;
    }

    {
        DecalFont_DrawLineOT("FILTER", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
        DecalFont_DrawLineOT(g_cfg_bilinearFiltering ? "SMOOTH" : "PIXEL", OPTIONS_MENU_VALUE_X, y, FONT_SMALL, g_cfg_bilinearFiltering ? TINY_GREEN : WHITE, ot);
        y += OPTIONS_MENU_ROW_HEIGHT;
    }

    int listHeight = OPTIONS_MENU_ROW_HEIGHT * OPTIONS_MENU_VISIBLE_ROWS;
    borders.x = OPTIONS_MENU_CENTER_X - OPTIONS_MENU_PANEL_W / 2 - 6;
    borders.y = OPTIONS_MENU_TITLE_Y - 8;
    borders.w = OPTIONS_MENU_PANEL_W + 12;
    borders.h = listHeight + (OPTIONS_MENU_LIST_TOP_Y - OPTIONS_MENU_TITLE_Y) + 0x30;
    RECTMENU_DrawInnerRect(&borders, 0, ot);

    if (sdata->buttonTapPerPlayer[0] & BTN_UP)
    {
        s_optionsSelectedIndex = (s_optionsSelectedIndex - 1 + OPTION_ROW_COUNT) % OPTION_ROW_COUNT;
        OtherFX_Play(0, 1);
        RECTMENU_ClearInput();
    }
    else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
    {
        s_optionsSelectedIndex = (s_optionsSelectedIndex + 1) % OPTION_ROW_COUNT;
        OtherFX_Play(0, 1);
        RECTMENU_ClearInput();
    }

    if (sdata->AnyPlayerHold & (BTN_LEFT | BTN_RIGHT))
    {
        int delta = 0;
        if (sdata->AnyPlayerHold & BTN_LEFT) delta = -8;
        else if (sdata->AnyPlayerHold & BTN_RIGHT) delta = 8;

        if (s_optionsSelectedIndex == OPTION_ROW_FX)
        {
            int v = (howl_VolumeGet(0) & 0xff) + delta;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            howl_VolumeSet(0, (u8)v);
            RECTMENU_ClearInput();
        }
        else if (s_optionsSelectedIndex == OPTION_ROW_MUSIC)
        {
            int v = (howl_VolumeGet(1) & 0xff) + delta;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            howl_VolumeSet(1, (u8)v);
            RECTMENU_ClearInput();
        }
        else if (s_optionsSelectedIndex == OPTION_ROW_VOICE)
        {
            int v = (howl_VolumeGet(2) & 0xff) + delta;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            howl_VolumeSet(2, (u8)v);
            RECTMENU_ClearInput();
        }
    }

    if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
    {
        OtherFX_Play(1, 1);
        RECTMENU_ClearInput();

        if (s_optionsSelectedIndex == OPTION_ROW_MODE)
        {
            int mode = howl_ModeGet();
            howl_ModeSet(!mode);
        }
        else if (s_optionsSelectedIndex == OPTION_ROW_ASPECT)
        {
            g_cfg_aspectMode = (g_cfg_aspectMode + 1) % 3;
            NativeRenderer_SetAspectMode(g_cfg_aspectMode);
        }
        else if (s_optionsSelectedIndex == OPTION_ROW_FPS)
        {
            g_cfg_60fpsMode = (g_cfg_60fpsMode + 1) % 3;
            NativeRenderer_UpdateSwapIntervalState(g_cfg_60fpsMode != 0 ? 1 : -1);
        }
        else if (s_optionsSelectedIndex == OPTION_ROW_FILTER)
        {
            g_cfg_bilinearFiltering = !g_cfg_bilinearFiltering;
        }
    }

    if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
    {
        OtherFX_Play(2, 1);
        RECTMENU_ClearInput();
        MM_JumpTo_Title_Returning();
    }
}
