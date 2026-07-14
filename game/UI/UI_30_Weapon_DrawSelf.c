#include <common.h>

#ifdef CTR_NATIVE
// Rotated (rot=1) weapon icon with a vertex-color tint, so tweak items
// (Super Engine / Invisibility / Spring) are visually distinct. DecalHUD_DrawWeapon
// forces shadeTex on and ignores color, so this replicates its rotation but tints.
static void DrawWeaponTinted(struct Icon *icon, s16 posX, s16 posY,
                             struct PrimMem *primMem, u_long *ot,
                             char transparency, s16 scale, u32 tint)
{
	if (!icon)
		return;

	POLY_GT4 *p = (POLY_GT4 *)primMem->curr;
	addPolyGT4(ot, p);

	u32 width  = icon->texLayout.u1 - icon->texLayout.u0;
	u32 height = icon->texLayout.v2 - icon->texLayout.v0;
	u32 rightX    = (u16)posX + FP_Mult(width, scale);
	u32 bottomY   = (u16)posY + FP_Mult(height, scale);
	u32 sidewaysX = (u16)posX + FP_Mult(height, scale);
	u32 sidewaysY = (u16)posY + FP_Mult(width, scale);

	setXY4CompilerHack(p, (u16)posX, sidewaysY, (u16)posX, posY, (u16)sidewaysX, sidewaysY, (u16)sidewaysX, posY);
	setIconUV(p, icon);

	setShadeTex(p, false);
	{
		u_char r = (u_char)((tint >> 16) & 0xff);
		u_char g = (u_char)((tint >> 8) & 0xff);
		u_char b = (u_char)(tint & 0xff);
		p->r0 = r; p->g0 = g; p->b0 = b;
		p->r1 = r; p->g1 = g; p->b1 = b;
		p->r2 = r; p->g2 = g; p->b2 = b;
		p->r3 = r; p->g3 = g; p->b3 = b;
	}

	if (transparency)
		setTransparency(p, transparency);

	primMem->curr = p + 1;
}

// Semi-transparent gray quad overlay that pushes an icon toward true black & white
// by averaging the icon's color with mid-gray via the GPU's blend (back-avg-forward).
// Must be drawn AFTER the icon at the same rotated position.
static void DrawGrayOverlay(s16 posX, s16 posY, s16 scale,
                            u32 iconWidth, u32 iconHeight,
                            struct PrimMem *primMem, u_long *ot)
{
	POLY_G4 *p = (POLY_G4 *)primMem->curr;
	addPolyG4(ot, p);

	u32 sidewaysX = (u16)posX + FP_Mult(iconHeight, scale);
	u32 sidewaysY = (u16)posY + FP_Mult(iconWidth, scale);

	setXY4CompilerHack(p, (u16)posX, sidewaysY, (u16)posX, posY, (u16)sidewaysX, sidewaysY, (u16)sidewaysX, posY);
	// Mid-gray vertex color
	{ u_char g = 0x80; p->r0 = g; p->g0 = g; p->b0 = g; p->r1 = g; p->g1 = g; p->b1 = g;
	                   p->r2 = g; p->g2 = g; p->b2 = g; p->r3 = g; p->g3 = g; p->b3 = g; }
	p->code |= 2; // enable semi-transparency

	primMem->curr = p + 1;
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800507e0-0x80050af8.
// Draw weapon and wumpa fruit in HUD
void UI_Weapon_DrawSelf(s16 posX, s16 posY, s16 scale, struct Driver *d)

{
	u32 currChar;
	int itemID;
	int iconID;
	struct GameTracker *gGT;
	s16 posXY[2];

	// beat 7360

	gGT = sdata->gGT;
	itemID = d->heldItemID;

	// If you do have "no weapon icon"
	if (itemID == 0xf)
		return;

	// If you are not shuffling through weapon roulette
	if (itemID != 0x10)
	{
		iconID = itemID + 5;

		// character ID
		currChar = data.characterIDs[d->driverID];

		// if mask item
		if (itemID == 7)
		{
			// Crash, Coco, Pura, Polar, NO Penta
			u32 maskBits = 0xc9;

			// This is a bad guy, change icon to Uka
			if (((maskBits >> currChar) & 1) == 0)
				iconID = 0x32;
		}

		if ((d->numWumpas >= 10) &&

		    // TNT, Potion, Shield
		    (((u32)(itemID - 3) < 2) || (itemID == 6)))
		{
			iconID = itemID + 0x11;
		}

		// make weapon flicker
		if (((d->noItemTimer) != 0) && ((gGT->timer & 1) == 0))
		{
			return;
		}

		// If this weapon has a quantity (3 missiles)
		if (d->numHeldItems != 0)
		{
			// Get the ascii character to represent the quantity
			// of weapon that you have (3 missiles)
			sdata->s_spacebar[0] = d->numHeldItems + '0';

			// Draw the number near the weapon icon to show how many
			DecalFont_DrawLine(sdata->s_spacebar, (int)posX, (int)posY, 2, 4);
		}
	}

	// if roulette shuffle
	else
	{
		itemID = 0;
		posXY[0] = posX;
		posXY[1] = posY;

		// If game is not paused
		if ((gGT->gameMode1 & PAUSE_ALL) == 0)
		{
			// random item
			itemID = rand();

			// If you're not in Battle Mode
			if ((gGT->gameMode1 & BATTLE_MODE) == 0)
			{
				itemID = itemID % 0xc;

				// replace spring with turbo
				if (itemID == 5)
					goto LAB_800508ec;
			}

			// if Battle Mode
			else
			{
				itemID = itemID % 0xe;

				// replace spring
				if (itemID == 5)
				{
				LAB_800508ec:
					itemID = 0;
				}

				// replace clock
				else if (itemID == 8)
				{
					itemID = 1;
				}

				// replace warpball
				else if (itemID == 9)
				{
					itemID = 3;
				}
			}

			// only change icon once per 2 frames,
			// take advantage of unused padding
		}

		// if timer is not finished
		if (d->PickupTimeboxHUD.cooldown != 0)
		{
			UI_Lerp2D_HUD(&posXY[0], d->PickupTimeboxHUD.startX, d->PickupTimeboxHUD.startY, (int)posX, (int)posY, d->PickupTimeboxHUD.cooldown, 5);

			// subtract one from timer
#ifdef CTR_NATIVE
{ static int s_60fpsWeaponTimebox = 0; if (!IS_NATIVE_60FPS || (s_60fpsWeaponTimebox ^= 1)) d->PickupTimeboxHUD.cooldown--; }
#else
d->PickupTimeboxHUD.cooldown--;
#endif
		}

		iconID = itemID + 5;

		posX = posXY[0];
		posY = posXY[1];
	}

#ifdef CTR_NATIVE
	// Fallback icons for battle-only items whose ptrIcons entry is NULL in race.
	// Drawn rotated (rot=1) and tinted so each tweak item is visually distinct.
	if (gGT->ptrIcons[iconID] == 0)
	{
		struct Icon *subIcon = 0;
		if (itemID == 0xC) {
			subIcon = gGT->ptrIcons[0xE]; // Turbo icon
			if (subIcon != 0)
				DrawWeaponTinted(subIcon, posX, posY,
					&gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT,
					TRANS_50_DECAL, scale, 0xffd860); // gold tint
		} else if (itemID == 0xD) {
			subIcon = gGT->ptrIcons[5]; // Warp Orb icon
			if (subIcon != 0) {
				// Two-pass B&W: full-color icon + gray overlay
				u32 iw = subIcon->texLayout.u1 - subIcon->texLayout.u0;
				u32 ih = subIcon->texLayout.v2 - subIcon->texLayout.v0;
				DrawWeaponTinted(subIcon, posX, posY,
					&gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT,
					TRANS_50_DECAL, scale, 0xffffff);
				DrawGrayOverlay(posX, posY, scale, iw, ih,
					&gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT);
			}
		} else if (itemID == 0x5) {
			subIcon = gGT->ptrIcons[0xC]; // Mask icon
			if (subIcon != 0)
				DrawWeaponTinted(subIcon, posX, posY,
					&gGT->backBuffer->primMem, gGT->pushBuffer_UI.ptrOT,
					TRANS_50_DECAL, scale, 0xffd860); // gold tint
		}
		return;
	}
#endif

	DecalHUD_DrawWeapon(
	    // pointer to icon, from array of icon pointers
	    gGT->ptrIcons[iconID],

	    (int)posX, (int)posY,

	    // PrimMem
	    &gGT->backBuffer->primMem,

	    // OTMem
	    gGT->pushBuffer_UI.ptrOT,

	    TRANS_50_DECAL, (int)scale, 1);

	return;
}
