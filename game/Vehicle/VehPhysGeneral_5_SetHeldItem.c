#include <common.h>
#ifdef CTR_NATIVE
#include <platform/native_netplay.h>
#endif

enum ItemSet
{
        ITEMSET_Race1 = 0,
        ITEMSET_Race2,
        ITEMSET_Race3,
        ITEMSET_Race4,
        ITEMSET_BattleDefault,
        ITEMSET_BattleCustom,

        // these two swapped,
        // for the sake of array
        ITEMSET_BossRace,
        ITEMSET_CrystalChallenge
};

// all except CrystalChallenge
extern char *charPtr[7];
extern char numWeapons[7];

// Itemset infographic (outdated):
// https://discord.com/channels/330945093416779787/550106151887568906/734368526294450267
// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80060f0c-0x80061488.
void VehPhysGeneral_SetHeldItem(struct Driver *driver)
{
        u32 rng;
        int itemSet;
        char item;
        char bossFails;
        struct GameTracker *gGT;

        gGT = sdata->gGT;

        // 6th Itemset (Battle Mode Custom Itemset)
        itemSet = ITEMSET_BattleCustom;

        // 5th Itemset (Battle Mode Default Itemset, 0x34de)
        if (gGT->battleSetup.enabledWeapons == 0x34de)
                itemSet = ITEMSET_BattleDefault;

        // Not in Battle Mode
        if ((gGT->gameMode1 & BATTLE_MODE) == 0)
        {
                // 7th Itemset (Crystal Challenge)
                itemSet = ITEMSET_CrystalChallenge;

                // Not in Crystal Challenge
                if ((gGT->gameMode1 & CRYSTAL_CHALLENGE) == 0)
                {
                        // Choose Itemset based on number of Drivers
                        int mode = gGT->numPlyrCurrGame + gGT->numBotsNextGame;

                        switch (mode)
                        {
                        // if boss race
                        case 2:

                                // boss race, last place
                                itemSet = ITEMSET_BossRace;

                                // if in first place
                                if (driver->driverRank == 0)
                                {
                                Itemset1:
                                        // 1st Itemset
                                        itemSet = ITEMSET_Race1;
                                }
                                break;

                        // 3P VS race
                        case 3:

                                // if first place
                                if (driver->driverRank == 0)
                                        goto Itemset1;

                                // default (2nd or 3rd place)
                                itemSet = ITEMSET_Race4;

                                // 50/50 chance of an upgrade,
                                // while in 2nd place

                                if (driver->driverRank == 1)
                                {
                                        itemSet = ITEMSET_Race3;
                                        rng = MixRNG_Scramble();
                                        if (rng & 1)
                                                goto Itemset2;
                                }

                                break;
                        case 4:
                                itemSet = driver->driverRank;
                                break;
                        case 5:
                                itemSet = driver->driverRank;
                                // 5th rank is 4th Itemset
                                if (itemSet == 4)
                                        itemSet = 3;
                                break;

                        // 2P Arcade
                        case 6:

                                // careful, dont get confused by names
                                itemSet = driver->driverRank;

                                // if 1st place, ItemSet1
                                if (itemSet == 0)
                                        goto Itemset1;

                                // if 6th place, ItemSet4
                                if (itemSet == 5)
                                        itemSet = ITEMSET_Race4;

                                // 2nd, 3rd place, gets 2nd Itemset
                                // 4th, 5th place, gets 3rd Itemset
                                else
                                        itemSet = (itemSet - 1) / 2 + 1;

                                break;

                        // 1P Arcade
                        case 8:

                                // 0,1 = 0 (itemset1)
                                // 2,3 = 1 (itemset2)
                                // 4,5 = 2 (itemset3)
                                // 6,7 = 3 (itemset4)
                                itemSet = driver->driverRank >> 1;

                                // if in 2nd place, get itemSet2
                                if (itemSet == 1)
                                {
                                Itemset2:
                                        itemSet = ITEMSET_Race2;
                                }
                        }
                }

                // if you have 4th-place itemset on first lap,
                // then override to 3rd place
                if (itemSet == ITEMSET_Race4 && driver->lapIndex == 0)
                        itemSet = ITEMSET_Race3;
        }

#ifdef CTR_NATIVE
        // Chaos RNG: ignore driver rank, give any item regardless of position
        if (g_cfg_chaosRng && ((gGT->gameMode1 & (BATTLE_MODE | CRYSTAL_CHALLENGE)) == 0))
        {
                itemSet = MixRNG_Scramble() % 4;
        }
#endif


        // Decide item for Driver
        rng = (MixRNG_Scramble() >> 0x3) % 0xc8;

        switch (itemSet)
        {
        case ITEMSET_Race1:
        case ITEMSET_Race2:
        case ITEMSET_Race3:
        case ITEMSET_Race4:
        case ITEMSET_BattleDefault:
        case ITEMSET_BossRace:
                driver->heldItemID = charPtr[itemSet][(rng * numWeapons[itemSet]) / 0xc8];
                break;

        // uses int array instead of char,
        // should fix that later, requires 230 rewrite
        case ITEMSET_BattleCustom:
                driver->heldItemID = gGT->battleSetup.RNG_itemSetCustom[(rng * gGT->battleSetup.numWeapons) / 0xc8];
                break;

        case ITEMSET_CrystalChallenge:
                // Item is bomb at Rocky Road, Nitro Court
                // Item is turbo at Skull Rock and Rampage Ruins
                item = 0x1;
                if (gGT->levelID != SKULL_ROCK && gGT->levelID != RAMPAGE_RUINS)
                        goto SetItem;
                driver->heldItemID = 0x0;
                break;

        // "-1st place": Undecided rank
        default:
                rng = MixRNG_Scramble();
                item = (char)rng + -0xc * ((char)(rng / 6 + (rng >> 0x1f) >> 1) - (char)(rng >> 0x1f));
        SetItem:
                driver->heldItemID = item;
        }

        // In Boss race
        if (gGT->gameMode1 & ADVENTURE_BOSS)
        {
                bossFails = sdata->advProgress.timesLostBossRace[gGT->bossID];

                if (bossFails < 0x3)
                {
                        // Replace Clock, Mask,  with 3 Missiles
                        if ((u32)driver->heldItemID - 0x7 < 0x3)
                                driver->heldItemID = 0xb;
                }

                else if (bossFails < 0x4)
                {
                        // Replace Clock, Mask with 3 Missiles
                        if ((u32)driver->heldItemID - 0x7 < 0x2)
                                driver->heldItemID = 0xb;
                }

                else if (bossFails < 0x5 && driver->heldItemID == 0x8)
                {
                        // Replace Clock with 3 Missiles
                        driver->heldItemID = 0xb;
                }

                // Replace 3 Missiles with 1 Missile if racing Komodo Joe
                if (gGT->levelID == DRAGON_MINES && driver->heldItemID == 0xb)
                        driver->heldItemID = 0x2;
        }

        // Replace unused Spring item with Turbo
#ifdef CTR_NATIVE
        if (driver->heldItemID == 0x5)
        {
                // Special Items: keep Spring in races so it becomes usable
                if (!g_cfg_specialItems)
                        driver->heldItemID = 0x0;
        }
        // Special Items: allow battle items (Super Engine, Invisibility, Spring) in races
        else if (g_cfg_specialItems &&
                 ((gGT->gameMode1 & (BATTLE_MODE | CRYSTAL_CHALLENGE)) == 0))
        {
                u32 sp = MixRNG_Scramble();
                if (sp % 5 == 0)
                        driver->heldItemID = 0x5; // Spring
                else if (sp % 5 == 1)
                        driver->heldItemID = 0xC; // Super Engine
                else if (sp % 5 == 2)
                        driver->heldItemID = 0xD; // Invisibility
        }

        // CPU All Items: let the CPU roll Warp Orb / Clock like any other item
        if (g_cfg_cpuAllItems && (driver->driverID >= gGT->numPlyrCurrGame))
        {
                u32 r = MixRNG_Scramble();
                if (r % 7 == 0)
                        driver->heldItemID = 0x9; // Warp Orb
                else if (r % 7 == 1)
                        driver->heldItemID = 0x8; // Clock
        }
#else
        if (driver->heldItemID == 0x5)
                driver->heldItemID = 0x0;
#endif

        // Make sure only 1 Warpball is instanced at once
        if (driver->heldItemID == 0x9)
        {
                // if nobody has warpball, then set flag that somebody has it
                if ((gGT->gameMode1 & WARPBALL_HELD) == 0)
                        gGT->gameMode1 |= WARPBALL_HELD;

                // if somebody has warpball already, then give 3 missiles
                else
                        driver->heldItemID = 0xb;
        }

        if (
            // if you got 3 missiles
            driver->heldItemID == 0xb &&

            // if more than 2 players
            gGT->numPlyrCurrGame > 2 &&

            // if not in battle mode
            ((gGT->gameMode1 & BATTLE_MODE) == 0))
        {
                // if less than 2 drivers have 3 missiles, then increase number of drivers that have it
                if (gGT->numPlayersWith3Missiles < 2)
                        gGT->numPlayersWith3Missiles++;

                // if 2 drivers already have 3 missiles, now you have 1 missile
                else
                        driver->heldItemID = 0x2;
        }

        // Set number of held items
        if ((u32)driver->heldItemID - 0xA < 0x2)
                driver->numHeldItems = 0x3;

#ifdef CTR_NATIVE
        // Item Chaos: give a random 1-9 of the item instead of a single unit
        if (g_cfg_itemChaos)
                driver->numHeldItems = (char)(1 + (MixRNG_Scramble() % 9));
#endif

#ifdef CTR_NATIVE
        // Netplay: broadcast the item we just picked up so remote machines
        // set heldItemID/numHeldItems on our remote driver immediately.
        // Without this, the remote only learns about it via the 5-frame state
        // snapshot, which causes visible desync (wrong item icon on HUD,
        // missed weapon-fire events, etc.).
        if (g_NetplayRacing)
        {
                u8 playerId = (u8)driver->driverID;
                u8 itemId = (u8)driver->heldItemID;
                u8 numItems = (u8)driver->numHeldItems;
                u32 frameNum = (u32)sdata->frameCounter;
                Netplay_BroadcastItemPickup(playerId, itemId, numItems, frameNum);
        }
#endif

        return;
}

char *charPtr[7] = {&data.RNG_itemSetRace1[0],   &data.RNG_itemSetRace2[0],         &data.RNG_itemSetRace3[0],
                    &data.RNG_itemSetRace4[0],   &data.RNG_itemSetBattleDefault[0], (char *)&sdata_static.gameTracker.battleSetup.RNG_itemSetCustom[0],
                    &data.RNG_itemSetBossrace[0]};

char numWeapons[7] = {0x14, 0x34, 0x14, 0x13, 0x14, -1, 0x14};
