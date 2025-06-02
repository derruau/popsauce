#ifndef USERNAME_SCREEN_HEADER
#define USERNAME_SCREEN_HEADER

#include <form.h>
#include "common.h"

#define MAX_INPUT_LENGTH 64

ClientState handle_set_username(ClientState cs, ClientPlayer *player, LobbyList **lobbylist);

#endif