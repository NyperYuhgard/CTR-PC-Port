#include <common.h>
#include <platform/native_mods.h>

// ============================================================================
// Mods Menu – Full-screen CTR-native style
//
// Visual style matches the game's built-in menus (High Score, Track Select):
//   • Large centered panel with RECTMENU_DrawInnerRect border
//   • Title drawn with FONT_BIG, orange
//   • Each mod is a row: name on the left, status pill on the right
//   • Selected row is highlighted with CTR_Box_DrawClearBox
//     using the same menuRowHighlight_Normal as other menus
//   • ON  = bright green text "ON"
//   • OFF = red text "OFF"
//   • Scroll indicator shown when mods overflow the visible area
//   • Help bar at the bottom with controls
//
// IMPORTANT — PS1 OT Drawing Order:
//   Primitives added to the OT first end up at the TAIL of the linked list
//   and are drawn LAST (on top).  Primitives added last are at the HEAD
//   and drawn FIRST (behind).  Therefore we must draw content (text,
//   highlights) FIRST and the panel border LAST so the border's
//   semi-transparent fill appears BEHIND the text, not on top of it.
//   This matches how CTR's native menus draw: content first, then
//   RECTMENU_DrawSelf / RECTMENU_DrawInnerRect last.
//
// Coordinate space: 512 x 216 (CTR's PS1 OT framebuffer)
//   Screen center: 0x100 (256) horizontal
// ============================================================================

// Maximum number of mods visible at once before scrolling
#define MODS_MENU_VISIBLE_ROWS 8

// ---- Layout constants (PS1 OT coordinate space: 512 x 216) ----
#define MODS_MENU_CENTER_X     0x100  // 256 – true horizontal center
#define MODS_MENU_PANEL_W      0x1C0  // 448 – panel width (good margins in 512px)

// Vertical positions (within 216px height, keep safe from edges)
#define MODS_MENU_TITLE_Y      0x1C   // 28 – Y position of "MODS" title
#define MODS_MENU_LIST_TOP_Y   0x3C   // 60 – Y where first mod row starts
#define MODS_MENU_ROW_HEIGHT   0x10   // 16 px per row (standard CTR)
#define MODS_MENU_HELP_Y       0xB4   // 180 – Y position of help bar (safe margin)

// Derived horizontal positions
#define MODS_MENU_LEFT_X      (MODS_MENU_CENTER_X - MODS_MENU_PANEL_W / 2)  // 256-224 = 32
#define MODS_MENU_RIGHT_X     (MODS_MENU_CENTER_X + MODS_MENU_PANEL_W / 2)  // 256+224 = 480
#define MODS_MENU_NAME_X      (MODS_MENU_LEFT_X + 0x10)     // 32+16 = 48
#define MODS_MENU_STATUS_X    (MODS_MENU_RIGHT_X - 0x10)    // 480-16 = 464
#define MODS_MENU_HIGHLIGHT_W (MODS_MENU_PANEL_W - 0x20)    // 448-32 = 416

static int s_modsMenuSelectedIndex = 0;
static int s_modsMenuScrollOffset  = 0;

void MM_Mods_Init(void)
{
        s_modsMenuSelectedIndex = 0;
        s_modsMenuScrollOffset  = 0;
}

// Clamp scroll so the selected item is always visible
static void ModsMenu_ClampScroll(int modCount)
{
        int maxScroll;

        if (modCount <= MODS_MENU_VISIBLE_ROWS)
        {
                s_modsMenuScrollOffset = 0;
                return;
        }

        maxScroll = modCount - MODS_MENU_VISIBLE_ROWS;

        if (s_modsMenuSelectedIndex < s_modsMenuScrollOffset)
                s_modsMenuScrollOffset = s_modsMenuSelectedIndex;
        else if (s_modsMenuSelectedIndex >= s_modsMenuScrollOffset + MODS_MENU_VISIBLE_ROWS)
                s_modsMenuScrollOffset = s_modsMenuSelectedIndex - MODS_MENU_VISIBLE_ROWS + 1;

        if (s_modsMenuScrollOffset < 0)
                s_modsMenuScrollOffset = 0;
        if (s_modsMenuScrollOffset > maxScroll)
                s_modsMenuScrollOffset = maxScroll;
}

void MM_Mods_MenuProc(struct RectMenu *menu)
{
        struct GameTracker *gGT = sdata->gGT;
        u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
        int modCount;
        int i;
        int y;
        int visibleCount;
        int listHeight;
        RECT highlight;
        RECT borders;

        // ---- Back navigation ----
        if (menu->rowSelected == -1)
        {
                if (menu->ptrPrevBox_InHierarchy != NULL)
                        menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);

                s_modsMenuSelectedIndex = 0;
                s_modsMenuScrollOffset  = 0;
                OtherFX_Play(2, 1);
                MM_JumpTo_Title_Returning();
                return;
        }

        modCount = NativeMods_GetModCount();
        ModsMenu_ClampScroll(modCount);

        // ================================================================
        // DRAW CONTENT FIRST (text, highlights)
        // These are added to the OT first → they end up at the TAIL →
        // drawn LAST → appear ON TOP of the panel border.
        // ================================================================

        // ---- Title ----
        DecalFont_DrawLineOT(
                sdata->lngStrings[0x014],
                MODS_MENU_CENTER_X, MODS_MENU_TITLE_Y,
                FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

        // ---- Selection highlight bar ----
        if (modCount > 0)
        {
                int relIndex = s_modsMenuSelectedIndex - s_modsMenuScrollOffset;

                highlight.x = MODS_MENU_LEFT_X + 4;
                highlight.y = MODS_MENU_LIST_TOP_Y + (relIndex * MODS_MENU_ROW_HEIGHT) - 1;
                highlight.w = MODS_MENU_HIGHLIGHT_W;
                highlight.h = MODS_MENU_ROW_HEIGHT + 1;

                CTR_Box_DrawClearBox(
                        &highlight,
                        &sdata->menuRowHighlight_Normal,
                        TRANS_50_DECAL, ot);
        }

        // ---- Mod rows ----
        y = MODS_MENU_LIST_TOP_Y;

        if (modCount == 0)
        {
                DecalFont_DrawLineOT(
                        "No mods found",
                        MODS_MENU_CENTER_X, y,
                        FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);
                DecalFont_DrawLineOT(
                        "Place mods in the mods/ folder",
                        MODS_MENU_CENTER_X, y + 0x14,
                        FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
        }
        else
        {
                visibleCount = modCount - s_modsMenuScrollOffset;
                if (visibleCount > MODS_MENU_VISIBLE_ROWS)
                        visibleCount = MODS_MENU_VISIBLE_ROWS;

                for (i = 0; i < visibleCount; i++)
                {
                        int absIndex   = s_modsMenuScrollOffset + i;
                        const struct NativeModInfo *mod = NativeMods_GetMod(absIndex);
                        int isSelected = (absIndex == s_modsMenuSelectedIndex);
                        int nameColor;
                        int statusColor;
                        const char *statusText;

                        if (mod == NULL)
                                break;

                        nameColor   = isSelected ? WHITE : ORANGE;
                        statusColor = mod->enabled ? TINY_GREEN : RED;
                        statusText  = mod->enabled ? "ON" : "OFF";

                        DecalFont_DrawLineOT(
                                (char *)mod->name,
                                MODS_MENU_NAME_X, y,
                                FONT_SMALL, nameColor, ot);

                        DecalFont_DrawLineOT(
                                (char *)statusText,
                                MODS_MENU_STATUS_X, y,
                                FONT_SMALL, statusColor, ot);

                        y += MODS_MENU_ROW_HEIGHT;
                }

                // ---- Scroll indicators ----
                if (s_modsMenuScrollOffset > 0)
                {
                        DecalFont_DrawLineOT(
                                "^",
                                MODS_MENU_CENTER_X, MODS_MENU_LIST_TOP_Y - 0x0C,
                                FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
                }

                if (modCount > s_modsMenuScrollOffset + MODS_MENU_VISIBLE_ROWS)
                {
                        DecalFont_DrawLineOT(
                                "v",
                                MODS_MENU_CENTER_X, y + 2,
                                FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
                }

                // ---- Page indicator ----
                if (modCount > MODS_MENU_VISIBLE_ROWS)
                {
                        char pageBuf[32];
                        snprintf(pageBuf, sizeof(pageBuf), "%d / %d",
                                s_modsMenuSelectedIndex + 1, modCount);

                        DecalFont_DrawLineOT(
                                pageBuf,
                                MODS_MENU_CENTER_X, y + 0x10,
                                FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
                }
        }

        // ---- Help bar ----
        DecalFont_DrawLineOT(
                "UP/DN: Select   X: Toggle   TRI: Back",
                MODS_MENU_CENTER_X, MODS_MENU_HELP_Y,
                FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);

        // ================================================================
        // DRAW PANEL BORDER LAST
        // This is added to the OT last → it ends up at the HEAD →
        // drawn FIRST → appears BEHIND all the content above.
        // This is the same pattern CTR's native menus use:
        // content first, then RECTMENU_DrawSelf / DrawInnerRect.
        // ================================================================
        listHeight = MODS_MENU_VISIBLE_ROWS * MODS_MENU_ROW_HEIGHT;

        borders.x = MODS_MENU_CENTER_X - MODS_MENU_PANEL_W / 2 - 6;
        borders.y = MODS_MENU_TITLE_Y - 8;
        borders.w = MODS_MENU_PANEL_W + 12;
        borders.h = listHeight + (MODS_MENU_LIST_TOP_Y - MODS_MENU_TITLE_Y) + 0x30;
        RECTMENU_DrawInnerRect(&borders, 0, ot);

        // ---- Input handling ----
        if (modCount == 0)
        {
                if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
                {
                        OtherFX_Play(2, 1);
                        RECTMENU_ClearInput();
                        MM_JumpTo_Title_Returning();
                }
                return;
        }

        if (sdata->buttonTapPerPlayer[0] & BTN_UP)
        {
                s_modsMenuSelectedIndex =
                        (s_modsMenuSelectedIndex - 1 + modCount) % modCount;
                OtherFX_Play(0, 1);
                RECTMENU_ClearInput();
        }
        else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
        {
                s_modsMenuSelectedIndex =
                        (s_modsMenuSelectedIndex + 1) % modCount;
                OtherFX_Play(0, 1);
                RECTMENU_ClearInput();
        }
        else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
        {
                if (s_modsMenuSelectedIndex < modCount)
                {
                        NativeMods_ToggleMod(s_modsMenuSelectedIndex);
                        OtherFX_Play(1, 1);
                }
                RECTMENU_ClearInput();
        }
        else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
        {
                OtherFX_Play(2, 1);
                RECTMENU_ClearInput();
                MM_JumpTo_Title_Returning();
        }
}
