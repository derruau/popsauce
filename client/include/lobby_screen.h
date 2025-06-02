#ifndef LOBBY_SCREEN_HEADER
#define LOBBY_SCREEN_HEADER

#include <ncurses.h>
#include <menu.h>
#include <form.h>
#include <stdlib.h>
#include <string.h>
#include "pthread.h"
#include "common.h"

#define NO_SOCKET 0

ClientState handle_lobby_screen(ClientState cs, ClientPlayer *p, PlayersData **pdata);

#endif