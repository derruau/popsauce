#ifndef LOBBYLIST_SCREEN_HEADER
#define LOBBYLIST_SCREEN_HEADER

#include <ncurses.h>
#include <menu.h>
#include <form.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define MAX_LOBBY_NAME 32

#define CREATE_LOBBY_STRING "Create Lobby"

// choice == -1 => Creating lobby
// choice >= 0 => Joining lobby
ClientState handle_lobbylist_menu(ClientPlayer *p, LobbyList *ll, int *choice, PlayersData **pdata, int *max_players);

ClientState show_create_lobby_screen(ClientPlayer *p, int *max_players);

#endif