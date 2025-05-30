/*

TODO: Important: Put every action that is sent to the server in a queue and execute them in order

J'ai besoin de:
- Un moyen de se connecter et gérer les connections
    - Server multithread avec broadcast
- Un moyen de choisir des questions
    - Une connection à sqlite qui cherche dans la db de question
- Un moyen de communiquer avec le(s) client(s).
    - Protocol de communicatigetavailable_client_threadson et fonctions pour faciliter ça.


Usage:
    - ./popsauce-server start
    - ./popsauce-server add [txt/img] 
        - Ça fait entrer dans un menu pour ajouter la question:
        - Intitulé de la question
        - Support de la question
        - Réponses acceptées (séparé par une virgule)
        - Les réponses sont checkées par une fonction de hachage qui transforme 
    - ./popsauce-server list
    - ./popsauce-server rm [ID]
*/

#include "main.h"

char *stringIP(uint32_t entierIP) {
    struct in_addr ia;
    ia.s_addr = htonl(entierIP); 
    return inet_ntoa(ia);
  }

// Sends a message to every client of a lobby at once.
// Retuns 0 when successfull and a nonzero value when an error occured
int broadcast(Message *m, int lobby_id) {
    int retval = 0;

    uint32_t buffer_size;
    uint8_t *buffer;

    int ok = serialize_message(m, &buffer, &buffer_size);

    if (ok != 0) {
        free_message(m);
        m = responsecode_to_message(EC_INTERNAL_ERROR, SERVER_UUID, EC_INTERNAL_ERROR);
        ok = serialize_message(m, &buffer, &buffer_size);
        // IDK How we got here
        if (ok != 0) exit(EXIT_FAILURE);
    }

    int players_in_lobby;
    Player **plist = get_players_from_lobby(lobby_id, &players_in_lobby);
    for (int i = 0; i < players_in_lobby; i++) {
        retval |= send_message(plist[i]->socket, buffer, buffer_size, NULL);   
    }

    return retval;
}


void *handle_server_broadcast(void *args) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    int lobby_id = *((int*)args);

    while (1) {
        int send_queue_empty = lobby_mq_is_empty(lobby_id, SEND_QUEUE);
        if (send_queue_empty == -1) break; // Lobby doesn't exist anymore
        if (send_queue_empty == 1) continue;

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        
        MessageQueueItem *mqi = lobby_dequeue(lobby_id, SEND_QUEUE);

        uint32_t buffer_size;
        uint8_t *buffer;
        int err = serialize_message(mqi->m, &buffer, &buffer_size);

        // When an error occures, skip the message but don't crash.
        // We don't need to free buffer because a nonzero value only appears when
        // malloc on buffer failed.

        if (err != 0) {
            continue;
        }

        // Even if there's an error here we can't do anything about it so we don't check
        send_message(mqi->socket, buffer, buffer_size, NULL);

        free(buffer);
        free_message(mqi->m);
        
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    }

    pthread_exit(NULL);
}

int server_listen(char *port) {
    struct sockaddr_in listen_socket;
    uint16_t _port = (uint16_t)atoi(port);
  
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        // TODO: error can't create a socket
    };
    
    listen_socket.sin_family = AF_INET;
    listen_socket.sin_addr.s_addr = INADDR_ANY;
    listen_socket.sin_port = htons(_port);

    int errors = bind(listen_fd,  (struct sockaddr *)&listen_socket, sizeof(listen_socket));
    if (errors < 0) {
        // TODO: error cannot bind
    };
    
    errors = listen(listen_fd, 5);
    if (errors < 0) {
        // TODO: error fail to listen
    };

    return listen_fd;
}

void INThandler(int sig) {
    char  c;

    signal(sig, SIG_IGN);
    printf("\x1b""7\r Do you really want to quit?? [y/N] ");
    fflush(stdout);
    c = getchar();
    if (c == 'y' || c == 'Y') {
        server_online = 0;
        printf("[SERVER]: Server stopping...\n");
        // On ferme bien toutes les connections avant de fermer le serveur
        for (int i=0; i < MAX_NUMBER_OF_PLAYERS; i++) {
            if (available_client_threads[i] != 0) {
                pthread_cancel(client_threads[available_client_threads[i]]);
            }
        }
    
        close(listen_socket);
        exit(EXIT_SUCCESS);
    }
    else {
        printf("\x1b""8\r\x1b[0J");
        signal(SIGINT, INThandler);
    }
    return;
}

int getavailable_client_threads() {
    for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        if (available_client_threads[i] == 0) {
            return i;
        }
    }

    return -1;
}

// Tries to send a message with a lot of failsafe mechanisms.
// When something fails, this function still tries to send a EC_INTERNAL_ERROR message.
// When even this fails the server crashes because something went very wrong.
int safe_send_message(int socket, Message *m) {
    uint8_t *buffer;
    uint32_t buffer_size;
    int ok = serialize_message(m, &buffer, &buffer_size);

    if (ok != 0) {
        free_message(m);
        m = responsecode_to_message(EC_INTERNAL_ERROR, SERVER_UUID, EC_INTERNAL_ERROR);
        ok = serialize_message(m, &buffer, &buffer_size);

        // IDK how we even got here
        if (ok != 0) exit(EXIT_FAILURE);
    }

    return send_message(socket, buffer, buffer_size, NULL);
}

void *handle_client_thread(void *arg) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    session_thread_args *a = (session_thread_args*)arg;

    uint32_t buffer_size;
    uint8_t *buffer;
    while(1) {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        buffer = receive_message(a->socket, &buffer_size, MAX_PAYLOAD_LENGTH);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        if (errno != 0) {
            free(buffer);
            continue;
        }
            
        // This happens because of a sudden disconnect from the client
        if (buffer_size == 0) {
            int player_id = get_player_id_from_socket(a->socket);
            if (player_id != -1) {
                Message *playerquit;
                delete_player(player_id, &playerquit);
                safe_send_message(a->socket, playerquit);
            }
            close(a->socket);
            free(buffer);
            free(a);
            pthread_exit(NULL);
        }

        Message *m = deserialize_message(buffer);

        // TODO: implement private server and banned users (using ip addresses) 
        // Put the functions that check for that here

        ResponseCode rc;
        Message *response;
        int lobby_id;
        switch (m->type) {
        case CONNECT:
            Connect *c = (Connect*)m->payload;
            rc = create_player(a->socket, m->uuid, c->username);
            if (rc == RC_SUCCESS) {
                rc = get_lobby_list(&response);

                // get_lobby_list() doesn't leak when it fails so we don't have to free anything
                if (rc != RC_SUCCESS) response = responsecode_to_message(EC_INTERNAL_ERROR, SERVER_UUID, EC_INTERNAL_ERROR);
            } else {
                response = responsecode_to_message(rc, SERVER_UUID, rc);
            }

            safe_send_message(a->socket, response);
            break;
        case DISCONNECT:
            rc = delete_player(m->uuid, &response);

            if (rc != RC_SUCCESS) {
                free_message(response);
                response = responsecode_to_message(rc, SERVER_UUID, rc);
            }

            lobby_id = get_lobby_of_player(m->uuid);
            if (response != NULL) lobby_enqueue(lobby_id, SEND_QUEUE, response, NO_SOCKET);
            break;
        case CREATE_LOBBY:
            CreateLobby *cl = (CreateLobby*)m->payload;
            rc = create_lobby(cl->name, cl->max_players, m->uuid, &lobby_id);

            response = responsecode_to_message(rc, SERVER_UUID, lobby_id);

            if (rc == RC_SUCCESS) {
                pthread_create(&send_threads[lobby_id], NULL, handle_server_broadcast, (void*)&lobby_id);
                set_lobby_send_thread(lobby_id, &send_threads[lobby_id]);
            }

            // Cannot recover from this error, crashes the server.
            if (errno != 0) exit(EXIT_FAILURE);

            safe_send_message(a->socket, response);
            break;
        case JOIN_LOBBY:
            JoinLobby *jl = (JoinLobby*)m->payload;
            Message *playerjoined;
            rc = join_lobby(m->uuid, jl->lobby_id, &playerjoined);

            lobby_id = get_lobby_of_player(m->uuid);
            
            if (rc != RC_SUCCESS) {
                free_message(playerjoined);
                response = responsecode_to_message(rc, SERVER_UUID, rc);
                safe_send_message(a->socket, response);
                break;
            } else get_players_data(lobby_id, &response);

            safe_send_message(a->socket, response);

            int lobby_id = get_lobby_of_player(m->uuid);
            lobby_enqueue(lobby_id, SEND_QUEUE, playerjoined, NO_SOCKET);
            break;
        case QUIT_LOBBY:
            Message *playerquit;
            rc = quit_lobby(m->uuid, &playerquit);

            lobby_id = get_lobby_of_player(m->uuid);

            // TODO: maybe add a confirmation message that you successfully quit?
            if (rc != RC_SUCCESS) {
                free_message(playerquit);
                break;
            } else lobby_enqueue(lobby_id, SEND_QUEUE, playerquit, NO_SOCKET);
            break;
        case START_GAME:
            lobby_id = get_lobby_of_player(m->uuid);
            rc = start_game(m->uuid, lobby_id);
            // The response is queued by game_loop()

            if (rc != RC_SUCCESS) {
                response = responsecode_to_message(rc, SERVER_UUID, rc);
                safe_send_message(a->socket, response);
                break;
            }
            break;
        case CHANGE_RULES:
            //TODO:
            break;
        case SEND_RESPONSE:
            lobby_id = get_lobby_of_player(m->uuid);
                        
            if (!can_submit_answers(lobby_id, m->uuid)) {
                response = responsecode_to_message(EC_CANNOT_SUBMIT, SERVER_UUID, EC_CANNOT_SUBMIT);
                safe_send_message(a->socket, response);
            }

            lobby_enqueue(lobby_id, RECEIVE_QUEUE, m, a->socket);
            break;
        default:
            //TODO
            // If this executes, the client sent a message only meant for servers to send to clients (bad)
            // those messages are: PLAYER_JOINED, PLAYER_QUIT, GAME_STARTS, RULES_CHANGED, PLAYER_RESPONSE_CHANGED
            // QUESTION_SENT, ANSWER_SENT and GAME_ENDED
            break;
        }
    }

    available_client_threads[a->connection_id] = 0;
    close(a->socket);
    free(buffer);
    free(a);
    pthread_exit(NULL);
}

// void print_help() {
//     printf(
//         "POPSAUCE-SERVER - Usage:\n"
//         "   - ./popsauce-server: starts the server\n"
//         "   - ./popsauce-server add [txt/img]: adds a new question to the database.\n"
//         "             This command starts a menu to add a question.\n"
//         "             'txt' for text style support and 'img' for image format support.\n"
//         "             IMPORTANT: 'img' doesn't work for now!\n"
//         "\n"
//         "More options incoming..."
//     );
// }

int main(int argc, char *argv[]) {
    if ( (argc == 3) && (strcmp(argv[1], "add") == 0)) {
        SupportType support_type = STRING;
        if (strcmp(argv[2], "img") == 0) support_type = BITMAP;

        Question *question = (Question*)malloc(sizeof(Question));
        if (question == NULL) {
            exit(EXIT_FAILURE);
        }

        init_database(DATABASE_PATH);
        
        question->support_type = support_type;
        printf("Enter the question: ");
        fgets(question->question, MAX_QUESTION_LENGTH, stdin);

        // Normally, this should be of size MAX_PAYLOAD_LENGTH
        char support_buffer[2048];
        printf("Enter the support to the question: ");
        fgets(support_buffer, 2048, stdin);
        question->support = malloc(sizeof(char)*(strlen(support_buffer) + 1));
        if (question->support == NULL) {
            exit(EXIT_FAILURE);
        }
        strncpy(question->support, support_buffer, strlen(support_buffer) + 1);

        char valid_answers_buffer[512];
        printf("Enter the possible answers to the question (separated by a comma): ");
        fgets(valid_answers_buffer, 512, stdin);
        size_t number_of_valid_answers;
        question->valid_answers = __parse_string(valid_answers_buffer, &number_of_valid_answers);
        question->number_of_valid_answers = number_of_valid_answers;

        insert_question(DATABASE_PATH, question, number_of_valid_answers);

        return 0;
    }

    struct sockaddr_in client_socket;

    init_mutexes();

    printf("[SERVER]: Initializing Database...\n");
    init_database(DATABASE_PATH);
    printf("[SERVER]: Database Initialized...\n");

    int listen_fd = server_listen(PORT);
    printf("[SERVER]: Server Listening on port '%s'\n", PORT);

    signal(SIGINT, INThandler);
    
    while (1) {
        unsigned int client_socket_size = sizeof(client_socket);
        int socket = accept(listen_fd, (struct sockaddr*)&client_socket, &client_socket_size);
        if (socket < 0) {
            continue;
        };

        int available_thread = getavailable_client_threads();
        if (available_thread == -1) {
            printf("[ERROR]: Cannot accept another connection: server full!\n");
            continue;
        }

        printf("[INFO]: New connection %s:%hu assignated to thread ID: %i\n",
            stringIP(ntohl(client_socket.sin_addr.s_addr)),
            ntohs(client_socket.sin_port),
            available_thread
        );

        session_thread_args *arg = malloc(sizeof(session_thread_args));
        arg->socket = socket;
        arg->connection_id = available_thread;

        int ret = pthread_create(&client_threads[available_thread], NULL, handle_client_thread, arg);
        if (ret != 0) {
            printf("[ERROR]: Cannot create client thread for IP: %s", stringIP(ntohl(client_socket.sin_addr.s_addr)));
        }

        available_client_threads[available_thread] = 1;

    }

    printf("[SERVER]: Server Stopping...\n");    
    // On ferme bien toutes les connections avant de fermer le serveur
    for (int i=0; i < MAX_NUMBER_OF_PLAYERS; i++) {
        if (available_client_threads[i] != 0) {
            pthread_join(client_threads[available_client_threads[i]], NULL);
        }
    }


    close(listen_socket);

    exit(EXIT_SUCCESS);
}