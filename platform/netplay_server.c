/*===========================================================================
 * CTR-Netplay Dedicated Server
 *
 * Standalone executable. Manages up to NETPLAY_MAX_ROOMS rooms with
 * NETPLAY_MAX_PLAYERS players each. Relays packets between peers.
 * Tracks state transitions (loadAll, raceAll, endAll) per room.
 *
 * Usage: ctr_server [--port PORT] [--max-rooms N]
 *===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <enet/enet.h>

#include <platform/netplay.h>

/* Room state for a player */
typedef struct
{
        ENetPeer *peer;
        char name[NETPLAY_NAME_LEN + 1];
        char characterID;
        char boolLoadSelf;
        char boolRaceSelf;
        char boolEndSelf;
} PeerInfo;

/* Room state */
typedef struct
{
        PeerInfo peerInfos[NETPLAY_MAX_PLAYERS];
        unsigned char clientCount;
        unsigned char levelPlayed;
        unsigned char padding[2];
        char boolRoomLocked;
        char boolLoadAll;
        char boolRaceAll;
        char boolEndAll;
        int endTime;
} RoomInfo;

static RoomInfo s_rooms[NETPLAY_MAX_ROOMS];
static ENetHost *s_server;
static int s_port = NETPLAY_DEFAULT_PORT;

/*──────────────────────────────────────────────
 * Send helpers
 *──────────────────────────────────────────────*/
static void broadcast_to_peers_unreliable(RoomInfo *ri, const void *data, size_t size)
{
        for (int i = 0; i < ri->clientCount; i++)
        {
                if (ri->peerInfos[i].peer != NULL)
                {
                        ENetPacket *p = enet_packet_create(data, size, ENET_PACKET_FLAG_UNSEQUENCED);
                        enet_peer_send(ri->peerInfos[i].peer, NETPLAY_CHAN_UNRELIABLE, p);
                }
        }
}

static void broadcast_to_peers_reliable(RoomInfo *ri, const void *data, size_t size)
{
        for (int i = 0; i < ri->clientCount; i++)
        {
                if (ri->peerInfos[i].peer != NULL)
                {
                        ENetPacket *p = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(ri->peerInfos[i].peer, NETPLAY_CHAN_RELIABLE, p);
                }
        }
}

static void send_to_peer_reliable(ENetPeer *peer, const void *data, size_t size)
{
        ENetPacket *p = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, NETPLAY_CHAN_RELIABLE, p);
}

static void send_to_peer_unreliable(ENetPeer *peer, const void *data, size_t size)
{
        ENetPacket *p = enet_packet_create(data, size, ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(peer, NETPLAY_CHAN_UNRELIABLE, p);
}

/*──────────────────────────────────────────────
 * Send room data to a newly connected peer
 *──────────────────────────────────────────────*/
static void send_room_data(ENetPeer *peer)
{
        struct SG_MessageRooms msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = SG_ROOMS;
        msg.version = NETPLAY_PROTOCOL_VERSION;
        msg.numRooms = NETPLAY_MAX_ROOMS;

        for (int i = 0; i < NETPLAY_MAX_ROOMS; i++)
        {
                uint8_t count = s_rooms[i].clientCount;
                if (s_rooms[i].boolRoomLocked)
                        count += 8;
                msg.roomClients[i] = count;
        }

        send_to_peer_reliable(peer, &msg, sizeof(msg));
}

/*──────────────────────────────────────────────
 * Welcome a new client in a room
 *──────────────────────────────────────────────*/
static void welcome_new_client(RoomInfo *ri, int clientIndex)
{
        struct SG_NewClient welcome;
        welcome.type = SG_NEWCLIENT;
        welcome.clientID = clientIndex;
        welcome.numClients = ri->clientCount;
        send_to_peer_reliable(ri->peerInfos[clientIndex].peer, &welcome, sizeof(welcome));
}

/*──────────────────────────────────────────────
 * Find peer info from ENetPeer
 *──────────────────────────────────────────────*/
static int find_peer(ENetPeer *peer, RoomInfo **outRi, int *outIndex)
{
        for (int r = 0; r < NETPLAY_MAX_ROOMS; r++)
        {
                RoomInfo *ri = &s_rooms[r];
                for (int i = 0; i < ri->clientCount; i++)
                {
                        if (ri->peerInfos[i].peer == peer)
                        {
                                *outRi = ri;
                                *outIndex = i;
                                return r;
                        }
                }
        }
        return -1;
}

/*──────────────────────────────────────────────
 * Remove a player from a room slot
 *──────────────────────────────────────────────*/
static void remove_player(RoomInfo *ri, int index)
{
        /* Shift remaining players down */
        for (int i = index; i < ri->clientCount - 1; i++)
                ri->peerInfos[i] = ri->peerInfos[i + 1];
        ri->clientCount--;
        memset(&ri->peerInfos[ri->clientCount], 0, sizeof(PeerInfo));

        /* If no players left, reset room */
        if (ri->clientCount == 0)
        {
                memset(ri, 0, sizeof(RoomInfo));
        }
        else
        {
                /* Broadcast updated player list */
                for (int i = 0; i < ri->clientCount; i++)
                {
                        struct SG_PlayerName nameMsg;
                        nameMsg.type = SG_NAME;
                        nameMsg.clientID = i;
                        nameMsg.numClients = ri->clientCount;
                        strncpy(nameMsg.name, ri->peerInfos[i].name, NETPLAY_NAME_LEN);
                        broadcast_to_peers_reliable(ri, &nameMsg, sizeof(nameMsg));
                }
        }
}

/*──────────────────────────────────────────────
 * Process connection event
 *──────────────────────────────────────────────*/
static void process_connect_event(ENetEvent *ev)
{
        /* Set a timeout */
        enet_peer_timeout(ev->peer, 64, 60000, 3000);
        send_room_data(ev->peer);
}

/*──────────────────────────────────────────────
 * Process disconnection event
 *──────────────────────────────────────────────*/
static void process_disconnect_event(ENetEvent *ev)
{
        RoomInfo *ri;
        int index;
        if (find_peer(ev->peer, &ri, &index) < 0)
                return;

        printf("[Server] Player %d (%s) disconnected from room\n",
               index, ri->peerInfos[index].name);

        /* Broadcast NULL name to signal disconnection */
        struct SG_PlayerName nameMsg;
        memset(&nameMsg, 0, sizeof(nameMsg));
        nameMsg.type = SG_NAME;
        nameMsg.clientID = index;
        nameMsg.numClients = ri->clientCount - 1;
        broadcast_to_peers_reliable(ri, &nameMsg, sizeof(nameMsg));

        remove_player(ri, index);

        /* Broadcast updated room data to all peers still in lobby */
        /* (The disconnected player likely wasn't in a race yet) */
        for (int i = 0; i < ri->clientCount; i++)
        {
                if (ri->peerInfos[i].peer != NULL)
                        send_room_data(ri->peerInfos[i].peer);
        }
}

/*──────────────────────────────────────────────
 * Process incoming packet
 *──────────────────────────────────────────────*/
static void process_receive_event(ENetEvent *ev)
{
        uint8_t type = packet_get_type(ev->packet->data);
        size_t dataLen = ev->packet->dataLength;

        switch (type)
        {
        case CG_JOINROOM:
        {
                if (dataLen < sizeof(struct CG_JoinRoom))
                        break;
                struct CG_JoinRoom *join = (struct CG_JoinRoom *)ev->packet->data;
                int roomIdx = join->roomIndex;

                /* Validate room */
                if (roomIdx < 0 || roomIdx >= NETPLAY_MAX_ROOMS)
                        break;

                RoomInfo *ri = &s_rooms[roomIdx];

                /* Check if peer already in another room */
                RoomInfo *oldRi = NULL;
                int oldIdx = -1;
                int found = find_peer(ev->peer, &oldRi, &oldIdx);
                if (found >= 0 && oldRi != ri)
                {
                        /* Remove from old room first */
                        remove_player(oldRi, oldIdx);
                }

                /* Check room capacity */
                if (ri->clientCount >= NETPLAY_MAX_PLAYERS)
                        break;

                /* Check for duplicate peer */
                for (int i = 0; i < ri->clientCount; i++)
                {
                        if (ri->peerInfos[i].peer == ev->peer)
                                return; /* already in this room */
                }

                /* Add peer to room */
                int slot = ri->clientCount++;
                ri->peerInfos[slot].peer = ev->peer;
                memset(ri->peerInfos[slot].name, 0, sizeof(ri->peerInfos[slot].name));
                ri->peerInfos[slot].characterID = -1;
                ri->peerInfos[slot].boolLoadSelf = 0;
                ri->peerInfos[slot].boolRaceSelf = 0;
                ri->peerInfos[slot].boolEndSelf = 0;

                printf("[Server] Peer joined room %d, slot %d (now %d clients)\n",
                       roomIdx, slot, ri->clientCount);

                /* Send welcome */
                welcome_new_client(ri, slot);

                /* Send existing player names to new peer */
                for (int i = 0; i < ri->clientCount - 1; i++)
                {
                        struct SG_PlayerName nameMsg;
                        nameMsg.type = SG_NAME;
                        nameMsg.clientID = i;
                        nameMsg.numClients = ri->clientCount;
                        strncpy(nameMsg.name, ri->peerInfos[i].name, NETPLAY_NAME_LEN);
                        send_to_peer_reliable(ri->peerInfos[slot].peer, &nameMsg, sizeof(nameMsg));
                }

                /* Broadcast new peer info (empty name triggers "Player X joined") */
                struct SG_PlayerName newMsg;
                newMsg.type = SG_NAME;
                newMsg.clientID = slot;
                newMsg.numClients = ri->clientCount;
                memset(newMsg.name, 0, sizeof(newMsg.name));
                broadcast_to_peers_reliable(ri, &newMsg, sizeof(newMsg));

                /* Send updated room data to all */
                for (int i = 0; i < ri->clientCount; i++)
                {
                        if (ri->peerInfos[i].peer != NULL)
                                send_room_data(ri->peerInfos[i].peer);
                }
                break;
        }

        case CG_NAME:
        {
                RoomInfo *ri;
                int index;
                int room = find_peer(ev->peer, &ri, &index);
                if (room < 0)
                        break;

                if (dataLen < sizeof(struct SG_PlayerName))
                        break;
                struct SG_PlayerName *nameMsg = (struct SG_PlayerName *)ev->packet->data;
                strncpy(ri->peerInfos[index].name, nameMsg->name, NETPLAY_NAME_LEN);
                ri->peerInfos[index].name[NETPLAY_NAME_LEN] = 0;

                printf("[Server] Room %d: Player %d set name to '%s'\n",
                       room, index, ri->peerInfos[index].name);

                /* Broadcast to everyone else */
                struct SG_PlayerName broadcast;
                broadcast.type = SG_NAME;
                broadcast.clientID = index;
                broadcast.numClients = ri->clientCount;
                strncpy(broadcast.name, ri->peerInfos[index].name, NETPLAY_NAME_LEN);
                broadcast_to_peers_reliable(ri, &broadcast, sizeof(broadcast));
                break;
        }

        case CG_TRACK:
        {
                RoomInfo *ri;
                int index;
                int room = find_peer(ev->peer, &ri, &index);
                if (room < 0 || index != 0)
                        break; /* only host/slot-0 can set track */

                if (dataLen < sizeof(struct SG_TrackSelect))
                        break;
                struct SG_TrackSelect *track = (struct SG_TrackSelect *)ev->packet->data;

                ri->boolRoomLocked = 1;
                ri->levelPlayed = track->trackID;

                printf("[Server] Room %d: Track set to %d, laps %d\n",
                       room, track->trackID, track->lapCount);

                /* Broadcast to all */
                broadcast_to_peers_reliable(ri, track, sizeof(*track));
                break;
        }

        case CG_CHARACTER:
        {
                RoomInfo *ri;
                int index;
                int room = find_peer(ev->peer, &ri, &index);
                if (room < 0)
                        break;

                if (dataLen < sizeof(struct SG_CharacterSelect))
                        break;
                struct SG_CharacterSelect *ch = (struct SG_CharacterSelect *)ev->packet->data;

                ri->peerInfos[index].characterID = ch->charID;

                /* Broadcast to all */
                struct SG_CharacterSelect broadcast;
                broadcast.type = SG_CHARACTER;
                broadcast.clientID = index;
                broadcast.lockedIn = ch->lockedIn;
                broadcast.charID = ch->charID;
                broadcast.engineID = ch->engineID;
                broadcast_to_peers_reliable(ri, &broadcast, sizeof(broadcast));
                break;
        }

        case CG_STARTRACE:
        {
                RoomInfo *ri;
                int index;
                int room = find_peer(ev->peer, &ri, &index);
                if (room < 0)
                        break;

                ri->peerInfos[index].boolLoadSelf = 1;
                printf("[Server] Room %d: Player %d started loading\n", room, index);

                /* When host signals start, immediately tell all players to load.
                 * (No need to wait for all guests — host-driven flow.) */
                if (index == 0)
                {
                        if (!ri->boolLoadAll)
                        {
                                ri->boolLoadAll = 1;
                                uint8_t msg[1];
                                packet_set_header(msg, SG_STARTLOADING);
                                broadcast_to_peers_reliable(ri, msg, 1);
                        }
                }
                break;
        }

        case CG_LOADINGDONE:
        {
                RoomInfo *ri;
                int index;
                int room = find_peer(ev->peer, &ri, &index);
                if (room < 0)
                        break;

                ri->peerInfos[index].boolRaceSelf = 1;
                printf("[Server] Room %d: Player %d finished loading\n", room, index);
                break;
        }

        case CG_RACEDATA:
        {
                RoomInfo *ri;
                int index;
                int room = find_peer(ev->peer, &ri, &index);
                if (room < 0)
                        break;

                /* Relay to all OTHER peers in the room */
                struct EverythingKart *ek = (struct EverythingKart *)ev->packet->data;
                struct EverythingKart relay;
                memcpy(&relay, ek, sizeof(relay));

                /* Set the client ID in the relayed packet */
                relay.header[1] = (relay.header[1] & 0x07) | ((index & 7) << 3);

                for (int i = 0; i < ri->clientCount; i++)
                {
                        if (ri->peerInfos[i].peer != NULL && i != index)
                        {
                                send_to_peer_unreliable(ri->peerInfos[i].peer, &relay, sizeof(relay));
                        }
                }
                break;
        }

        case CG_WEAPON:
        {
                RoomInfo *ri;
                int index;
                int room = find_peer(ev->peer, &ri, &index);
                if (room < 0)
                        break;

                if (dataLen < sizeof(struct SG_WeaponUse))
                        break;

                /* Relay to all others */
                struct SG_WeaponUse *wp = (struct SG_WeaponUse *)ev->packet->data;
                struct SG_WeaponUse relay;
                relay.type = SG_WEAPON;
                relay.clientID = index;
                relay.juiced = wp->juiced;
                relay.weaponID = wp->weaponID;
                broadcast_to_peers_reliable(ri, &relay, sizeof(relay));
                break;
        }

        case CG_ENDRACE:
        {
                RoomInfo *ri;
                int index;
                int room = find_peer(ev->peer, &ri, &index);
                if (room < 0)
                        break;

                ri->peerInfos[index].boolEndSelf = 1;

                if (dataLen < sizeof(struct SG_EndRace))
                        break;
                struct SG_EndRace *er = (struct SG_EndRace *)ev->packet->data;
                struct SG_EndRace relay;
                relay.type = SG_ENDRACE;
                relay.clientID = index;
                relay.padding = 0;
                relay.courseTime = er->courseTime;
                relay.bestLapTime = er->bestLapTime;
                relay.posX = er->posX;
                relay.posZ = er->posZ;

                printf("[Server] Room %d: Player %d finished race\n", room, index);

                /* Broadcast to all */
                broadcast_to_peers_reliable(ri, &relay, sizeof(relay));
                break;
        }

        case CG_PING:
        {
                send_to_peer_reliable(ev->peer, ev->packet->data, dataLen);
                break;
        }

        default:
                break;
        }
}

/*──────────────────────────────────────────────
 * Process all pending ENet events
 *──────────────────────────────────────────────*/
static void process_new_messages(void)
{
        ENetEvent ev;
        while (enet_host_service(s_server, &ev, 0) > 0)
        {
                switch (ev.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                        printf("[Server] Connection from %x:%u\n",
                               ev.peer->address.host, ev.peer->address.port);
                        process_connect_event(&ev);
                        break;
                case ENET_EVENT_TYPE_DISCONNECT:
                        printf("[Server] Disconnection from %x:%u\n",
                               ev.peer->address.host, ev.peer->address.port);
                        process_disconnect_event(&ev);
                        break;
                case ENET_EVENT_TYPE_RECEIVE:
                        process_receive_event(&ev);
                        enet_packet_destroy(ev.packet);
                        break;
                default:
                        break;
                }
        }
}

/*──────────────────────────────────────────────
 * Server main tick
 *──────────────────────────────────────────────*/
static void server_state_tick(void)
{
        process_new_messages();

        for (int r = 0; r < NETPLAY_MAX_ROOMS; r++)
        {
                RoomInfo *ri = &s_rooms[r];
                if (ri->clientCount < 2)
                        continue;

                /* Check loadAll */
                if (!ri->boolLoadAll)
                {
                        int allLoaded = 1;
                        for (int i = 0; i < ri->clientCount; i++)
                        {
                                if (!ri->peerInfos[i].boolLoadSelf)
                                {
                                        allLoaded = 0;
                                        break;
                                }
                        }
                        if (allLoaded)
                        {
                                ri->boolLoadAll = 1;
                                printf("[Server] Room %d: All players loaded, sending SG_STARTLOADING\n", r);

                                uint8_t msg[1];
                                packet_set_header(msg, SG_STARTLOADING);
                                broadcast_to_peers_reliable(ri, msg, 1);
                        }
                }

                /* Check raceAll */
                if (ri->boolLoadAll && !ri->boolRaceAll)
                {
                        int allRacing = 1;
                        for (int i = 0; i < ri->clientCount; i++)
                        {
                                if (!ri->peerInfos[i].boolRaceSelf)
                                {
                                        allRacing = 0;
                                        break;
                                }
                        }
                        if (allRacing)
                        {
                                ri->boolRaceAll = 1;
                                printf("[Server] Room %d: All players racing, sending SG_STARTRACE\n", r);

                                uint8_t msg[1];
                                packet_set_header(msg, SG_STARTRACE);
                                broadcast_to_peers_reliable(ri, msg, 1);
                        }
                }

                /* Check endAll */
                if (ri->boolRaceAll && !ri->boolEndAll)
                {
                        int allEnded = 1;
                        for (int i = 0; i < ri->clientCount; i++)
                        {
                                if (!ri->peerInfos[i].boolEndSelf)
                                {
                                        allEnded = 0;
                                        break;
                                }
                        }
                        if (allEnded)
                        {
                                ri->boolEndAll = 1;
                                ri->endTime = 0;
                                printf("[Server] Room %d: All players finished\n", r);
                        }
                }

                /* Reset room 6 seconds after all finished */
                if (ri->boolEndAll)
                {
                        ri->endTime++;
                        if (ri->endTime > 360)  /* ~6 seconds at 60fps tick */
                        {
                                printf("[Server] Room %d: Resetting for next race\n", r);

                                /* Reset room state but keep players */
                                for (int i = 0; i < ri->clientCount; i++)
                                {
                                        ri->peerInfos[i].boolLoadSelf = 0;
                                        ri->peerInfos[i].boolRaceSelf = 0;
                                        ri->peerInfos[i].boolEndSelf = 0;
                                }
                                ri->boolRoomLocked = 0;
                                ri->boolLoadAll = 0;
                                ri->boolRaceAll = 0;
                                ri->boolEndAll = 0;
                                ri->endTime = 0;
                                ri->levelPlayed = 0;

                                /* Re-send welcome to all */
                                for (int i = 0; i < ri->clientCount; i++)
                                {
                                        if (ri->peerInfos[i].peer != NULL)
                                        {
                                                welcome_new_client(ri, i);
                                                send_room_data(ri->peerInfos[i].peer);
                                        }
                                }
                        }
                }
        }

        enet_host_flush(s_server);
}

/*──────────────────────────────────────────────
 * Entry point
 *──────────────────────────────────────────────*/
int main(int argc, char **argv)
{
        printf("CTR-Netplay Server v%s\n", "0.1.0");
        printf("Protocol version: %d\n", NETPLAY_PROTOCOL_VERSION);

        /* Parse args */
        for (int i = 1; i < argc; i++)
        {
                if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0)
                {
                        if (i + 1 < argc)
                                s_port = atoi(argv[i + 1]);
                }
        }

        printf("Starting on port %d...\n", s_port);

        /* Initialize ENet */
        if (enet_initialize() != 0)
        {
                fprintf(stderr, "Failed to initialize ENet\n");
                return 1;
        }
        atexit(enet_deinitialize);

        /* Create server host */
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = s_port;

        s_server = enet_host_create(&address, NETPLAY_MAX_PLAYERS * NETPLAY_MAX_ROOMS,
                                     2, 0, 0);
        if (s_server == NULL)
        {
                fprintf(stderr, "Failed to create ENet server host on port %d\n", s_port);
                return 1;
        }

        printf("Server ready. Listening on port %d...\n", s_port);
        printf("Max rooms: %d, Max players per room: %d\n",
               NETPLAY_MAX_ROOMS, NETPLAY_MAX_PLAYERS);

        /* Main loop */
        memset(s_rooms, 0, sizeof(s_rooms));

        while (1)
        {
                server_state_tick();

                /* Sleep 1ms to avoid busy-wait */
                enet_host_service(s_server, NULL, 1);
        }

        enet_host_destroy(s_server);
        return 0;
}
