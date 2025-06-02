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
#include "client_main.h"
#include "username_screen.h"
#include "lobbylist_screen.h"
#include "lobby_screen.h"

ClientState client_state = CS_DISCONNECTED;
ClientPlayer self;
LobbyList *lobbylist;
PlayersData *pdata;

int lobby_max_players = -1;

TerminalParams *terminal_specs_check(char *script_path) {
    TerminalParams *tp = malloc(sizeof(TerminalParams));

    if (tp == NULL) {
        return NULL;
    }

    FILE *fp;
    char line[128];

    fp = popen(script_path, "r");

    if (fp == NULL) {
        printf("[ERROR]: Cannot open the file '%s' for checking sixel support!", script_path);
        return NULL;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "hassixel=%i", &tp->has_sixel) == 1) continue;
        if (sscanf(line, "numcolors=%i", &tp->numcolors) == 1) continue;
        if (sscanf(line, "width=%i", &tp->width) == 1) continue;
        if (sscanf(line, "COLUMNS=%i", &tp->columns) == 1) continue;
        if (sscanf(line, "LINES=%i", &tp->lines) == 1) continue;
    }

    pclose(fp);

    return tp;
}

struct sockaddr_in *resolve(char *ip) {
    char domain[100];
    char port[6];
    // %255[^:] scans untill the ':' character
    sscanf(ip, "%99[^:]:%s", domain, port);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;       // Force IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    int status = getaddrinfo(domain, port, &hints, &res);
    if (status != 0) {
        printf("[ERROR]: Can't resolve this address: %s:%s", domain, port);
        return NULL;
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;

    freeaddrinfo(res);

    return addr;
}

int connect_to_server(struct sockaddr_in *addr) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    if (addr == NULL) return 0;

    int ok = connect(sock, (struct sockaddr*)addr, sizeof(struct sockaddr_in));
    if (ok != 0) return 0;
    
    return sock;
}

int main(int argc, char *argv[]) {
    
    // Random number generator initialisation
    srand(time(NULL) ^ getpid());

    if (argc != 2) {
        printf("Usage: ./popsauce-client [IP]:[PORT]\n"
               "Example: ./popsauce-server 192.168.1.0:7677\n");
        return 1;
    }

    // TODO: enable once testing finishes
    // TerminalParams *tp = terminal_specs_check(TERMINAL_SPECS_PATH);
    // if (tp->has_sixel == 0) {
    //     printf( "[ERROR]: Terminal doesn't support the Sixel image format!\n"
    //             "Please use a VT-102 compatible terminal to run this program!\n\n"
    //             "Check this website for more infos: https://www.arewesixelyet.com/\n");
    //     return 1;
    // }

    struct sockaddr_in *addr = resolve(argv[1]);
    if (addr == NULL) {
        printf("[ERROR]: Cannot resolve this address: %s\n", argv[1]);
        return 1;
    }

    self.socket = connect_to_server(addr);
    if (self.socket == 0) {
        printf("[ERROR]: Cannot connect to server!\n");
        return 1;
    }

    int player_id = rand();
    while (player_id == 0) {
        player_id = rand();
    }
    self.player_id = player_id;
    client_state = CS_CONNECTED;

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(1);
    keypad(stdscr, TRUE);

    while (1) {
        switch (client_state) {
        case CS_DISCONNECTED:
            // TODO: cleanup if needed
            endwin();
            exit(EXIT_SUCCESS);
            break;
        case CS_CONNECTED:
            self.username = malloc(sizeof(char)*MAX_USERNAME_LENGTH);
    
            if (self.username == NULL) {
                close(self.socket);
                client_state = CS_DISCONNECTED;
                break;
            }
            
            client_state = handle_set_username(client_state, &self, &lobbylist);

            break;
        case CS_LOBBY_SELECTION:
            int choice;

            client_state = handle_lobbylist_menu(&self, lobbylist, &choice, &pdata, &lobby_max_players);

            if (choice >= 0 && pdata != NULL) {
                self.lobby_id = lobbylist->ids[choice];
            }

            if (choice == -1) {
                pdata = malloc(sizeof(PlayersData));
                pdata->number_of_players = 1;
                pdata->player_names = malloc(sizeof(char)*MAX_USERNAME_LENGTH);
                pdata->players_id = malloc(sizeof(int));
                strncpy(pdata->player_names[0], self.username, MAX_USERNAME_LENGTH);
                pdata->players_id[0] = self.public_id; 
            }

            break;

        case CS_WAITING_ROOM:
            client_state = handle_lobby_screen(client_state, &self, &pdata);
 
        default:
            break;
        }
    }
}
