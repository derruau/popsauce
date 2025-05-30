#ifndef GAME_LOGIC_HEADER
#define GAME_LOGIC_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "common.h"
#include "message_queue.h"
#include "questions.h"

#define NO_SOCKET -1

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
    int socket;
    char username[MAX_USERNAME_LENGTH];
    int lobby_id; // Negative value => not in a lobby
    PlayerState state;
} Player;

typedef struct {
    int lobby_id;
    int owner_id;
    int private; // Unused, will be implemented when Lobby Rules are implemented
    int max_players;
    int players_in_lobby;
    pthread_t *game_thread;
    pthread_t *send_thread;
    char name[MAX_LOBBY_LENGTH];
    GameState state;
    // Pour ces deux, la position dans les array est la même que
    // public_player_id
    int *player_points; // negative points => no players;
    Player **players; // NULL ptr => no player

    MessageQueue *receive_queue; // Messages from players to the server
    MessageQueue *send_queue; // Messages from the server broadcasted to the players
} Lobby;

// Used in get_message_queue_of_lobby()
typedef enum {
    RECEIVE_QUEUE,
    SEND_QUEUE,
} LobbyMessageQueue;

extern Lobby *lobbies[MAX_NUMBER_OF_LOBBIES];
extern pthread_mutex_t lobbies_mutex[MAX_NUMBER_OF_LOBBIES]; // TODO: implement that
extern Player *players[MAX_NUMBER_OF_PLAYERS];
extern pthread_mutex_t players_mutex[MAX_NUMBER_OF_PLAYERS]; // TODO: implement that

void init_mutexes();

void lock_all_lobbies();

void lock_all_players();

void unlock_all_lobbies();

void unlock_all_players();

// Gets the first available lobby space,
// returns -1 if no space was found
int get_available_lobby_space();

// Gets the first available player space,
// returns -1 if no space was found
int get_available_player_space();

// Returns a player's space from his id
// returns -1 if player doesn't exist
int get_player_space_from_id(int player_id);

// returns an available public_id for a specific lobby
// returns -1 if none is found
int get_available_public_id(int lobby_id);

// Returns the lobby_id of the player.
// Retuns a negative value when either the player isn't in a lobby or 
// the player doesn't exit
int get_lobby_of_player(int player_id);

// Returns a list of players from the lobby.
// Also returns the number of players in the lobby in players_in_lobby
// Returns NULL when in invalid lobby id is specified
Player **get_players_from_lobby(int lobby_id, int *players_in_lobby);

int get_player_id_from_socket(int socket);

MessageQueue *get_message_queue_of_lobby(int lobby_id, LobbyMessageQueue queue);

ResponseCode lobby_enqueue(int lobby_id, LobbyMessageQueue kind, Message *m, int socket);

MessageQueueItem *lobby_dequeue(int lobby_id, LobbyMessageQueue kind);

// Returns 1 when message queue is empty and 0 when it's not.
// Returns -1 when lobby doesn't exist
int lobby_mq_is_empty(int lobby_id, LobbyMessageQueue kind);

// Returns 1 when message queue is full and 0 when it's not.
// Returns -1 when lobby doesn't exist
int lobby_mq_is_full(int lobby_id, LobbyMessageQueue kind);

// Creates a player and returns the MessageQueue corresponding to this player.
// Use errno for error detection.
ResponseCode create_player(int player_socket, int player_id, char *username);

ResponseCode delete_player(int player_id, Message **player_quit);

// Returns the lobby's id in lobby_id if successfully created
ResponseCode create_lobby(char *name, int max_players, int owner_id, int *lobby_id);

ResponseCode delete_lobby(int lobby_id);

// Returns the PlayerQuit message in player_quit
ResponseCode quit_lobby(int player_id, Message **player_quit);

// Returns the PlayerJoined message in player_joined
ResponseCode join_lobby(int player_id, int lobby_id, Message **player_joined);

ResponseCode start_game(int player_id, int lobby_id);

ResponseCode get_lobby_list(Message **lobbylist);

ResponseCode get_players_data(int lobby_id, Message **playersdata);

ResponseCode set_lobby_send_thread(int lobby_id, pthread_t *thread);

// Returns 0 when you can't submit answers
// Returns 1 when you can
int can_submit_answers(int lobby_id, int player_id);

// J'ai besoin d'envoyer des données entre ce thread (2) et le thread principal (1)
// de (2) vers (1): Je dois envoyer: GAME_STARTS, QUESTION_SENT, ANSWER_SENT et GAME_ENDED, PLAYER_RESPONSE_CHANGED => que des broadcast
// de (1) vers (2): Je dois envoyer: ANSWER_SENT
// Dans tout les cas, on va avoir besoin d'une file de message à traiter
void *game_loop(void *args);

#endif