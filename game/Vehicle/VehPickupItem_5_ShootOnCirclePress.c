#include <common.h>
#ifdef CTR_NATIVE
#include <platform/native_netplay.h>
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800666e4-0x8006677c.
void VehPickupItem_ShootOnCirclePress(struct Driver *d)
{
        u8 weapon;

        if (d->ChangeState_param2 != 0)
        {
                VehPickState_NewState(d, d->ChangeState_param2, (struct Driver *)d->ChangeState_param3, d->ChangeState_param4);
        }

        // If you want to fire a weapon
        if ((d->actionsFlagSet & 0x8000) == 0)
                return;

        // Remove the request to fire a weapon, since we will fire it now
        d->actionsFlagSet &= ~(0x8000);

        weapon = d->heldItemID;

        // Missiles and Bombs share code,
        // Change Bomb1x, Bomb3x, Missile3x, to Missile1x
        if ((weapon == 1) || (weapon == 10) || (weapon == 11))
        {
                weapon = 2;
        }

#ifdef CTR_NATIVE
        // Netplay: broadcast that the LOCAL player just fired their weapon.
        // Remote machines will receive this and trigger ShootNow on the
        // remote copy of this driver, so the weapon appears for everyone.
        // We only broadcast for the local player — remote drivers fire their
        // weapons via this same hook on their own machine, and we receive
        // THEIR broadcast here.
        if (g_NetplayRacing && d->driverID == Netplay_GetLocalPlayerId())
        {
                Netplay_BroadcastItemUse((u8)d->driverID, weapon, (u32)sdata->frameCounter, 0);
        }
#endif

        VehPickupItem_ShootNow(d, (int)weapon, 0);
}
