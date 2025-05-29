#ifndef COMMON_HEADER
#define COMMON_HEADER

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <arpa/inet.h>

// TODO: Have coherent values for those constants 
#define MAX_USERNAME_LENGTH         32
#define MAX_RESPONSE_LENGTH         64
#define MAX_LOBBY_LENGTH            64 // The name is really bad but it's the maximum length of the name of a lobby
#define MAX_QUESTION_LENGTH         128
#define MAX_NUMBER_OF_LOBBIES       16
#define MAX_NUMBER_OF_PLAYERS       64

#define TIME_BEFORE_GAME_STARTS     5 // (in seconds)
#define TIME_TO_ANSWER              20 // (in seconds)
#define TIME_INBETWEEN_QUESTIONS    5 // (in seconds)

#define POINTS_TO_WIN               100

#define DATABASE_PATH               "questions_db.sqlite"

#define MAX_PAYLOAD_LENGTH      1024 * 1024 // 1MB Limit

#define EOT                     0x04 // End Of Transmission Character

#define SERVER_UUID             0

#pragma enum(4) // To make sure the enum is stored in 4 bytes

// IMPORTANT: Message types that need a response when sent have the LSB set to 1.
typedef enum {
    // Client Side Messages

    // Sent when connecting to server. It's used to provide the server with the client's informations.
    // The server responds by either sending a `LOBBYLIST` in which case you're successfully connected
    // or an `ERROR` response with the following error codes:
    // - `EC_PLAYER_UUID_EXISTS`: When a player has the same player_id as yours 
    // - `EC_TOO_MANY_PLAYERS`: Server is full and cannot accept any more players.
    // - `EC_BANNED`: You have been banned from this server.
    // - `EC_SERVER_PRIVATE`: Only whitelisted IPs can join the server. Theses whitelisted IPs can be configured in `server.conf`.
    // - `EC_INTERNAL_ERROR`: A general error that means something has gone very wrong with the server.
    CONNECT                     = 0b000001,
    // Sent when disconnecting from server. A sudden disconnect also works but isn't recommended.
    DISCONNECT                  = 0b000010,
    // Sent to server when wanting to create a lobby.
    // The server responds by either sending a `SUCCESS` reponse with the lobby code attached
    // or an `ERROR` response with the following error codes:
    // - `EC_TOO_MANY_LOBBIES`: Server has reached it's maximum number of lobbies allowed
    // - `EC_CANNOT_CREATE_LOBBY`: General error for refusing the lobby's creation
    // - `EC_INTERNAL_ERROR`: A general error that means something has gone very wrong with the server.
    CREATE_LOBBY                = 0b000011,
    // Sent to the server when wanting to join a certain lobby
    // The server responds by either sending a `PLAYERS_DATA` (it also contains your data) in which case you've
    // officially joined the lobby or an `ERROR` response with the following error codes:
    // - `EC_LOBBY_PRIVATE`: (UNUSED) Only whitelisted players can join
    // - `EC_ALREADY_IN_LOBBY`: When you're trying to join a lobby but you're already in one
    // - `EC_GAME_ALREADY_STARTED`: When you're trying to join a lobby whose game has already started 
    // - `EC_LOBBY_FULL`: The lobby is full and cannot host any more players
    // - `EC_LOBBY_DOESNT_EXIST`: The lobby doesn't exist, either because it was deleted or because it never existed in the first place.
    // - `EC_INTERNAL_ERROR`: A general error that means something has gone very wrong with the server.
    JOIN_LOBBY                  = 0b000101,
    // Sent to the server when quitting a lobby. Suddently closing the program also works but sending this message is recommended.
    QUIT_LOBBY                  = 0b000110,
    // Sent to the server when you're the lobby's host and want to start the game.
    // The server response by either sending a `SUCCESS`
    // or it sends an `ERROR` response with the following error codes:
    // - `EC_LOBBY_DOESNT_EXIST`: The lobby doesn't exist, either because it was deleted or because it never existed in the first place.
    // - `EC_NOT_ENOUGH_PLAYERS`: The game can only start when enough players have joined.
    // - `EC_INSUFFICIENT_PERMISSION`: You're not the lobby's host.
    // - `EC_GAME_ALREADY_STARTED`: For when you try to start a game that has already started.
    // - `EC_INTERNAL_ERROR`: A general error that means something has gone very wrong with the server.
    START_GAME                  = 0b000111,
    // Sent to the server when the host changes the rules of the lobby.
    // The server either responds by sending a `SUCCESS` in which case the game starts in 5 seconds.
    // or an `ERROR` with the following error codes:
    // - `EC_INSUFFICIENT_PERMISSION`: You're not the lobby's host.
    // - `EC_INTERNAL_ERROR`: A general error that means something has gone very wrong with the server.
    CHANGE_RULES                = 0b001001,
    // Sent to the server when you submit a response to a question.
    // The server responds with either a `SUCCESS` message in which case you have got the correct answer
    // or an `ERROR` message with the following error codes:
    // - `EC_WRONG_RESPONSE`: The response you sent is wrong.
    // - `EC_CANNOT_SUBMIT`: You cannot submit a response right now. Either because the game is in a state
    //                    where it doesn't accept response, or because you have already responded correctly.
    SEND_RESPONSE               = 0b001011, 


    // Server Side Messages (Broadcasted)

    // Broadcasted to players in a lobby when someone has joined the lobby.
    PLAYER_JOINED               = 0b001100,
    // Broadcasted to players in a lobby when someone has quit the lobby.
    PLAYER_QUIT                 = 0b001110,
    // Broadcasted to players in a lobby when the game is about to start. 
    // Normally, the game starts 5 seconds after receiving this message.
    GAME_STARTS                 = 0b010000,
    // Broadcasted to players in a lobby when the host has changed the rules.
    RULES_CHANGED               = 0b010010,
    // Broadcasted to players in a lobby when someone has submitted a response.
    PLAYER_RESPONSE_CHANGED     = 0b010100,
    // Broadcasted to players when a new question starts, this message contains the new question.
    QUESTION_SENT               = 0b010110,
    // Broadcasted to players when the question answer time is over. It's used to tell players what was the answer.
    ANSWER_SENT                 = 0b011000,
    // Broadcasted to players in a lobby when the game has ended.
    GAME_ENDED                  = 0b011010,

    // Server Response Messages (To Individuals)
    // Generally sent to confirm that a message that needs a response has been executed successfully.
    SUCCESS                     = 0b011100,
    // Generally sent to confirm that a message that needs a response has encountered an error.
    // The specifics of the error are contained inside the error code of the message.
    ERROR                       = 0b011110,
    // Sent by the server when requesting the list of lobbies that this server hosts. 
    // Generally sent by the server as a response to successfully connecting to the server.
    LOBBYLIST                   = 0b100000,
    // Sent by the server when joining a lobby. This message contains the player's data, meaning 
    PLAYERS_DATA                = 0b100010,
} MessageType;

typedef enum {
    RC_SUCCESS, // When an operation executed successfully
    EC_PLAYER_UUID_EXISTS,
    EC_INTERNAL_ERROR,
    EC_SERVER_PRIVATE,
    EC_BANNED,
    EC_TOO_MANY_PLAYERS,
    EC_GAME_ALREADY_STARTED,
    EC_TOO_MANY_LOBBIES,
    EC_CANNOT_CREATE_LOBBY,
    EC_CANNOT_DELETE_LOBBY,
    EC_LOBBY_PRIVATE,
    EC_ALREADY_IN_LOBBY,
    EC_LOBBY_FULL,
    EC_LOBBY_DOESNT_EXIST,
    EC_NOT_ENOUGH_PLAYERS,
    EC_INSUFFICIENT_PERMISSION,
    EC_WRONG_RESPONSE,
    EC_CANNOT_SUBMIT,
    EC_GAME_ALREADY_STARTED
} ResponseCode;

typedef enum {
    STRING,
    BITMAP,
} SupportType;

typedef struct {
    char username[MAX_USERNAME_LENGTH];
} Connect;

typedef struct {
    int max_players;
    char name[MAX_LOBBY_LENGTH];
} CreateLobby;

typedef struct {
    int lobby_id;
} JoinLobby;

// TODO
typedef struct {
} ChangeRules;

typedef struct {
    char response[MAX_RESPONSE_LENGTH];
} SendResponse;

typedef struct {
    // Une id spéciale différente de uuid qui permet d'authentifier un joueur 
    // pour les autres membres du serveur sans donner son véritable uuid
    // qui sert de sécurité d'authentification
    int player_id;
    char name[MAX_USERNAME_LENGTH];
} PlayerJoined;

typedef struct {
    int player_id;
} PlayerQuit;

// TODO
typedef struct {

} RulesChanged;

typedef struct {
    int player_id;
    int points_earned;
    int is_correct;
    char response[MAX_RESPONSE_LENGTH];
} PlayerResponseChanged;

typedef struct {
    char answer[MAX_RESPONSE_LENGTH];
} AnswerSent;

typedef struct {
    int winner_id; // The public ID of the winner of the game
} GameEnded;

// Ces messages sont compliqués à extraire, ils requièrent une méthode spéciale
// ==========================================================
typedef struct {
    char question[MAX_QUESTION_LENGTH];
    SupportType support_type;
    char *support;
} QuestionSent;

typedef struct {
    int number_of_players;
    char (*player_names)[MAX_USERNAME_LENGTH];
    int *player_points;
    int *players_id;
} PlayersData;
// ==========================================================


typedef struct {
    uint32_t __quantity;
    uint32_t ids[MAX_NUMBER_OF_LOBBIES];
    char names[MAX_LOBBY_LENGTH][MAX_NUMBER_OF_LOBBIES];
} LobbyList;


typedef struct {
    int data;
} Success;

typedef struct {
    int data;
} Error;

typedef struct {
    MessageType type;           // 4 bytes
    uint32_t uuid;              // 4 bytes
    uint32_t payload_size;      // 4 bytes 
    void *payload;
} Message;

// Returns 1 when payload has fixed size, 0 otherwise.
int __payload_is_fixed_size(MessageType type);

// Returns the size of a payload given it's type.
// For some message types, this is not sufficient so the payload is needed as well.
int __get_payload_size(MessageType type, void *payload);

// Helper function.
// Serializes a fixed size payload into the buffer. The buffer needs to already be initialized and have sufficient space
// for copying the serialized data onto it.
// Returns the amount of bytes copied to the buffer
int __serialize_fixed_size_payload(MessageType type, void *payload, uint8_t *buffer);

// Helper function.
// Serializes a QuestionSent payload into the buffer. The buffer needs to already be initialized and have sufficient space
// for copying the serialized data onto it.
// Returns the amount of bytes copied to the buffer.
int __serialize_question_sent(QuestionSent *payload, uint8_t *buffer);
// Helper function.
// Serializes a PlayersData payload into the buffer. The buffer needs to already be initialized and have sufficient space
// for copying the serialized data onto it.
// Returns the amount of bytes copied to the buffer.
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
int __serialize_players_data(PlayersData *payload, uint8_t *buffer);

// Frees a Message 
void free_message(Message *m);

// Returns a Message from a payload and the player's uuid.
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
Message *payload_to_message(MessageType type, void *payload, uint32_t uuid);

// Serializes a message into the buffer and also returns the buffer's size into `buffer_size`. 
// The buffer has to be uninitialized.
// Returns 0 when successful, and a non zero value when an error occurs.
int serialize_message(Message *message, uint8_t **buffer, uint32_t *buffer_size);

// Sends a message `buffer` throught the socket file descriptor `sockfd`.
// You also need to provide the length of the buffer.
// If the server responds to the message, the reponse will be stored inside `response`. This parameter needs to be initialized beforehand.
// If `response == NULL`, the message that is sent doesn't expect a response.
// Returns 0 when successful, and a non zero value when an error occurs.
int send_message(int sockfd, uint8_t *buffer, uint32_t buffer_size, Message *response);
// Waits for a message to be sent on the socket `sockfd` and returns it. This function is blocking.
// The message buffer's size is returned as part of the `buffer_size` parameter.
// If the message contains more bytes than max_buffer_size, the whole message is thrown away and an error is thrown.
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
uint8_t *receive_message(int sockfd, uint32_t *buffer_size, uint32_t max_buffer_size);

// Deserializes a serialized message and returns a structure that can be read by the program.
// You also need to provide the size of the buffer to be deserialized.
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
Message *deserialize_message(uint8_t *buffer, uint32_t buffer_size);

// Transforms a ResponseCode into a Message
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
Message *responsecode_to_message(ResponseCode rc, int uuid, int data);
#endif