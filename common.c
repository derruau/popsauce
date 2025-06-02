/* 
TODO: refactor serialization / deserialization mecanism to be more general.
    i.e create a function to serialize/deserialize a field automatically.

*/
#include "common.h"

// Reads the entire contents of a binary file (like a .jpg) into a buffer.
// Returns the buffer and sets `size` to the file length.
// Caller must free the returned buffer.
// Returns NULL on failure.
uint8_t *__read_image_to_buffer(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    // Seek to end to determine size
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    // Get file size
    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }

    // Check if file size exceeds maximum payload length
    if (file_size > MAX_PAYLOAD_LENGTH) {
        fclose(file);
        return NULL;
    }

    rewind(file);  // Go back to beginning

    // Allocate buffer
    uint8_t *buffer = malloc(sizeof(uint8_t)*file_size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    // Read file into buffer
    size_t read_bytes = fread(buffer, 1, file_size, file);
    if (read_bytes != (size_t)file_size) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *size = file_size;
    return buffer;
}

// Returns 1 when payload has fixed size, 0 otherwise.
int __payload_is_fixed_size(MessageType type) {
    return !( (type == QUESTION_SENT) || (type == PLAYERS_DATA) );
}

// Returns the size of a payload given it's type.
// For some message types, this is not sufficient so the payload is needed as well.
int __get_payload_size(MessageType type, void *payload) {
    switch (type) {
        case CONNECT:
            return sizeof(Connect);
        case CREATE_LOBBY:
            return sizeof(CreateLobby);
        case JOIN_LOBBY:
            return sizeof(JoinLobby);
        case CHANGE_RULES:
            return sizeof(ChangeRules);
        case SEND_RESPONSE:
            return sizeof(SendResponse);
        case PLAYER_JOINED:
            return sizeof(PlayerJoined);
        case PLAYER_QUIT:
            return sizeof(PlayerQuit);
        case RULES_CHANGED:
            return sizeof(RulesChanged);
        case PLAYER_RESPONSE_CHANGED:
            return sizeof(PlayerResponseChanged);
        case ANSWER_SENT:
            return sizeof(AnswerSent);
        case GAME_ENDED:
            return sizeof(GameEnded);
        case QUESTION_SENT:
            QuestionSent *qsent = (QuestionSent*)payload;
            if (qsent->support == NULL) {
                return sizeof(qsent->question) + sizeof(qsent->support_type);
            }
            return sizeof(qsent->question) + sizeof(qsent->support_type) + strlen(qsent->support);
        case PLAYERS_DATA:
            PlayersData *pdata = (PlayersData*)payload;
            return  sizeof(pdata->number_of_players) + 
                    pdata->number_of_players*MAX_USERNAME_LENGTH      + 
                    pdata->number_of_players*(sizeof(int) + sizeof(int));
        case LOBBYLIST:
            return sizeof(LobbyList);
        case SUCCESS:
            return sizeof(Success);
        case ERROR:
            return sizeof(Error);
        // All other Messages don't have a payload therefore their payload size is 0.
        default:
            return 0;
        }
}

// Helper function.
// Serializes a fixed size payload into the buffer. The buffer needs to already be initialized and have sufficient space
// for copying the serialized data onto it.
// Returns the amount of bytes copied to the buffer
int __serialize_fixed_size_payload(MessageType type, void *payload, uint8_t *buffer) {
    if (!__payload_is_fixed_size(type)) {
        return 1;
    }

    // This is tedious work and the order needs to be respected for deserialization.
    int offset = 0;
    uint32_t net_max_users;
    uint32_t net_lobby_id;
    uint32_t net_player_id;
    uint32_t net_data;
    uint32_t net_quantity;
    switch (type) {
        case CONNECT:
            Connect *connect = (Connect*)payload;
            memcpy(buffer, connect->username, sizeof(connect->username));
            return sizeof(connect->username);
        case CREATE_LOBBY:
            CreateLobby *createlobby = (CreateLobby*)payload;
            net_max_users = htonl(createlobby->max_players);
            memcpy(buffer, &net_max_users, sizeof(net_max_users));
            memcpy(buffer + sizeof(net_max_users), createlobby->name, sizeof(createlobby->name));
            return sizeof(net_max_users) + sizeof(createlobby->name);
        case JOIN_LOBBY:
            JoinLobby *joinlobby = (JoinLobby*)payload;
            net_lobby_id = htonl(joinlobby->lobby_id);
            memcpy(buffer, &net_lobby_id, sizeof(net_lobby_id));
            return sizeof(net_lobby_id);
        case CHANGE_RULES:
            // TODO
            break;
        case SEND_RESPONSE:
            SendResponse *sendresponse = (SendResponse*)payload;
            memcpy(buffer, sendresponse->response, sizeof(sendresponse->response));
            return sizeof(sendresponse->response);
        case PLAYER_JOINED:
            PlayerJoined *playerjoined = (PlayerJoined*)payload;
            net_player_id = htonl(playerjoined->player_id);
            memcpy(buffer, &net_player_id, sizeof(net_player_id));
            memcpy(buffer + sizeof(net_player_id), playerjoined->name, sizeof(playerjoined->name));
            return sizeof(net_player_id) + sizeof(playerjoined->name);
        case PLAYER_QUIT:
            PlayerQuit *playerquit = (PlayerQuit*)payload;
            net_player_id = htonl(playerquit->player_id);
            memcpy(buffer, &net_player_id, sizeof(net_player_id));
            return sizeof(net_player_id);
        case RULES_CHANGED:
            // TODO
            break;
        case ANSWER_SENT:
            AnswerSent *answersent = (AnswerSent*)payload;
            memcpy(buffer, answersent->answer, sizeof(answersent->answer));
            return sizeof(answersent->answer);
        case GAME_ENDED:
            GameEnded *gameended = (GameEnded*)payload;
            net_player_id = htonl(gameended->winner_id);
            memcpy(buffer, &net_player_id, sizeof(net_player_id));
            return sizeof(net_player_id);
        case PLAYER_RESPONSE_CHANGED:
            PlayerResponseChanged *prespchanged = (PlayerResponseChanged*)payload;
            net_player_id = htonl(prespchanged->player_id);
            int net_points_earned = htonl(prespchanged->points_earned);
            int net_is_correct = htonl(prespchanged->is_correct);
            memcpy(buffer, &net_player_id, sizeof(net_player_id));
            offset += sizeof(net_player_id);
            memcpy(buffer + offset, &net_points_earned, sizeof(net_points_earned));
            offset += sizeof(net_points_earned);
            memcpy(buffer + offset, &net_is_correct, sizeof(net_is_correct));
            offset += sizeof(net_is_correct);   
            memcpy(buffer + offset, prespchanged->response, sizeof(prespchanged->response));
            offset += sizeof(prespchanged->response);
            return offset;
        case LOBBYLIST:
            LobbyList *lobbylist = (LobbyList*)payload;
            net_quantity = htonl(lobbylist->__quantity);
            memcpy(buffer, &net_quantity, sizeof(net_quantity));

            uint32_t *net_ids = (uint32_t*)malloc(sizeof(uint32_t)*MAX_NUMBER_OF_LOBBIES);
            if (net_ids == NULL) return 0;

            for (int i = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
                net_ids[i] = htonl((lobbylist->ids)[i]);
            }

            memcpy(buffer + sizeof(net_quantity), net_ids, sizeof(lobbylist->ids));
            memcpy(buffer + sizeof(net_quantity) + sizeof(lobbylist->ids), lobbylist->names, sizeof(lobbylist->names));

            free(net_ids);

            return sizeof(net_quantity) + sizeof(lobbylist->ids) + sizeof(lobbylist->names);
        case SUCCESS:
            Success *success = (Success*)payload;
            net_data = htonl(success->data);
            memcpy(buffer, &net_data, sizeof(net_data));
            return sizeof(net_data);
        case ERROR:
            Error *error = (Error*)payload;
            net_data = htonl(error->data);
            memcpy(buffer, &net_data, sizeof(net_data));
            return sizeof(net_data);
        // All other Messages don't have a payload therefore their payload size is 0.
        default:
            return 0;
    }

    return 0;
}

// Helper function.
// Deserializes a fixed size payload into `payload`. 
// Returns the number of bytes copied to the payload.
int __deserialize_fixed_size_payload(MessageType type, uint8_t *buffer, void **payload) {
    if (!__payload_is_fixed_size(type)) {
        return 0;
    }

    int offset = 0;
    // This is tedious work and the order needs to be respected for deserialization.
    switch (type) {
        case CONNECT:
            *payload = malloc(sizeof(Connect));
            if (*payload == NULL) return 0;

            Connect *connect = (Connect*)*payload;
            memcpy(connect->username, buffer, sizeof(connect->username));
            return sizeof(connect->username);
        case CREATE_LOBBY:
            *payload = malloc(sizeof(CreateLobby));
            if (*payload == NULL) return 0;

            CreateLobby *createlobby = (CreateLobby*)*payload;
            memcpy(&(createlobby->max_players), buffer, sizeof(createlobby->max_players));
            createlobby->max_players = ntohl(createlobby->max_players);
            memcpy(createlobby->name, buffer + sizeof(createlobby->max_players), sizeof(createlobby->name));
            return sizeof(createlobby->max_players) + sizeof(createlobby->name);
        case JOIN_LOBBY:
            *payload = malloc(sizeof(JoinLobby));
            if (*payload == NULL) return 0;

            JoinLobby *joinlobby = (JoinLobby*)*payload;
            memcpy(&(joinlobby->lobby_id), buffer, sizeof(joinlobby->lobby_id));
            joinlobby->lobby_id = ntohl(joinlobby->lobby_id);
            return sizeof(joinlobby->lobby_id);
        case CHANGE_RULES:
            // TODO
            break;
        case SEND_RESPONSE:
            *payload = malloc(sizeof(SendResponse));
            if (*payload == NULL) return 0;

            SendResponse *sendresponse = (SendResponse*)*payload;
            memcpy(sendresponse->response, buffer, sizeof(sendresponse->response));
            return sizeof(sendresponse->response);
        case PLAYER_JOINED:
            *payload = malloc(sizeof(PlayerJoined));
            if (*payload == NULL) return 0;

            PlayerJoined *playerjoined = (PlayerJoined*)*payload;
            memcpy(&(playerjoined->player_id), buffer, sizeof(playerjoined->player_id));
            playerjoined->player_id = ntohl(playerjoined->player_id);
            memcpy(playerjoined->name, buffer + sizeof(playerjoined->player_id), sizeof(playerjoined->name));
            return sizeof(playerjoined->player_id) + sizeof(playerjoined->name);
        case PLAYER_QUIT:
            *payload = malloc(sizeof(PlayerQuit));
            if (*payload == NULL) return 0;

            PlayerQuit *playerquit = (PlayerQuit*)*payload;
            memcpy(&(playerquit->player_id), buffer, sizeof(playerquit->player_id));
            playerquit->player_id = ntohl(playerquit->player_id);
            return sizeof(playerquit->player_id);
        case RULES_CHANGED:
            // TODO
            break;
        case ANSWER_SENT:
            *payload = malloc(sizeof(AnswerSent));
            if (*payload == NULL) return 0;

            AnswerSent *answersent = (AnswerSent*)*payload;
            memcpy(answersent->answer, buffer, sizeof(answersent->answer));
            return sizeof(answersent->answer);
        case GAME_ENDED:
            *payload = malloc(sizeof(GameEnded));
            if (*payload == NULL) return 0;

            GameEnded *gameended = (GameEnded*)*payload;
            memcpy(&(gameended->winner_id), buffer, sizeof(gameended->winner_id));
            gameended->winner_id = ntohl(gameended->winner_id);
            return sizeof(gameended->winner_id);
        case PLAYER_RESPONSE_CHANGED:
            *payload = malloc(sizeof(PlayerResponseChanged));
            if (*payload == NULL) return 0;

            PlayerResponseChanged *prespchanged = (PlayerResponseChanged*)*payload;
            memcpy(&(prespchanged->player_id), buffer, sizeof(prespchanged->player_id));
            prespchanged->player_id = ntohl(prespchanged->player_id);
            offset += sizeof(prespchanged->player_id);
            memcpy(&(prespchanged->points_earned), buffer + offset, sizeof(prespchanged->points_earned));
            prespchanged->points_earned = ntohl(prespchanged->points_earned);
            offset += sizeof(prespchanged->points_earned);
            memcpy(&(prespchanged->is_correct), buffer + offset, sizeof(prespchanged->is_correct));
            prespchanged->is_correct = ntohl(prespchanged->is_correct);
            offset += sizeof(prespchanged->is_correct);
            memcpy(prespchanged->response, buffer + offset, sizeof(prespchanged->response));
            offset += sizeof(prespchanged->response);
            return offset;
        case LOBBYLIST:
            *payload = malloc(sizeof(LobbyList));
            if (*payload == NULL) return 0;

            LobbyList *lobbylist = (LobbyList*)*payload;
            memcpy(&(lobbylist->__quantity), buffer, sizeof(lobbylist->__quantity));
            lobbylist->__quantity = ntohl(lobbylist->__quantity);

            memcpy(&(lobbylist->ids), buffer + sizeof(lobbylist->__quantity), sizeof(lobbylist->ids));
            for (int i = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
                (lobbylist->ids)[i] = ntohl((lobbylist->ids)[i]);
            } 

            memcpy(&(lobbylist->names), buffer + sizeof(lobbylist->__quantity) + sizeof(lobbylist->ids), sizeof(lobbylist->names));

            return sizeof(lobbylist->__quantity) + sizeof(lobbylist->ids) + sizeof(lobbylist->names);
        case SUCCESS:
            *payload = malloc(sizeof(Success));
            if (*payload == NULL) return 0;
    
            Success *success = (Success*)*payload;
            memcpy(&(success->data), buffer, sizeof(success->data));
            success->data = ntohl(success->data);
            return sizeof(success->data);
        case ERROR:
            *payload = malloc(sizeof(Error));
            if (*payload == NULL) return 0;

            Error *error = (Error*)*payload;
            memcpy(&(error->data), buffer, sizeof(error->data));
            error->data = ntohl(error->data);
            return sizeof(error->data);
        // All other Messages don't have a payload therefore their payload size is 0.
        default:
            return 0;
    }

    return 0;
}

// Helper function.
// Serializes a QuestionSent payload into the buffer. The buffer needs to already be initialized and have sufficient space
// for copying the serialized data onto it.
// Returns the amount of bytes copied to the buffer.
int __serialize_question_sent(QuestionSent *payload, uint8_t *buffer) {
    
    uint32_t net_support_type = htonl(payload->support_type);
    memcpy(buffer, &net_support_type, sizeof(net_support_type));

    size_t offset = sizeof(net_support_type);

    memcpy(buffer + offset, payload->question, sizeof(payload->question));
    offset += sizeof(payload->question);

    // We add one to copy the NULL Character (\0)
    size_t payload_length = strlen(payload->support) + 1;
    memcpy(buffer + offset, payload->support, payload_length);

    return offset + payload_length;
}

// Helper function.
// Deserializes a QuestionSent buffer into `payload`.
// Returns the amount a bytes copied to the payload.
int __deserialize_question_sent(uint8_t *buffer, void **payload) {

    *payload = malloc(sizeof(QuestionSent));
    if (*payload == NULL) return 0;
    QuestionSent *qsent = (QuestionSent*)*payload;

    memcpy(&(qsent->support_type), buffer, sizeof(qsent->support_type));
    qsent->support_type = ntohl(qsent->support_type);

    size_t offset  = sizeof(qsent->support_type);

    memcpy(qsent->question, buffer + offset, sizeof(qsent->question));

    offset += sizeof(qsent->question);

    size_t support_length = strlen((char*)(buffer + offset));
    char *support = (char*)malloc(sizeof(char)*support_length);
    if (support == NULL) {
        free(qsent);
        return 0;
    }

    memcpy(support, buffer + offset, support_length);
    qsent->support = support;

    return offset + support_length; 
}

// Helper function.
// Serializes a PlayersData payload into the buffer. The buffer needs to already be initialized and have sufficient space
// for copying the serialized data onto it.
// Returns the amount of bytes copied to the buffer.
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
int __serialize_players_data(PlayersData *payload, uint8_t *buffer) {
    uint32_t net_number_of_players = htonl(payload->number_of_players);
    memcpy(buffer, &net_number_of_players, sizeof(net_number_of_players));

    size_t offset = sizeof(net_number_of_players);

    memcpy(buffer + offset, payload->player_names, payload->number_of_players*MAX_USERNAME_LENGTH);
    offset += payload->number_of_players*MAX_USERNAME_LENGTH;

    uint32_t *net_players_id = (uint32_t*)malloc(sizeof(uint32_t)*payload->number_of_players);
    uint32_t *net_player_points = (uint32_t*)malloc(sizeof(uint32_t)*payload->number_of_players);

    if ((net_player_points == NULL) || (net_players_id == NULL)) {
        errno = 1;
        return 0;
    }

    for (int i = 0; i < payload->number_of_players; i++) {
        net_players_id[i] = htonl(payload->players_id[i]);
        net_player_points[i] = htonl(payload->player_points[i]);
    }

    memcpy(buffer + offset, net_players_id, sizeof(uint32_t)*payload->number_of_players);
    offset += sizeof(uint32_t)*payload->number_of_players;

    memcpy(buffer + offset, net_player_points, sizeof(uint32_t)*payload->number_of_players);
    offset += sizeof(uint32_t)*payload->number_of_players;

    free(net_players_id);
    free(net_player_points);

    return offset;

}

// Helper function.
// Deserializes a PlayersData buffer into `payload`.
// Returns the amount of bytes copied to the payload.
int __deserialize_players_data(uint8_t *buffer, void **payload) {
    *payload = malloc(sizeof(PlayersData));
    if (*payload == NULL) return 0;
    PlayersData *pdata = (PlayersData*)*payload;

    memcpy(&(pdata->number_of_players), buffer, sizeof(pdata->number_of_players));
    pdata->number_of_players = ntohl(pdata->number_of_players);

    size_t offset = sizeof(pdata->number_of_players);

    pdata->player_names = malloc(sizeof(char)*pdata->number_of_players*MAX_USERNAME_LENGTH);
    memcpy(pdata->player_names, buffer + offset, pdata->number_of_players*MAX_USERNAME_LENGTH);
    
    offset += sizeof(char) * (pdata->number_of_players*MAX_USERNAME_LENGTH);

    pdata->players_id = (int*)malloc(sizeof(int)*pdata->number_of_players);
    pdata->player_points = (int*)malloc(sizeof(int)*pdata->number_of_players);

    if ((pdata->players_id == NULL) || (pdata->player_points == NULL)) {
        free(pdata->players_id); // Safe to do on potentially NULL pointers
        free(pdata->player_points);
        free(pdata);
        return 0;
    }

    memcpy(pdata->players_id, buffer + offset, sizeof(int)*pdata->number_of_players);

    offset += sizeof(int)*pdata->number_of_players;

    memcpy(pdata->player_points, buffer + offset, sizeof(int)*pdata->number_of_players);

    offset += sizeof(int)*pdata->number_of_players;

    for (int i = 0; i < pdata->number_of_players; i++) {
        pdata->players_id[i] = ntohl(pdata->players_id[i]);
        pdata->player_points[i] = ntohl(pdata->player_points[i]);
    }

    return offset;
}

// Frees a Message 
void free_message(Message *m) {
    if (__payload_is_fixed_size(m->type)) {
        if (m->payload_size != 0) free(m->payload);
    }

    switch (m->type) {
        case QUESTION_SENT:
            QuestionSent *qsent = (QuestionSent*)m->payload;
            free(qsent->support);
            free(qsent);
            break;
        case PLAYERS_DATA:
            PlayersData *pdata = (PlayersData*)m->payload;
            free(pdata->player_names);
            free(pdata->player_points);
            free(pdata->players_id);
            free(pdata);
            break;
        default:
            break;
    }

    free(m);
}

// Returns a Message from a payload and the player's uuid.
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
Message *payload_to_message(MessageType type, void *payload, uint32_t uuid) {
    Message *m = (Message*)malloc(sizeof(Message));

    if (m == NULL) {
        errno = 1;
        return NULL;
    }

    m->type = type;
    m->uuid = uuid;
    m->payload_size = __get_payload_size(type, payload);
    m->payload = payload;

    return m;
}

// Serializes a message into the buffer and also returns the buffer's size into `buffer_size`. 
// The buffer has to be uninitialized.
// Returns 0 when successful, and a non zero value when an error occurs.
int serialize_message(Message *message, uint8_t **buffer, uint32_t *buffer_size) {
    *buffer_size = 4 + sizeof(Message) - sizeof(message->payload) + message->payload_size;
    
    // The '4+' is to put the length of the message in bytes.
    *buffer = (uint8_t*)malloc(*buffer_size);

    if (*buffer == NULL) {
        return 1;
    }

    // We need to write the size of the message at the beginning of the buffer.
    int msize = htonl(sizeof(Message) - sizeof(message->payload) + message->payload_size);
    memcpy(*buffer, &msize, sizeof(msize));

    size_t offset = sizeof(msize);

    // Type
    // MessageType is guaranteed to be 4 bytes long thanks to #pragma enum(4)
    uint32_t net_type = htonl(message->type);
    memcpy(*buffer + offset, &net_type, sizeof(net_type));

    offset += sizeof(net_type);

    // UUID
    uint32_t net_uuid = htonl(message->uuid);
    memcpy(*buffer + offset, &net_uuid, sizeof(net_uuid));
    offset += sizeof(net_uuid);

    // Payload size
    uint32_t net_payload_size = htonl(message->payload_size);
    memcpy(*buffer + offset, &net_payload_size, sizeof(net_payload_size));
    offset += sizeof(net_payload_size);

    // Payload
    switch (message->type) {
        case QUESTION_SENT:
            offset +=  __serialize_question_sent((QuestionSent*)message->payload, *buffer + offset);
            break;
        case PLAYERS_DATA:
            offset += __serialize_players_data((PlayersData*)message->payload, *buffer + offset);
            break;
        // Payload has fixed size due to it's type.
        default:
            offset += __serialize_fixed_size_payload(message->type, message->payload, *buffer + offset);
            break;
    } 

    (*buffer)[offset] = EOT;

    return 0;
    
}

// Sends a message `buffer` throught the socket file descriptor `sockfd`.
// You also need to provide the length of the buffer.
// If the server responds to the message, the reponse will be stored inside `response`. This parameter does not need to be initialized beforehand.
// If `response == NULL`, the message that is sent doesn't expect a response.
// Returns 0 when successful, and a non zero value when an error occurs.
int send_message(int sockfd, uint8_t *buffer, uint32_t buffer_size, Message **response) {
    // This function needs to handle the response given by the server to particular events
    if (buffer == NULL) {
        return 1;
    }

    write(sockfd, buffer, buffer_size);

    // The message doesn't expect a response so we can safely quit.
    if (response == NULL) return 0;


    uint32_t rbuffer_size;
    uint8_t *rbuffer = receive_message(sockfd, &rbuffer_size, MAX_PAYLOAD_LENGTH);

    if (errno != 0) {
        if (rbuffer != NULL) free(rbuffer);
        return errno;
    }

    *response = deserialize_message(rbuffer);

    if (errno != 0) {
        if (*response != NULL) free_message(*response);
        return errno;
    }

    free(rbuffer);

    return 0;
}

// Waits for a message to be sent on the socket `sockfd` and returns it. This function is blocking.
// The message buffer's size is returned as part of the `buffer_size` parameter.
// If the message contains more bytes than max_buffer_size, the whole message is thrown away and an error is thrown.
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
uint8_t *receive_message(int sockfd, uint32_t *buffer_size, uint32_t max_buffer_size) {
    // int total_char_read = 0;
    // int readbuffer_size = READBUFFER_LENGTH;
    // uint8_t *readbuffer = malloc(sizeof(uint8_t)*READBUFFER_LENGTH);

    // uint8_t char_buffer;
    // while ((uint32_t)total_char_read < max_buffer_size) {
    //     int nb_read = read(sockfd, &char_buffer, 1);

    //     if (nb_read == -1) {
    //         errno = 1;
    //         *buffer_size = 0;
    //         free(readbuffer);
    //         return NULL;
    //     }

    //     else if (nb_read == 0) {
    //         *buffer_size = 0;
    //         free(readbuffer);
    //         return NULL;
    //     }

    //     // At the end of every message we send an EOT character
    //     else if (char_buffer == EOT) {
    //         break;
    //     }

    //     total_char_read++;

    //     // Reallocation procedure. It's better to do this than to use realloc because of the way realloc works
    //     // https://linux.die.net/man/3/realloc
    //     if (total_char_read > readbuffer_size) {
    //         uint8_t *_tmp = (uint8_t*)malloc(sizeof(uint8_t)*( (int)(readbuffer_size*1.5) ));

    //         if (_tmp == NULL) {
    //             errno = 2;
    //             free(readbuffer);
    //             return NULL;
    //         }

    //         memcpy((char*)_tmp, (char*)readbuffer, total_char_read - 1);
    //         readbuffer_size = (int)(readbuffer_size*1.5);

    //         free(readbuffer);
    //         readbuffer = _tmp;
    //     }

    //     readbuffer[total_char_read - 1] = char_buffer;
    // }

    // *buffer_size = total_char_read;
    // return readbuffer;
    *buffer_size = 0;
    uint32_t net_msg_len;
    ssize_t n;

    // Step 1: Read 4-byte length header
    uint8_t *p = (uint8_t *)&net_msg_len;
    size_t read_len = 0;
    while (read_len < sizeof(net_msg_len)) {
        n = read(sockfd, p + read_len, sizeof(net_msg_len) - read_len);
        if (n == 0) {
            *buffer_size = 0;
            return NULL;
        }

        if (n < 0) {
            errno = 1;
            *buffer_size = 0;
            return NULL;
        }
        read_len += n;
    }

    uint32_t msg_len = ntohl(net_msg_len);  // Convert to host byte order

    if (msg_len > max_buffer_size) {
        errno = EMSGSIZE;
        return NULL;
    }

    // Step 2: Allocate buffer and read the full message
    uint8_t *buffer = malloc(msg_len);
    if (!buffer) {
        errno = ENOMEM;
        return NULL;
    }

    read_len = 0;
    while (read_len < msg_len) {
        n = read(sockfd, buffer + read_len, msg_len - read_len);
        if (n < 0) {
            free(buffer);
            return NULL;
        }
        read_len += n;
    }

    *buffer_size = msg_len;
    return buffer;
}

// Deserializes a serialized message and returns a structure that can be read by the program.
// Use errno for error detection. errno == 0 when successful, and a non zero value when an error occured. 
Message *deserialize_message(uint8_t *buffer) {
    // Type
    uint32_t type;
    memcpy(&type, buffer, sizeof(type));
    type = ntohl(type);

    size_t offset = sizeof(type);

    // UUID
    uint32_t uuid;
    memcpy(&uuid, buffer + offset, sizeof(uuid));
    uuid = ntohl(uuid);

    offset += sizeof(uuid);

    // Payload size
    uint32_t payload_size;
    memcpy(&payload_size, buffer + offset, sizeof(payload_size));
    payload_size = ntohl(payload_size);

    offset += sizeof(payload_size);

    void *payload;
    int bytes_copied;
    switch (type) {
        case QUESTION_SENT:
            bytes_copied = __deserialize_question_sent(buffer + offset, &payload);
            break;
        case PLAYERS_DATA:
            bytes_copied = __deserialize_players_data(buffer + offset, &payload);
            break;
        // In every other case the payload has fixed size
        default:
            bytes_copied = __deserialize_fixed_size_payload(type, buffer + offset, &payload);
            break;
    }

    // If didn't copy the same amount of bytes
    if ((uint32_t)bytes_copied != payload_size) {
        errno = 2;
        return NULL;
    }

    Message *m = (Message*)malloc(sizeof(Message));
    
    if (m == NULL) {
        errno = 1;
        return NULL;
    }

    m->type = type;
    m->uuid = uuid;
    m->payload_size = payload_size;
    m->payload = payload;

    return m;
}

Message *responsecode_to_message(ResponseCode rc, int uuid, int data) {
    Message *m = (Message*)malloc(sizeof(Message));

    if (m == NULL) {
        errno = 1;
        return NULL;
    }

    void *payload = NULL;
    if (rc == RC_SUCCESS) {
        payload = malloc(sizeof(Success));
    } else {
        payload = malloc(sizeof(Error));
    }

    if ( payload == NULL) {
        errno = 2;
        free(m);
        return NULL;
    }
    if (rc == RC_SUCCESS) ((Success*)payload)->data = data; else ((Error*)payload)->data = data; 

    m->uuid = uuid;
    m->type = rc == RC_SUCCESS ? SUCCESS : ERROR;
    m->payload_size = __get_payload_size(m->type, payload);
    m->payload = payload;

    return m;
}