/*
TODO: Use Mutex to block access to lobbies and players while an operation is ongoing
TODO: To avoid repeating questions, we need to select unique questions each time
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "common.h"
#include "message_queue.h"

typedef enum {
    PS_CONNECTED_TO_SERVER,
    PS_IN_WAITING_ROOM,
    PS_IN_GAME_NOT_ANSWERED, // Hasn't responded to the question
    PS_IN_GAME_ANSWERED // Has responded to the question
} PlayerState;

typedef enum {
    // When in the lobby's waiting room
    GS_WAITING_ROOM,
    // The 5 seconds period from the moment start_game() is called.
    // When this period is over, the lobby takes the GS_QUESTION state.
    GS_STARTING,
    // When a question has been sent and people can answer it
    GS_QUESTION,
    // When time's up and the question's answer is given
    GS_ANSWER,
} GameState;

typedef struct {
    int player_id;
    // The ID transmitted to players in a lobby to identify one another
    // From 0 to the max number of people in a lobby
    int public_player_id;
    char username[MAX_USERNAME_LENGTH];
    int lobby_id; // Negative value => not in a lobby
    MessageQueue *message_queue;
    PlayerState state;
} Player;

typedef struct {
    int lobby_id;
    int owner_id;
    int private; // Unused, will be implemented when Lobby Rules are implemented
    int max_players;
    int players_in_lobby;
    pthread_t thread;
    char name[MAX_LOBBY_LENGTH];
    GameState state;
    // Pour ces deux, la position dans les array est la même que
    // public_player_id
    int *player_points; // negative points => no players;
    Player **players; // NULL ptr => no player
} Lobby;

Lobby *lobbies[MAX_NUMBER_OF_LOBBIES];
pthread_mutex_t lobbies_mutex[MAX_NUMBER_OF_LOBBIES]; // TODO: implement that
Player *players[MAX_NUMBER_OF_PLAYERS];
pthread_mutex_t players_mutex[MAX_NUMBER_OF_PLAYERS]; // TODO: implement that

// Gets the first available lobby space,
// returns -1 if no space was found
int get_available_lobby_space() {
    for (int i  = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
        if (lobbies[i] == NULL) return i;
    }
    return -1;
}

// Gets the first available player space,
// returns -1 if no space was found
get_available_player_space() {
    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        if (players[i] == NULL) return i;
    }
    return -1;
}

// Returns a player's space from his id
// returns -1 if player doesn't exist
int get_player_space_from_id(int player_id) {
    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        if (players[i]->player_id == player_id) return i;
    }
    return -1;
}

// returns an available public_id for a specific lobby
// returns -1 if none is found
int get_available_public_id(int lobby_id) {
    Lobby *l = lobbies[lobby_id];
    for (int i = 0; i < l->max_players; i++) {
        if (l->players[i] == NULL) return i;
    }
    return -1;
}

// Creates a player and returns the MessageQueue corresponding to this player.
// Use errno for error detection.
MessageQueue *create_player(int player_id, char *username) {
    if (get_player_space_from_id(player_id) != -1) {
        errno = 1;
        return NULL;
    };

    int player_space = get_available_player_space();
    if (player_space == -1) {
        errno = 2;
        return NULL;
    }

    MessageQueue *mq = malloc(sizeof(MessageQueue));
    if (mq == NULL) {
        errno = 3;
        return NULL;
    }
    mq_init(mq);

    Player *p = (Player*)malloc(sizeof(Player));
    if (p == NULL) {
        errno = 3;
        return NULL;
    }

    p->player_id = player_id;
    strncpy(p->username, username, MAX_USERNAME_LENGTH);
    p->state = PS_CONNECTED_TO_SERVER;
    p->lobby_id = -1;
    p->public_player_id = -1;
    p->message_queue = mq;

    players[player_space] = p;

    return mq;
}

// TODO
int delete_player(int player_id) {
    
}

// Returns the lobby's id in lobby_id if successfully created
ResponseCode create_lobby(char *name, int max_players, int owner_id, int *lobby_id) {
    if (max_players <= 0) return EC_CANNOT_CREATE_LOBBY;

    int lobby_space = get_available_lobby_space();

    if (lobby_space == -1) return EC_TOO_MANY_LOBBIES;

    Lobby *lobby = (Lobby*)malloc(sizeof(Lobby));

    if (lobby == NULL) return EC_INTERNAL_ERROR;

    lobby->lobby_id = lobby_space;
    lobby->max_players = max_players;
    lobby->private = 0; // Unused
   
    if (strlen(name) > MAX_LOBBY_LENGTH) return EC_CANNOT_CREATE_LOBBY;
    strncpy(&lobby->name, name, MAX_LOBBY_LENGTH);

    lobby->owner_id = owner_id;
    lobby->state = GS_WAITING_ROOM;
    
    lobby->players = (Player**)malloc(sizeof(Player*)*max_players);
    lobby->player_points = (int*)malloc(sizeof(int)*max_players);

    if ((lobby->players == NULL) || (lobby->player_points == NULL)) {
        free(lobby->players);
        free(lobby->player_points);
        free(lobby);
        return EC_INTERNAL_ERROR;
    }

    int owner_space = get_player_space_from_id(owner_id);
    // Creating a lobby as a ghost player should never happen and is an 
    // internal error
    if (owner_space == -1) {
        free(lobby->players);
        free(lobby->player_points);
        free(lobby);
        return EC_INTERNAL_ERROR;
    }

    players[owner_space]->lobby_id = lobby->lobby_id;
    lobby->players[0] = &(players[owner_space]);

    *lobby_id = lobby->lobby_id;
    return RC_SUCCESS;
}

ResponseCode delete_lobby(int lobby_id) {
    Lobby *l = lobbies[lobby_id];
    if (l->players_in_lobby > 0) return EC_CANNOT_DELETE_LOBBY;

    free(l->players);
    free(l->player_points);
    free(l);

    return RC_SUCCESS;
}

// Returns the PlayerQuit message in player_quit
ResponseCode quit_lobby(int player_id, Message **player_quit) {
    int player_space = get_player_space_from_id(player_id);
    if (player_space == -1) return EC_INTERNAL_ERROR;

    Player *p = players[player_space];
    Lobby *l = lobbies[p->lobby_id];

    if (p->lobby_id < 0) return RC_SUCCESS;

    // Updates the local lobby & player definition
    l->players_in_lobby--;
    l->players[p->public_player_id] = NULL;
    l->player_points[p->public_player_id] = -1;
    p->lobby_id = -1;
    p->state = PS_CONNECTED_TO_SERVER;
    
    // If the person quitting is the lobby's owner, we change
    // the owner to player with the lowest id
    if (player_id == l->owner_id) {
        int changed = 0;
        for (int i = 0; i < l->max_players; i++) {
            if (l->players[i] != NULL) {
                l->owner_id = l->players[i]->player_id;
                changed = 1;
                break;
            }
        }

        // If the owner couldn't be changed, then there are no more players
        // in the lobby and it has to be deleted
        if (!changed) delete_lobby(l->lobby_id);
    }

    // Creates the message to broadcast to everyone else
    PlayerQuit *playerquit = (PlayerQuit*)malloc(sizeof(PlayerQuit));
    if (player_quit == NULL) return EC_INTERNAL_ERROR;

    playerquit->player_id = p->public_player_id;

    Message *m = payload_to_message(PLAYER_QUIT, playerquit, SERVER_UUID);
    if (m == NULL) return EC_INTERNAL_ERROR;

    *player_quit = m;

    return RC_SUCCESS;
}

// Returns the PlayerJoined message in player_joined
ResponseCode join_lobby(int player_id, int lobby_id, Message **player_joined) {
    int player_space = get_player_space_from_id(player_id);

    Player *p = players[player_space];
    Lobby *l = lobbies[lobby_id];

    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return EC_LOBBY_DOESNT_EXIST;
    if (player_space == -1) return EC_INTERNAL_ERROR;

    if (p->lobby_id >= 0) return EC_ALREADY_IN_LOBBY; 

    if (l->players_in_lobby >= l->max_players) return EC_LOBBY_FULL;

    if (l->state != GS_WAITING_ROOM) return EC_GAME_ALREADY_STARTED;

    int public_id = get_available_public_id(lobby_id);
    // We already checked that there is enough room for another player, so this case is
    // an internal error
    if (public_id < 0) return EC_INTERNAL_ERROR;

    // Updates the local lobby & player definition
    p->public_player_id = public_id;
    p->lobby_id = lobby_id;
    p->state = PS_IN_WAITING_ROOM;

    l->players_in_lobby++;
    l->players[public_id] = p;
    l->player_points[public_id] = 0;

    // Creates the message to broadcast to everyone else
    PlayerJoined *pjoined = (PlayerJoined*)malloc(sizeof(PlayerJoined));
    strncpy(pjoined->name, p->username, MAX_USERNAME_LENGTH);

    pjoined->player_id = public_id;

    Message *m = payload_to_message(PLAYER_JOINED, pjoined, SERVER_UUID);
    if (m == NULL) return EC_INTERNAL_ERROR;

    *player_joined = pjoined;
    return RC_SUCCESS;
}

ResponseCode start_game(int player_id, int lobby_id, Message **game_starts) {
    // Strat: on crée un thread pour chaque lobby actif i.e dans l'état GS_QUESTION ou GS_ANSWER.
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return EC_LOBBY_DOESNT_EXIST;

    Lobby *l = lobbies[lobby_id];

    if (l->owner_id != player_id ) return EC_INSUFFICIENT_PERMISSION;
    if (l->state != GS_WAITING_ROOM) return EC_GAME_ALREADY_STARTED;
    if (l->players_in_lobby < 2) return EC_NOT_ENOUGH_PLAYERS;

    pthread_create(&(l->thread), NULL, game_loop, &l);

    *game_starts = (Message*)malloc(sizeof(Message));
    if (*game_starts == NULL) {
        pthread_cancel(&(l->thread));
        pthread_join(l->thread, NULL);
        return EC_INTERNAL_ERROR;
    }

    (*game_starts)->uuid = SERVER_UUID;
    (*game_starts)->type = GAME_STARTS;
    (*game_starts)->payload_size = 0;
    (*game_starts)->payload = NULL;

    return RC_SUCCESS;
}

// J'ai besoin d'envoyer des données entre ce thread (2) et le thread principal (1)
// de (2) vers (1): Je dois envoyer: GAME_STARTS, QUESTION_SENT, ANSWER_SENT et GAME_ENDED, PLAYER_RESPONSE_CHANGED => que des broadcast
// de (1) vers (2): Je dois envoyer: ANSWER_SENT
// Dans tout les cas, on va avoir besoin d'une file de message à traiter
void *game_loop(void *args) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    int lobby_id = *((int*)args);

    ResponseCode rc;
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) {
        rc = EC_LOBBY_DOESNT_EXIST;
        pthread_exit(&rc);
    }

    Lobby *l = lobbies[lobby_id];

    // Send GAME_STARTS
    // Prepare questions
    // while no winner
    //   Send QUESTION_SENT
    //   while (!TIMES_UP_QUESTION && NO_MORE_PROCESSING)
    //      Process ANSWERS
    //      Send PLAYER_RESPONSE_CHANGED
    //   Send ANSWER_SENT
    //  while (!TIMES_UP_INBETWEEN && NO_MORE_PROCESSING)
    //      Process MESSAGES
    // 
    // Send GAME_ENDED
    // return RC_SUCCESS

    // Côté main thread:
    // 
}