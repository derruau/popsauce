#include "main.h"

int listen_socket;
// This array contains all the client threads, one for each player.
pthread_t  client_threads[MAX_NUMBER_OF_PLAYERS] = {0};
// This array contains all the send threads, one for each lobby.
pthread_t send_threads[MAX_NUMBER_OF_LOBBIES] = {0};
//
int available_client_threads[MAX_NUMBER_OF_PLAYERS] = {0};

int server_online = 1; // If set to zero, the server closes
int accept_connections = 1; // If set to zero, the server stops accepting connections

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
        if (send_queue_empty == 1) {
            struct timespec ts = {0, 1000000L}; // 1ms
            nanosleep(&ts, &ts);
            continue;
        }

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
        printf("[ERROR]: Cannot create socket.\n");
        exit(EXIT_FAILURE);
    };
    
    listen_socket.sin_family = AF_INET;
    listen_socket.sin_addr.s_addr = INADDR_ANY;
    listen_socket.sin_port = htons(_port);

    // If program crashes without closing the sockets, can rebind it immediatly.
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    int errors = bind(listen_fd,  (struct sockaddr *)&listen_socket, sizeof(listen_socket));
    if (errors < 0) {
        printf("[ERROR]: Cannot bind socket to port %s.\n", port);
        close(listen_fd);
        exit(EXIT_FAILURE);
    };
    
    errors = listen(listen_fd, 5);
    if (errors < 0) {
        printf("[ERROR]: Cannot listen on port %s.\n", port);
        close(listen_fd);
        exit(EXIT_FAILURE);
    };

    return listen_fd;
}

void INThandler(int sig) {
    if (sig != SIGINT) return;
    if (server_online == 0) {
        printf("[SERVER]: Server already stopping...\n");
        return;
    }

    char  c;

    Message *lobby_list;
    get_lobby_list(&lobby_list);
    LobbyList *ll = (LobbyList*)lobby_list->payload;
    printf("\r Do you really want to quit (%i lobbies active)? [y/N] ", ll->__quantity);
    free_message(lobby_list);
    fflush(stdout);

    c = getchar();
    if (c == 'y' || c == 'Y') {
        printf("[SERVER]: Server stopping...\n");

        accept_connections = 0;
        for (int i = 0; i < MAX_NUMBER_OF_PLAYERS; i++) {
            if (players[i] != NULL) {
                // By doing this, we also delete all lobbies
                delete_player(players[i]->player_id, NULL);
                printf("[SERVER]: Player '%s' disconnected.\n", players[i]->username);
            }
        }

        for (int i=0; i < MAX_NUMBER_OF_PLAYERS; i++) {
            if (available_client_threads[i] != 0) {
                pthread_cancel(client_threads[available_client_threads[i]]);
                pthread_join(client_threads[available_client_threads[i]], NULL);
                available_client_threads[i] = 0;
                printf("[SERVER]: Client thread %d stopped.\n", i);
            }
                
            server_online = 0;
        }
    
        close(listen_socket);
        exit(EXIT_SUCCESS);
    }
    else {
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

    printf("[SERVER]: Sending message of type %i to socket %i\n", m->type, socket);
    free_message(m);

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
        
        // This happens because of a sudden disconnect from the client
        if (buffer_size == 0) {
            if (errno != 0) {
                printf("[WARNING]: Connection suddently closed with thread ID: %i\n", a->connection_id);
            } else {
                printf("[INFO]: Connection closed with thread ID: %i\n", a->connection_id);
            }

            int player_id = get_player_id_from_socket(a->socket);
            if (player_id != -1) {
                Message *playerquit;
                int lobby_id = get_lobby_of_player(player_id);
                delete_player(player_id, &playerquit);
                if (lobby_id != -1 && playerquit != NULL) {
                    lobby_enqueue(lobby_id, SEND_QUEUE, playerquit, NO_SOCKET);
                }
            }
            break;
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
            printf("[SERVER]: Received CONNECT Message from thread ID %i: (Username: '%s')\n", a->connection_id, c->username);
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

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    available_client_threads[a->connection_id] = 0;
    close(a->socket);
    free(buffer);
    free(a);
    pthread_exit(NULL);
}

void print_help() {
    printf(
        "POPSAUCE-SERVER - Usage:\n"
        "   - ./popsauce-server: starts the server\n"
        "   - ./popsauce-server help: prints this help message\n"
        "   - ./popsauce-server add [txt/img]: adds a new question to the database.\n"
        "             This command starts a menu to add a question.\n"
        "             'txt' for text style support and 'img' for image format support.\n"
        "             IMPORTANT: 'img' doesn't work for now!\n"
        "\n"
        "More options incoming..."
    );
}

int add_question(int argc, char *argv[]) {
    if (argc != 3 || (strcmp(argv[1], "add") != 0)) return 1;

    SupportType support_type = STRING;
    if (strcmp(argv[2], "img") == 0) support_type = BITMAP;

    Question *question = (Question*)malloc(sizeof(Question));
    if (question == NULL) {
        exit(EXIT_FAILURE);
    }

    printf("Adding a new question to the database:\n");

    init_database(DATABASE_PATH);
    
    question->support_type = support_type;
    printf("Enter the question you want to add: ");
    fgets(question->question, MAX_QUESTION_LENGTH, stdin);

    char support_buffer[2048];
    if (support_type == BITMAP) {
        printf("Enter the path to the image you want to use as support: ");
        fgets(support_buffer, 2048, stdin);
        // Remove newline character
        size_t len = strlen(support_buffer);
        if (len > 0 && support_buffer[len - 1] == '\n') {
            support_buffer[len - 1] = '\0';
        }
        size_t img_size;
        uint8_t *img_buffer = __read_image_to_buffer(support_buffer, &img_size);
        free(img_buffer);
        
        if(img_size > MAX_PAYLOAD_LENGTH) {
            printf("[ERROR]: Image size exceeds the maximum payload length of %d bytes.\n", MAX_PAYLOAD_LENGTH);
            free(question);
            return 1;
        }
    } else {
        printf("Enter the text support to the question: ");
        fgets(support_buffer, 2048, stdin);
    }
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

    int ok = insert_question(DATABASE_PATH, question, number_of_valid_answers);

    return ok;
}

int main(int argc, char *argv[]) {
    if (argc == 3) {
        add_question(argc, argv);
        return 0;
    } else if (argc > 1) {
        if (strcmp(argv[1], "help") == 0) {
            print_help();
            return 0;
        }
    }

    struct sockaddr_in client_socket;

    init_game_mutexes();

    printf("[SERVER]: Initializing Database...\n");
    init_database(DATABASE_PATH);
    printf("[SERVER]: Database Initialized...\n");

    int listen_fd = server_listen(PORT);
    printf("[SERVER]: Server Listening on port '%s'\n", PORT);

    signal(SIGINT, INThandler);
    
    while (server_online) {
        unsigned int client_socket_size = sizeof(client_socket);
        if (!accept_connections) continue;

        int socket = accept(listen_fd, (struct sockaddr*)&client_socket, &client_socket_size);
        if (socket < 0) continue;

        int available_thread = getavailable_client_threads();
        if (available_thread == -1) {
            printf("[ERROR]: Cannot accept another connection: server full!\n");
            continue;
        }

        printf("[INFO]: New connection %s:%hu assignated to client thread ID: %i\n",
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

    exit(EXIT_SUCCESS);
}