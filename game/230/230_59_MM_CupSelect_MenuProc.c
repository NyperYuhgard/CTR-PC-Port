// NOTE(aalhendi): ASM-verified NTSC-U 926 overlay 230 0x800b0eec-0x800b164c.
void MM_CupSelect_MenuProc(struct RectMenu *menu)
{
	char i;
	u8 cupIndex;
	u8 starIndex;
	u8 trackIndex;
	s16 elapsedFrames;
	u32 txtColor;
	u32 *starColor;
	int startX;
	int startY;
	struct GameTracker *gGT = sdata->gGT;
	RECT cupBox;

	if (menu->unk1e == 0)
	{
		D230.cupSel_postTransition_boolStart = (menu->rowSelected != -1);
		D230.cupSel_transitionState = 2;
		D230.menuCupSelect.state &= ~(EXECUTE_FUNCPTR);
		D230.menuCupSelect.state |= DISABLE_INPUT_ALLOW_FUNCPTRS;
		return;
	}

	elapsedFrames = D230.cupSel_transitionFrames;

	// if not stationary
	if (D230.cupSel_transitionState != 1)
	{
		// if transitioning in
		if (D230.cupSel_transitionState == 0)
		{
			MM_TransitionInOut(&D230.transitionMeta_cupSel[0], elapsedFrames, 8);

			// if no more frames
			if (elapsedFrames == 0)
			{
				// menu is now in focus
				D230.cupSel_transitionState = 1;
				D230.menuCupSelect.state &= ~(DISABLE_INPUT_ALLOW_FUNCPTRS);
				D230.menuCupSelect.state |= EXECUTE_FUNCPTR;
			}

			else
			{
 #ifdef CTR_NATIVE
				{ static int s_60fpsCupSelToggle = 0; if (!IS_NATIVE_60FPS || (s_60fpsCupSelToggle ^= 1)) elapsedFrames--; }
 #else
				elapsedFrames--;
 #endif
			}
		}
		// if transitioning out
		else if (D230.cupSel_transitionState == 2)
		{
			MM_TransitionInOut(&D230.transitionMeta_cupSel[0], elapsedFrames, 8);

			// increase frame count
			elapsedFrames++;

			// if more than 12 frames pass
			if (12 < elapsedFrames)
			{
				// if cup selected
				if (D230.cupSel_postTransition_boolStart != 0)
				{
					// set cupID to the cup selected
					gGT->cup.cupID = menu->rowSelected;

					// set track index to zero, to go to first track
					gGT->cup.trackIndex = 0;

					// loop through 8 drivers
					for (i = 0; i < 8; i++)
					{
						// set all points for all 8 drivers to zero
						gGT->cup.points[i] = 0;
					}

					// passthrough Menu for the function
					sdata->ptrDesiredMenu = &data.menuQueueLoadTrack;

					// set current level (Oxide Cup uses hardcoded tracks)
					if (gGT->cup.cupID < 4)
						gGT->currLEV = data.ArcadeCups[gGT->cup.cupID].CupTrack[gGT->cup.trackIndex].trackID;
					else
					{
						static const int oxideTracks[4] = {13, 17, 16, 8};
						gGT->currLEV = oxideTracks[gGT->cup.trackIndex];
					}
					return;
				}

				// return to character selection
				sdata->ptrDesiredMenu = &D230.menuCharacterSelect;

				MM_Characters_RestoreIDs();
				return;
			}
		}
	}

	D230.cupSel_transitionFrames = elapsedFrames;

DecalFont_DrawLine(sdata->lngStrings[LNG_SELECT_CUP_RACE], (D230.transitionMeta_cupSel[4].currX + 0x100), (D230.transitionMeta_cupSel[4].currY + 0x10), 1,
                   0xffff8000);

// Loop through all cups (4 standard cups; Oxide Cup at index 4 if unlocked)
int numCups = 4;
if ((sdata->gameProgress.unlocks[1] & (1 << 5)) != 0)
    numCups = 5;

// Scroll handling: L1/R1 to scroll pages (4 cups per page)
int cupsPerPage = 4;
int maxScroll = numCups > cupsPerPage ? numCups - cupsPerPage : 0;
if (numCups > cupsPerPage)
{
    struct GamepadBuffer *gpad = &sdata->gGamepads->gamepad[0];
    int tap = gpad->buttonsTapped;
    
    // L2 (scroll left/up) / R2 (scroll right/down)
    if (tap & BTN_L2)
    {
        if (D230.cupSel_scrollOffset > 0)
        {
            D230.cupSel_scrollOffset--;
            OtherFX_Play(0x66, 1);
        }
    }
    else if (tap & BTN_R2)
    {
        if (D230.cupSel_scrollOffset < maxScroll)
        {
            D230.cupSel_scrollOffset++;
            OtherFX_Play(0x66, 1);
        }
    }
}

// Draw only visible cups (4 per page, starting from scrollOffset)
int drawStart = D230.cupSel_scrollOffset;
int drawEnd = drawStart + 4;
if (drawEnd > numCups)
    drawEnd = numCups;

for (cupIndex = drawStart; cupIndex < drawEnd; cupIndex++)
	{
		// Visible index within the current page (0-3)
		int visIndex = cupIndex - drawStart;
		
		// Use solid color
		txtColor = 0xffff8000;

		// If this cup is the one you selected
		if (cupIndex == menu->rowSelected)
		{
			// Make text flash
			if ((sdata->frameCounter & 2) != 0)
				txtColor |= 4;
		}

		startX = (s16)D230.transitionMeta_cupSel[visIndex].currX + (visIndex & 1) * 200;
		startY = (s16)D230.transitionMeta_cupSel[visIndex].currY + (visIndex >> 1) * 0x54;

		// draw the name of the cup
		if (cupIndex < 4)
		{
			DecalFont_DrawLine(sdata->lngStrings[data.ArcadeCups[cupIndex].lngIndex_CupName], startX + 0xa2, startY + 0x44, 3, txtColor);
		}
		else
		{
			// Oxide Cup (index 4) - hardcoded name
			DecalFont_DrawLine("OXIDE CUP", startX + 0xa2, startY + 0x44, 3, txtColor);
		}

		startX = startX + 0x4e;
		startY = startY + 0x29;

		// loop through 3 stars to draw
		for (starIndex = 0; starIndex < 3; starIndex++)
		{
			int starUnlock;
			if (cupIndex < 4)
				starUnlock = D230.cupSel_StarUnlockFlag[starIndex] + cupIndex;
			else
				starUnlock = 38 + starIndex; // unlocks[1] bits 6-8 for Oxide Cup stars
			if (CHECK_ADV_BIT(sdata->gameProgress.unlocks, starUnlock) != 0)
			{
				// array of colorIDs
				// 0x11: driver_C (tropy) (blue)
				// 0x0E: driver_9 (papu) (yellow)
				// 0x16: silver

				starColor = data.ptrColor[D230.cupSel_StarColorIndex[starIndex]];

				struct Icon **iconPtrArray = ICONGROUP_GETICONS(gGT->iconGroup[5]);

				DecalHUD_DrawPolyGT4(iconPtrArray[0x37], (startX + (visIndex & 1) * 0xCA - 0x16), (startY + ((starIndex * 0x10) + 0x10)),
				                     &gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT, starColor[0], starColor[1], starColor[2], starColor[3], 0, FP(1.0));
			}
		}

		// loop through all four track icons in one cup
		for (trackIndex = 0; trackIndex < 4; trackIndex++)
		{
			int posX = (startX + (trackIndex & 1) * 0x54);
			int posY = (startY + (trackIndex >> 1) * 0x23);

			int trackIconID;
			if (cupIndex < 4)
			{
				trackIconID = data.ArcadeCups[cupIndex].CupTrack[trackIndex].iconID;
			}
			else
			{
				// Oxide Cup tracks: Oxide Station, Turbo Track, Slide Coliseum, Sewer Speedway
				static const int oxideCupIcons[4] = {0x58, 0x56, 0x55, 0x57};
				trackIconID = oxideCupIcons[trackIndex];
			}

			// Draw Icon of each track
			RECTMENU_DrawPolyGT4(gGT->ptrIcons[trackIconID], posX, posY, &gGT->backBuffer->primMem,
			                     gGT->pushBuffer_UI.ptrOT, D230.cupSel_Color, D230.cupSel_Color, D230.cupSel_Color, D230.cupSel_Color, 0, FP(0.5));
		}

		if (cupIndex == menu->rowSelected)
		{
			// highlight box
			cupBox.x = startX - 3;
			cupBox.y = startY - 2;
			cupBox.w = 174;
			cupBox.h = 74;

			CTR_Box_DrawClearBox(&cupBox, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, gGT->backBuffer->otMem.startPlusFour);
		}

		// background box
		cupBox.x = startX - 6;
		cupBox.y = startY - 4;
		cupBox.w = 180;
		cupBox.h = 78;

		RECTMENU_DrawInnerRect(&cupBox, 0, gGT->backBuffer->otMem.startPlusFour);
	}
}