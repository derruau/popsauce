/*
TODO: Use Mutex to block access to lobbies and players while an operation is ongoing
TODO: To avoid repeating questions, we need to select unique questions each time

*/

#include "game_logic.h"

Lobby *lobbies[MAX_NUMBER_OF_LOBBIES];
pthread_mutex_t lobbies_mutex[MAX_NUMBER_OF_LOBBIES];
Player *players[MAX_NUMBER_OF_PLAYERS];
pthread_mutex_t players_mutex[MAX_NUMBER_OF_PLAYERS];

// Initializes the mutexes used for lobbies and players
void init_game_mutexes() {
    for (int i = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
        pthread_mutex_init(&lobbies_mutex[i], NULL);
    }

    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        pthread_mutex_init(&players_mutex[i], NULL);
    }
}

// Locks all lobbies mutexes
void lock_all_lobbies() {
    for (int i = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
        pthread_mutex_lock(&lobbies_mutex[i]);
    }
}

// Locks all players mutexes
void lock_all_players() {
    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        pthread_mutex_lock(&players_mutex[i]);
    }
}

// Unlocks all lobbies mutexes
void unlock_all_lobbies() {
    for (int i = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
        pthread_mutex_unlock(&lobbies_mutex[i]);
    }
}

// Unlocks all players mutexes
void unlock_all_players() {
    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        pthread_mutex_unlock(&players_mutex[i]);
    }
}


// Gets the first available lobby space,
// returns -1 if no space was found
int get_available_lobby_space() {
    lock_all_lobbies();
    for (int i  = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
        if (lobbies[i] == NULL) {
            unlock_all_lobbies();
            return i;
        }
    }
    unlock_all_lobbies();
    return -1;
}

// Gets the first available player space,
// returns -1 if no space was found
int get_available_player_space() {
    lock_all_players();
    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        if (players[i] == NULL) {
            unlock_all_players();
            return i;
        }
    }
    unlock_all_players();
    return -1;
}

// Returns a player's space from his id
// returns -1 if player doesn't exist
int get_player_space_from_id(int player_id) {
    lock_all_players();
    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        if (players[i] == NULL) continue;
        if (players[i]->player_id == player_id) {
            unlock_all_players();
            return i;
        }
    }
    unlock_all_players();
    return -1;
}

// Returns a player's id from his socket
// returns -1 if player doesn't exist
int get_player_id_from_socket(int socket) {
    lock_all_players();
    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        if (players[i] == NULL) continue;
        if (players[i]->socket == socket) {
            unlock_all_players();
            return players[i]->player_id;
        }
    }
    unlock_all_players();
    return -1;   
}

// returns an available public_id for a specific lobby
// returns -1 if none is found
int get_available_public_id(int lobby_id) {
    pthread_mutex_lock(&lobbies_mutex[lobby_id]);

    Lobby *l = lobbies[lobby_id];
    for (int i = 0; i < l->max_players; i++) {
        if (l->players[i] == NULL) {
            pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
            return i;
        }
    }

    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
    return -1;
}

// Returns the lobby_id of the player.
// Retuns a negative value when either the player isn't in a lobby or 
// the player doesn't exit
int get_lobby_of_player(int player_id) {
    int player_space = get_player_space_from_id(player_id);
    if (player_space == -1) return -1;

    pthread_mutex_lock(&players_mutex[player_space]);
    int ret = players[player_space]->lobby_id;
    pthread_mutex_unlock(&players_mutex[player_space]);

    return ret;
}

// Returns a list of players from the lobby.
// Also returns the number of players in the lobby in players_in_lobby
// Returns NULL when in invalid lobby id is specified
Player **get_players_from_lobby(int lobby_id, int *players_in_lobby) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return NULL;

    *players_in_lobby = lobbies[lobby_id]->players_in_lobby;
    return lobbies[lobby_id]->players;
}


// Returns the MessageQueue corresponding to the lobby and queue type.
// Returns NULL when the lobby doesn't exist or the queue type is invalid.
MessageQueue *get_message_queue_of_lobby(int lobby_id, LobbyMessageQueue queue) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return NULL;
    if (lobbies[lobby_id] == NULL) return NULL;

    pthread_mutex_lock(&lobbies_mutex[lobby_id]);

    MessageQueue *ret;
    switch (queue) {
    case RECEIVE_QUEUE:
        ret = lobbies[lobby_id]->receive_queue;
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return ret;
    case SEND_QUEUE:
        ret = lobbies[lobby_id]->send_queue;
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return ret;
    default:
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return NULL;
    }
}

// Sets the lobby's send thread to the given thread.
ResponseCode set_lobby_send_thread(int lobby_id, pthread_t *thread) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return EC_LOBBY_DOESNT_EXIST;
    if (lobbies[lobby_id] == NULL) return EC_LOBBY_DOESNT_EXIST;

    pthread_mutex_lock(&lobbies_mutex[lobby_id]);
    lobbies[lobby_id]->send_thread = thread;
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);

    return RC_SUCCESS;
}

// Enqueues a message in the lobby's message queue.
ResponseCode lobby_enqueue(int lobby_id, LobbyMessageQueue kind, Message *m, int socket) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return EC_LOBBY_DOESNT_EXIST;
    if (lobbies[lobby_id] == NULL) return EC_LOBBY_DOESNT_EXIST;

    Lobby *l = lobbies[lobby_id];
    
    switch (kind) {
    case RECEIVE_QUEUE:
        mq_enqueue(l->receive_queue, m, socket);
        break;
    case SEND_QUEUE:
        mq_enqueue(l->send_queue, m, socket);
        break;
    default:
        return EC_INTERNAL_ERROR;
    }

    return RC_SUCCESS;
}

// Dequeues a message from the lobby's message queue.
// Returns NULL when the lobby doesn't exist or the queue type is invalid.
MessageQueueItem *lobby_dequeue(int lobby_id, LobbyMessageQueue kind) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return NULL;
    if (lobbies[lobby_id] == NULL) return NULL;

    Lobby *l = lobbies[lobby_id];

    MessageQueueItem *mqi;
    switch (kind) {
    case RECEIVE_QUEUE:
        mqi = mq_dequeue(l->receive_queue);
        return mqi;
    case SEND_QUEUE:
        mqi = mq_dequeue(l->send_queue);
        return mqi;
    default:
        return NULL;
    }
}

// Returns 1 when message queue is empty and 0 when it's not.
// Returns -1 when lobby doesn't exist
int lobby_mq_is_empty(int lobby_id, LobbyMessageQueue kind) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return -1;
    if (lobbies[lobby_id] == NULL) return -1;

    Lobby *l = lobbies[lobby_id];

    switch (kind) {
        case RECEIVE_QUEUE:
            return mq_is_empty(l->receive_queue);
        case SEND_QUEUE:
            return mq_is_empty(l->send_queue);
        default:
            return -1;
        }
}

// Returns 1 when message queue is full and 0 when it's not.
// Returns -1 when lobby doesn't exist
int lobby_mq_is_full(int lobby_id, LobbyMessageQueue kind) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return -1;
    if (lobbies[lobby_id] == NULL) return -1;

    Lobby *l = lobbies[lobby_id];

    int is_full;
    switch (kind) {
        case RECEIVE_QUEUE:
            is_full = mq_is_full(l->receive_queue);
            return is_full;
        case SEND_QUEUE:
            is_full = mq_is_full(l->send_queue);
            return is_full;
        default:
            return -1;
        }
}

// Creates a player and returns the MessageQueue corresponding to this player.
ResponseCode create_player(int player_socket, int player_id, char *username) {
    if (get_player_space_from_id(player_id) != -1) return EC_PLAYER_UUID_EXISTS;

    int player_space = get_available_player_space();
    pthread_mutex_lock(&players_mutex[player_space]);
    if (player_space == -1) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        return EC_TOO_MANY_PLAYERS;
    }
    if (players[player_space] != NULL) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        return EC_PLAYER_UUID_EXISTS;
    }


    Player *p = (Player*)malloc(sizeof(Player));
    if (p == NULL) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        return EC_INTERNAL_ERROR;
    }

    p->socket = player_socket;
    p->player_id = player_id;
    strncpy(p->username, username, MAX_USERNAME_LENGTH);
    p->state = PS_CONNECTED_TO_SERVER;
    p->lobby_id = -1;
    p->public_player_id = -1;

    players[player_space] = p;

    pthread_mutex_unlock(&players_mutex[player_space]);
    return RC_SUCCESS;
}

// Deletes a player from the game.
// Returns EC_PLAYER_DOESNT_EXIST when the player doesn't exist, RC_SUCCESS when the player was deleted successfully
ResponseCode delete_player(int player_id, Message **player_quit) {
    int player_space = get_player_space_from_id(player_id);
    if (player_space == -1) return EC_PLAYER_DOESNT_EXIST; 

    pthread_mutex_lock(&players_mutex[player_space]);

    Player *p = players[player_space];

    if (p->lobby_id >= 0) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        ResponseCode rc = quit_lobby(player_id, player_quit);
        pthread_mutex_lock(&players_mutex[player_space]);

        if (rc != RC_SUCCESS) {
            pthread_mutex_unlock(&players_mutex[player_space]);
            return rc;
        }
    }

    free(p);
    players[player_space] = NULL;
    
    pthread_mutex_unlock(&players_mutex[player_space]);
    return RC_SUCCESS;
}

// Returns the lobby's id in lobby_id if successfully created
ResponseCode create_lobby(char *name, int max_players, int owner_id, int *lobby_id) {
    if (max_players <= 0) return EC_CANNOT_CREATE_LOBBY;
    
    int lobby_space = get_available_lobby_space();
    
    if (lobby_space == -1) return EC_TOO_MANY_LOBBIES;
    
    pthread_mutex_lock(&lobbies_mutex[lobby_space]);
    
    Lobby *lobby = (Lobby*)malloc(sizeof(Lobby));
    
    if (lobby == NULL) {
        pthread_mutex_unlock(&lobbies_mutex[lobby_space]);
        return EC_INTERNAL_ERROR;
    }
    
    lobby->lobby_id = lobby_space;
    lobby->max_players = max_players;
    lobby->private = 0; // Unused

    if (strlen(name) > MAX_LOBBY_LENGTH) {
        pthread_mutex_unlock(&lobbies_mutex[lobby_space]);
        return EC_CANNOT_CREATE_LOBBY;
    }
    strncpy(lobby->name, name, MAX_LOBBY_LENGTH);

    lobby->owner_id = owner_id;
    lobby->state = GS_WAITING_ROOM;
    
    lobby->players = (Player**)malloc(sizeof(Player*)*max_players);
    lobby->player_points = (int*)malloc(sizeof(int)*max_players);
    
    if ((lobby->players == NULL) || (lobby->player_points == NULL)) {
        free(lobby->players);
        free(lobby->player_points);
        free(lobby);
        pthread_mutex_unlock(&lobbies_mutex[lobby_space]);
        return EC_INTERNAL_ERROR;
    }
    
    int owner_space = get_player_space_from_id(owner_id);

    // Creating a lobby as a ghost player should never happen and is an 
    // internal error
    if (owner_space == -1) {
        free(lobby->players);
        free(lobby->player_points);
        free(lobby);
        pthread_mutex_unlock(&lobbies_mutex[lobby_space]);
        return EC_INTERNAL_ERROR;
    }
    
    players[owner_space]->lobby_id = lobby->lobby_id;
    lobby->players[0] = players[owner_space];
    lobby->players[0]->public_player_id = 0; // The owner is always the first player
    lobby->players[0]->state = PS_IN_WAITING_ROOM;
    for (int i = 1; i < max_players; i++) {
        lobby->players[i] = NULL; // No player in this space
    }
    lobby->send_thread = NULL; // No send thread yet
    lobby->players_in_lobby = 1;
    lobby->player_points[0] = 0;

    lobby->receive_queue = mq_init();
    lobby->send_queue = mq_init();
    
    create_lobby_table(DATABASE_PATH, lobby->lobby_id);
    
    lobbies[lobby_space] = lobby;
    
    if (lobby_id != NULL) {
        *lobby_id = lobby->lobby_id;
    }
    
    pthread_mutex_unlock(&lobbies_mutex[lobby_space]);
    return RC_SUCCESS;
}

// Deletes a lobby from the game.
// Returns EC_CANNOT_DELETE_LOBBY when the lobby has players in it
ResponseCode delete_lobby(int lobby_id) {
    Lobby *l = lobbies[lobby_id];
    if (l->players_in_lobby > 0) return EC_CANNOT_DELETE_LOBBY;

    pthread_mutex_lock(&lobbies_mutex[lobby_id]);

    if (l->state == GS_QUESTION || (l->state == GS_ANSWER)) {
        pthread_cancel(l->game_thread);
        pthread_join(l->game_thread, NULL);
    }
    
    destroy_lobby_table(DATABASE_PATH, lobby_id);
    
    mq_free(l->receive_queue);
    mq_free(l->send_queue);
    free(l->players); // No one is in the lobby so we can just free it like this
    free(l->player_points);
    free(l);

    lobbies[lobby_id] = NULL;
    
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
    
    return RC_SUCCESS;
}

// Returns the PlayerQuit message in player_quit
ResponseCode quit_lobby(int player_id, Message **player_quit) {
    int player_space = get_player_space_from_id(player_id);
    if (player_space == -1) return EC_INTERNAL_ERROR;
    
    
    Player *p = players[player_space];
    pthread_mutex_lock(&players_mutex[player_space]);
    pthread_mutex_lock(&lobbies_mutex[p->lobby_id]);
    Lobby *l = lobbies[p->lobby_id];
    
    if (p->lobby_id < 0) {
        pthread_mutex_unlock(&lobbies_mutex[p->lobby_id]);
        pthread_mutex_unlock(&players_mutex[player_space]);
        return RC_SUCCESS;
    }
    
    int lobby_id = p->lobby_id;

    // Updates the local lobby & player definition
    l->players_in_lobby--;
    if (p->public_player_id < 0 || p->public_player_id >= l->max_players) {
        pthread_mutex_unlock(&lobbies_mutex[p->lobby_id]);
        pthread_mutex_unlock(&players_mutex[player_space]);
        return EC_INTERNAL_ERROR;
    }
    l->players[p->public_player_id] = NULL;
    l->player_points[p->public_player_id] = -1;
    p->lobby_id = -1;
    p->state = PS_CONNECTED_TO_SERVER;
    
    // If the person quitting is the lobby's owner, we change
    // the owner to player with the lowest id
    if (player_id == l->owner_id) {
        int changed = 0;
        for (int i = 0; i < l->max_players; i++) {
            if (l->players && l->players[i] != NULL) {
                if (l->players[i]->player_id == l->owner_id) continue;
                l->owner_id = l->players[i]->player_id;
                changed = 1;
                break;
            }
        }
        
        // If the owner couldn't be changed, then there are no more players
        // in the lobby and it has to be deleted
        if (!changed) {
            pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
            pthread_mutex_unlock(&players_mutex[player_space]);
            delete_lobby(l->lobby_id);
            return RC_SUCCESS;
        }
    }
    
    // Creates the message to broadcast to everyone else
    PlayerQuit *playerquit = (PlayerQuit*)malloc(sizeof(PlayerQuit));
    if (playerquit == NULL) {
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        pthread_mutex_unlock(&players_mutex[player_space]);
        return EC_INTERNAL_ERROR;
    }

    playerquit->player_id = p->public_player_id;
    
    Message *m = payload_to_message(PLAYER_QUIT, (void*)playerquit, SERVER_UUID);
    if (m == NULL) {
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        pthread_mutex_unlock(&players_mutex[player_space]);
        return EC_INTERNAL_ERROR;
    }
    
    if (player_quit != NULL) {
        *player_quit = m;
    }
    
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
    pthread_mutex_unlock(&players_mutex[player_space]);
    
    return RC_SUCCESS;
}

// Returns the PlayerJoined message in player_joined
ResponseCode join_lobby(int player_id, int lobby_id, Message **player_joined) {
    int player_space = get_player_space_from_id(player_id);
    
    Player *p = players[player_space];
    
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return EC_LOBBY_DOESNT_EXIST;
    Lobby *l = lobbies[lobby_id];
    if (player_space == -1) return EC_INTERNAL_ERROR;
    
    pthread_mutex_lock(&players_mutex[player_space]);
    pthread_mutex_lock(&lobbies_mutex[lobby_id]);

    if (p->lobby_id >= 0) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_ALREADY_IN_LOBBY;
    }
    
    if (l->players_in_lobby >= l->max_players) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_LOBBY_FULL;
    }
    
    if (l->state != GS_WAITING_ROOM) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_GAME_ALREADY_STARTED;
    }
    
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
    int public_id = get_available_public_id(lobby_id);
    pthread_mutex_lock(&lobbies_mutex[lobby_id]);

    // We already checked that there is enough room for another player, so this case is
    // an internal error
    if (public_id < 0) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_INTERNAL_ERROR;
    }
    
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
    if (m == NULL) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_INTERNAL_ERROR;
    }
    fflush(stdout);
    if (player_joined != NULL) {
        *player_joined = m;
    }

    pthread_mutex_unlock(&players_mutex[player_space]);
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);

    return RC_SUCCESS;
}

// Starts the game in the lobby with the given id.
ResponseCode start_game(int player_id, int lobby_id) {
    // Strat: on crée un thread pour chaque lobby actif i.e dans l'état GS_QUESTION ou GS_ANSWER.
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return EC_LOBBY_DOESNT_EXIST;

    Lobby *l = lobbies[lobby_id];
    
    printf("here\n");

    int player_space = get_player_space_from_id(player_id);
    if (player_space == -1) return EC_INTERNAL_ERROR;
    
    pthread_mutex_lock(&players_mutex[player_space]);
    pthread_mutex_lock(&lobbies_mutex[lobby_id]);
    
    
    if (l->owner_id != player_id ) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_INSUFFICIENT_PERMISSION;
    }
    if (l->state != GS_WAITING_ROOM) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_GAME_ALREADY_STARTED;
    }
    if (l->players_in_lobby < 2) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_NOT_ENOUGH_PLAYERS;
    }
    
    pthread_create(&l->game_thread, NULL, game_loop, (void*)l);
    
    Message *game_starts = (Message*)malloc(sizeof(Message));
    if (game_starts == NULL) {
        pthread_cancel(l->game_thread);
        pthread_join(l->game_thread, NULL);

        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_INTERNAL_ERROR;
    }

    game_starts->uuid = SERVER_UUID;
    game_starts->type = GAME_STARTS;
    game_starts->payload_size = 0;
    game_starts->payload = NULL;

    pthread_mutex_unlock(&players_mutex[player_space]);
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
    ResponseCode rc = lobby_enqueue(lobby_id, SEND_QUEUE, game_starts, NO_SOCKET);
    pthread_mutex_lock(&players_mutex[player_space]);
    pthread_mutex_lock(&lobbies_mutex[lobby_id]);


    if (rc != RC_SUCCESS) {
        pthread_mutex_unlock(&players_mutex[player_space]);
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_INTERNAL_ERROR;
    }

    pthread_mutex_unlock(&players_mutex[player_space]);
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);

    return RC_SUCCESS;
}

// Returns a list of lobbies in the game.
ResponseCode get_lobby_list(Message **lobbylist) {
    LobbyList *ll = (LobbyList*)malloc(sizeof(LobbyList));
    
    if (ll == NULL) {
        return EC_INTERNAL_ERROR;
    }
    
    lock_all_lobbies();
    int cnt = 0;
    for (int i = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
        if (lobbies[i] == NULL) continue;

        ll->ids[cnt] = i;
        strncpy(ll->names[cnt], lobbies[i]->name, MAX_LOBBY_LENGTH);
        cnt++;
    }
    unlock_all_lobbies();

    ll->__quantity = cnt;

    *lobbylist = payload_to_message(LOBBYLIST, ll, SERVER_UUID);

    if (errno != 0) return EC_INTERNAL_ERROR;

    return RC_SUCCESS;
}

// Returns the PlayersData message with the players in the lobby.
// If the lobby doesn't exist or there are no players in it, returns EC_LOBBY_DOESNT_EXIST.
ResponseCode get_players_data(int lobby_id, Message **playersdata) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return EC_LOBBY_DOESNT_EXIST;
    if (lobbies[lobby_id] == NULL) return EC_LOBBY_DOESNT_EXIST;

    
    Lobby *l = lobbies[lobby_id];
    PlayersData *pdata = (PlayersData*)malloc(sizeof(PlayersData));

    if (pdata == NULL) return EC_INTERNAL_ERROR;

    lock_all_players();
    pthread_mutex_lock(&lobbies_mutex[lobby_id]);
    
    pdata->number_of_players = l->players_in_lobby;
    pdata->player_names = malloc(sizeof(char)*MAX_USERNAME_LENGTH*l->players_in_lobby);
    pdata->player_points = (int*)malloc(sizeof(int)*l->players_in_lobby);
    pdata->players_id = (int*)malloc(sizeof(int)*l->players_in_lobby);
    
    
    if ((pdata->player_names == NULL) || (pdata->player_points == NULL) || (pdata->players_id == NULL)) {
        free(pdata->player_names);
        free(pdata->player_points);
        free(pdata->players_id);
        free(pdata);
        unlock_all_players();
        pthread_mutex_unlock(&lobbies_mutex[lobby_id]);
        return EC_INTERNAL_ERROR;
    }
    
    int cnt = 0;
    for (int i = 0; i < l->max_players; i++) {
        if (l->players[i] == NULL) continue; 
        
        strncpy(pdata->player_names[cnt], l->players[i]->username, MAX_USERNAME_LENGTH);
        pdata->player_points[cnt] = l->player_points[i];
        pdata->players_id[cnt] = l->players[i]->public_player_id;
        
        cnt++;
    } 
    for (int i = cnt; i < l->players_in_lobby; i++) {
        pdata->player_names[i][0] = '\0'; // Empty string
        pdata->player_points[i] = -1; // No points
        pdata->players_id[i] = -1; // No player id
    }

    *playersdata = payload_to_message(PLAYERS_DATA, pdata, SERVER_UUID);

    unlock_all_players();
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);

    return RC_SUCCESS;
}

// Returns 0 when you can't submit answers
// Returns 1 when you can
int can_submit_answers(int lobby_id, int player_id) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return 0;
    if (lobbies[lobby_id] == NULL) return 0;

    int player_space = get_player_space_from_id(player_id);
    if (player_space == -1) return 0;

    Lobby *l = lobbies[lobby_id];
    Player *p = players[player_space];

    pthread_mutex_lock(&players_mutex[player_space]);
    pthread_mutex_lock(&lobbies_mutex[lobby_id]);
    int ret = (l->state == GS_QUESTION) && (p->state == PS_IN_GAME_NOT_ANSWERED);
    pthread_mutex_unlock(&players_mutex[player_space]);
    pthread_mutex_unlock(&lobbies_mutex[lobby_id]);

    return ret;
}

// Ends the game in the lobby with the given id.
// Returns 0 when the game couldn't be ended, 1 when it was ended successfully
int end_game(int lobby_id, int winner_public_id) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return 0;
    if (lobbies[lobby_id] == NULL) return 0;
    
    Lobby *l = lobbies[lobby_id];

    if ((l->state != GS_QUESTION) && (l->state != GS_ANSWER)) return 0;

    GameEnded *ge = (GameEnded*)malloc(sizeof(GameEnded));
    if (ge == NULL)  {
        return 0;
    }


    ge->winner_id = winner_public_id;

    Message *gem = payload_to_message(GAME_ENDED, (void*)ge, SERVER_UUID);

    lobby_enqueue(lobby_id, SEND_QUEUE, gem, NO_SOCKET);

    // Reseting the player_state
    for (int i = 0; i < l->max_players; i++) {
        if (l->players[i] == NULL) continue;
        
        l->players[i]->state = PS_IN_WAITING_ROOM;
    }
    l->state = GS_WAITING_ROOM;

    int rc = wipe_lobby_table(DATABASE_PATH, lobby_id);
    if (rc != SQLITE_OK) {
        printf("[ERROR]: Could not wipe the lobby table for lobby %i\n", lobby_id);
        return 0;
    }

    return 1;
}

// Returns the public player id of the player with the most points in the lobby.
// Returns -1 when the lobby doesn't exist, the game isn't in a valid state or there are no players in the lobby
int get_player_public_id_with_max_points(int lobby_id) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return -1;
    if (lobbies[lobby_id] == NULL) return -1;

    Lobby *l = lobbies[lobby_id];

    if ((l->state != GS_QUESTION) && (l->state != GS_ANSWER)) return -1;
    
    int max_points = -1;
    int player_with_max_points = -1;
    
    for (int i = 0; i < l->max_players; i++) {
        if (l->players[i] == NULL) continue;
        
        printf("Player %s has %i points\n", l->players[i]->username, l->player_points[i]);
        if (l->player_points[i] > max_points) {
            max_points = l->player_points[i];
            player_with_max_points = l->players[i]->public_player_id;
        }
    }
    
    return player_with_max_points;
}

// Enqueues a question 'q' to be sent to clients of the lobby 'lobby_id'.
// Returns 0 when the question couldn't be sent, 1 when it was sent successfully
int send_question(int lobby_id, Question *q) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return 0;
    if (lobbies[lobby_id] == NULL) return 0;
    
    Lobby *l = lobbies[lobby_id];
    if (l->state != GS_QUESTION) return 0;

    QuestionSent *qs = (QuestionSent*)malloc(sizeof(QuestionSent));
    if (qs == NULL) {
        return 0;
    }

    strncpy(qs->question, q->question, MAX_QUESTION_LENGTH);
    qs->support_type = q->support_type;

    size_t support_size;
    if (q->support_type == BITMAP) {
        qs->support = (char*)__read_image_to_buffer(q->support, &support_size);
        if (qs->support == NULL) {
            free(qs);
            return 0; // Error reading image
        }
        if (support_size > MAX_PAYLOAD_LENGTH) {
            free(qs->support);
            free(qs);
            return 0; // Support too large
        }
    } else {
        support_size = strlen(q->support);
        qs->support = (char*)malloc(sizeof(char)*(support_size + 1));
        if (qs->support == NULL) {
            return 0;
        }
        // For some reason, strncpy doesn't work here
        memcpy(qs->support, q->support, support_size + 1);
    }


    Message *qsm = payload_to_message(QUESTION_SENT, (void*)qs, SERVER_UUID);
    
    lobby_enqueue(lobby_id, SEND_QUEUE, qsm, NO_SOCKET);

    return 1;
}

// Sends a PlayerResponseChanged message to the clients of the lobby 'lobby_id'.
// Returns 0 when the message couldn't be sent, 1 when it was sent successfully
int send_player_response_changed(int lobby_id, int is_correct, int public_id, int points_earned, char *response) {
    if ( (lobby_id < 0) || (lobby_id >= MAX_NUMBER_OF_LOBBIES)) return 0;
    if (lobbies[lobby_id] == NULL) return 0;
    
    Lobby *l = lobbies[lobby_id];

    if (l->state != GS_QUESTION) return 0;

    PlayerResponseChanged *prespchanged = (PlayerResponseChanged*)malloc(sizeof(PlayerResponseChanged));
    if (prespchanged == NULL) {
        return 0;
    }
    if (response == NULL) {
        response = "No Response";
    }
    if (strlen(response) > MAX_RESPONSE_LENGTH) {
        response[MAX_RESPONSE_LENGTH - 1] = '\0'; // Ensure null termination
    }
    prespchanged->is_correct = is_correct;
    prespchanged->player_id = public_id;
    prespchanged->points_earned = is_correct == 0 ? 0 : points_earned;
    if (is_correct == 1) {
        strncpy(prespchanged->response, "Correct Answer", MAX_RESPONSE_LENGTH);
    } else {
        strncpy(prespchanged->response, response, MAX_RESPONSE_LENGTH);
    }
    
    Message *prespchanged_m = payload_to_message(PLAYER_RESPONSE_CHANGED, (void*)prespchanged, SERVER_UUID);
    lobby_enqueue(lobby_id, SEND_QUEUE, prespchanged_m, NO_SOCKET);
    return 1;
}


// The game loop for the lobby with the given id.
// This function is run in a separate thread and handles the game logic.
// It will run until the game is over or an error occurs.
// It will also handle the queuing of QUESTION_SENT, ANSWER_SENT, PLAYER_RESPONSE_CHANGED & GAME_ENDED messages.
// It also processes the answers sent by the players and updates their points accordingly.
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
    
    // sleep time before game starts
    Question *questions;
    int retrived_questions = get_random_questions(DATABASE_PATH, lobby_id, QUESTIONS_TO_RETRIEVE, &questions);
    if (retrived_questions == 0) {
        // If we don't have any questions in the db, we can't start the game
        printf("[WARNING]: No questions in the database for lobby n°%i: '%s', ending game\n", l->lobby_id, l->name);
        end_game(lobby_id, -1);
        pthread_exit(&rc);
    }

    sleep(TIME_BEFORE_GAME_STARTS);

    int max_pts = 0;
    int winner_public_id = -1;
    int questions_asked = 0;
    int number_of_correct_answer = 0;
    time_t first_correct_answer;
    while (max_pts < POINTS_TO_WIN) {
        if (questions_asked >= retrived_questions) {
            for (int i = 0; i < retrived_questions; i++) {
                free(questions[i].support);
                for (int j = 0; j < questions->number_of_valid_answers; j++) {
                    free(questions[i].valid_answers[j]);
                }
                free(questions[i].valid_answers);
            }
            free(questions);

            questions_asked = 0;
            retrived_questions = get_random_questions(DATABASE_PATH, lobby_id, QUESTIONS_TO_RETRIEVE, &questions);

            // If we don't have anymore questions in the db we end the game
            if (retrived_questions == 0) {
                winner_public_id = get_player_public_id_with_max_points(lobby_id);
                printf("[WARNING]: No more questions in the database for lobby n°%i: '%s', ending game\n", l->lobby_id, l->name);
                break;
            }
        }

        l->state = GS_QUESTION;
        number_of_correct_answer = 0;
        if (!send_question(lobby_id, &questions[questions_asked])) exit(EXIT_FAILURE);
        
        // Reseting the player_state
        for (int i = 0; i < l->max_players; i++) {
            if (l->players[i] == NULL) continue;
            l->players[i]->state = PS_IN_GAME_NOT_ANSWERED;
        }

        time_t question_started = time(NULL);
        
        MessageQueueItem *mqi = lobby_dequeue(lobby_id, RECEIVE_QUEUE);
        time_t message_time;

        message_time = (mqi == NULL) ? time(NULL) : mqi->time;
        while ((message_time - question_started <= TIME_TO_ANSWER) || 
        ((message_time - question_started > TIME_TO_ANSWER) && (lobby_mq_is_empty(lobby_id, RECEIVE_QUEUE) != 1))
    ) {
            // If There is not messages skip to the next 
            if (mqi == NULL) {
                mqi = lobby_dequeue(lobby_id, RECEIVE_QUEUE);
                struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000}; // 1ms
                nanosleep(&ts, &ts);
                message_time = (mqi == NULL) ? time(NULL) : mqi->time;
                continue;
            }

            // printf("Answer detected!\n");
            
            SendResponse *sendreponse = (SendResponse*)mqi->m->payload;
            int is_correct = 0;
            int public_id;
            int points_earned;
            char *sanitized;
            for (int i = 0; i < questions[questions_asked].number_of_valid_answers; i++) {
                sanitized = __sanitize_token(sendreponse->response);
                // If answer correct
                if (strcmp(sanitized, questions[questions_asked].valid_answers[i]) == 0) {
                    is_correct = 1;
                    int player_space = get_player_space_from_id(mqi->m->uuid);
                    public_id = players[player_space]->public_player_id;
                    if (number_of_correct_answer == 0) {
                        points_earned = 10;
                        l->player_points[public_id] += 10;
                        first_correct_answer = mqi->time;
                    } else {                  
                        points_earned = 10 - (mqi->time - first_correct_answer) < 1 ? 1 : 10 - (mqi->time - first_correct_answer);
                        l->player_points[public_id] += points_earned;
                    }
                    number_of_correct_answer++;
                    players[player_space]->state = PS_IN_GAME_ANSWERED;
                    break;
                }
                free(sanitized);
            }
            send_player_response_changed(lobby_id, is_correct, public_id, points_earned, sanitized);

            free_message(mqi->m);
            free(mqi);
            mqi = lobby_dequeue(lobby_id, RECEIVE_QUEUE);
            message_time = (mqi == NULL) ? time(NULL) : mqi->time;
        }

        AnswerSent *as = (AnswerSent*)malloc(sizeof(AnswerSent));
        if (as == NULL) {
            // TODO: handle this better
            exit(EXIT_FAILURE);
        }
        
        l->state = GS_ANSWER;

        strncpy(as->answer, questions[questions_asked].valid_answers[0], MAX_RESPONSE_LENGTH);
        Message *as_m = payload_to_message(ANSWER_SENT, (void*)as, SERVER_UUID);

        lobby_enqueue(lobby_id, SEND_QUEUE, as_m, NO_SOCKET);
        
        sleep(TIME_INBETWEEN_QUESTIONS);

        // Updating the maximum points 
        winner_public_id = get_player_public_id_with_max_points(lobby_id);

        questions_asked++;
    }

    for (int i = 0; i < retrived_questions; i++) {
        free(questions[i].support);
        for (int j = 0; j < questions[i].number_of_valid_answers; j++) {
            free(questions[i].valid_answers[j]);
        }
        free(questions[i].valid_answers);
    }
    free(questions);

    int ok = end_game(lobby_id, winner_public_id);
    if (!ok) {
        rc = EC_INTERNAL_ERROR;
        pthread_exit(&rc);
    }

    rc = RC_SUCCESS;
    pthread_exit(&rc);
}