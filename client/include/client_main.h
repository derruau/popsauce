#ifndef CLIENT_MAIN_HEADER
#define CLIENT_MAIN_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>
#include "common.h"
#include "sixel.h"
#include "username_screen.h"
/*
Can be compiled with: gcc -o sixel_viewer src/client.c ./lib/libsixel.a -lncurses -Iinclude -lm

Pour le client, j'ai besoin de:
    - Un check pour savoir si on supporte libsixel
    - Un ecran pour rentrer son username
    - Un écran de sélection de lobby / creation de lobby
    - Un écran de lobby
    - Un écran de jeu
    - Un écran de réponse
*/

#define TERMINAL_SPECS_PATH "./src/terminal_specs_check"

// Omitting background & foreground for now
typedef struct {
    int has_sixel;
    int numcolors;
    int width;
    int columns;
    int lines;
} TerminalParams;

TerminalParams *terminal_specs_check(char *script_path);

struct sockaddr_in *resolve(char *ip);

int connect_to_server(struct sockaddr_in *addr);

#endif