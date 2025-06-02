#include "lobby_screen.h"
#include "message_queue.h"

typedef struct {
    int number_of_players;
    char (*player_names)[MAX_USERNAME_LENGTH]; // Array of player names
    int *player_points; // Array of player points
    int *players_id; // Array of player IDs
    int *player_answered; // Array of player answered flags (1 if answered, 0 if not)
    char (*player_responses)[MAX_RESPONSE_LENGTH]; // Array of player responses
} TruePlayersData;

MessageQueue *mq;
TruePlayersData *tpdata;
ClientState lobby_state = CS_WAITING_ROOM;
char question_text[MAX_QUESTION_LENGTH];
char *question_support;
int question_support_type;
char question_answer[MAX_RESPONSE_LENGTH];

int get_id_from_public_id(TruePlayersData *tpdata, int public_id) {
    if (tpdata == NULL || tpdata->number_of_players <= 0) {
        return -1; // Error
    }

    for (int i = 0; i < tpdata->number_of_players; i++) {
        if (tpdata->players_id[i] == public_id) {
            return i; // Return the index of the player with the given public ID
        }
    }
    return -1; // Player not found
}

void *lobby_screen_thread(void *arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    ClientPlayer *p = (ClientPlayer*)arg;
    while (1) {
        uint32_t buffer_size;
        uint8_t *buffer = receive_message(p->socket, &buffer_size, MAX_PAYLOAD_LENGTH);
        if (buffer == NULL) {
            pthread_exit(NULL); 
        }
        Message *m = deserialize_message(buffer);
        if (m == NULL) {
            free(buffer);
            pthread_exit(NULL);
        }

        mq_enqueue(mq, m, NO_SOCKET);
    }
}

TruePlayersData *init_tpdata(PlayersData *pdata) {
    if (pdata == NULL) {
        return NULL; // Error
    }

    TruePlayersData *tpdata = malloc(sizeof(TruePlayersData));
    if (tpdata == NULL) {
        return NULL; // Error
    }

    tpdata->number_of_players = pdata->number_of_players;
    tpdata->player_names = malloc(pdata->number_of_players * MAX_USERNAME_LENGTH);
    tpdata->player_points = malloc(pdata->number_of_players * sizeof(int));
    tpdata->players_id = malloc(pdata->number_of_players * sizeof(int));
    tpdata->player_answered = malloc(pdata->number_of_players * sizeof(int));
    tpdata->player_responses = malloc(pdata->number_of_players * MAX_RESPONSE_LENGTH);

    if (tpdata->player_names == NULL || tpdata->player_points == NULL || 
        tpdata->players_id == NULL || tpdata->player_responses == NULL) {
        free(tpdata);
        return NULL; // Error
    }

    for (int i = 0; i < pdata->number_of_players; i++) {
        strncpy(tpdata->player_names[i], pdata->player_names[i], MAX_USERNAME_LENGTH);
        tpdata->players_id[i] = pdata->players_id[i];
        tpdata->player_points[i] = 0; // Initialize points to 0
        tpdata->player_answered[i] = 0; // Initialize answered flags to 0
        memset(tpdata->player_responses[i], 0, MAX_RESPONSE_LENGTH); // Initialize responses to empty
    }

    return tpdata;
}

int add_player_to_tpdata(TruePlayersData *tpdata, const char *name, int player_id) {
    if (tpdata == NULL || name == NULL) {
        return -1; // Error
    }

    for (int i = 0; i < tpdata->number_of_players; i++) {
        if (tpdata->players_id[i] == player_id) {
            return -1; // Player already exists
        }
    }

    tpdata->number_of_players++;
    tpdata->player_names = realloc(tpdata->player_names, tpdata->number_of_players * MAX_USERNAME_LENGTH);
    tpdata->players_id = realloc(tpdata->players_id, tpdata->number_of_players * sizeof(int));
    tpdata->player_points = realloc(tpdata->player_points, tpdata->number_of_players * sizeof(int));
    tpdata->player_answered = realloc(tpdata->player_answered, tpdata->number_of_players * sizeof(int));
    tpdata->player_responses = realloc(tpdata->player_responses, tpdata->number_of_players * MAX_RESPONSE_LENGTH);

    if (tpdata->player_names == NULL || tpdata->players_id == NULL || tpdata->player_points == NULL ||
        tpdata->player_answered == NULL || tpdata->player_responses == NULL) {
        return -1; // Error
    }

    strncpy(tpdata->player_names[tpdata->number_of_players - 1], name, MAX_USERNAME_LENGTH);
    tpdata->players_id[tpdata->number_of_players - 1] = player_id;
    tpdata->player_points[tpdata->number_of_players - 1] = 0; 
    tpdata->player_answered[tpdata->number_of_players - 1] = 0;
    memset(tpdata->player_responses[tpdata->number_of_players - 1], 0, MAX_RESPONSE_LENGTH);

    return 0; // Success
}

int remove_player_from_tpdata(TruePlayersData *tpdata, int player_id) {
    if (tpdata == NULL || tpdata->number_of_players <= 0) {
        return -1; // Error
    }

    for (int i = 0; i < tpdata->number_of_players; i++) {
        if (tpdata->players_id[i] == player_id) {
            // Shift players down
            for (int j = i; j < tpdata->number_of_players - 1; j++) {
                strncpy(tpdata->player_names[j], tpdata->player_names[j + 1], MAX_USERNAME_LENGTH);
                tpdata->players_id[j] = tpdata->players_id[j + 1];
                tpdata->player_points[j] = tpdata->player_points[j + 1];
                tpdata->player_answered[j] = tpdata->player_answered[j + 1];
                strncpy(tpdata->player_responses[j], tpdata->player_responses[j + 1], MAX_RESPONSE_LENGTH);
            }
            tpdata->number_of_players--;
            tpdata->player_names = realloc(tpdata->player_names, tpdata->number_of_players * MAX_USERNAME_LENGTH);
            tpdata->players_id = realloc(tpdata->players_id, tpdata->number_of_players * sizeof(int));
            tpdata->player_points = realloc(tpdata->player_points, tpdata->number_of_players * sizeof(int));
            tpdata->player_answered = realloc(tpdata->player_answered, tpdata->number_of_players * sizeof(int));
            tpdata->player_responses = realloc(tpdata->player_responses, tpdata->number_of_players * MAX_RESPONSE_LENGTH);

            return 0; // Success
        }
    }
    return -1; // Player not found
}

int update_player_response(TruePlayersData *tpdata, int player_id, int points_earned, char *response, int is_correct) {
    
    if (tpdata == NULL || tpdata->number_of_players <= 0) {
        return -1; // Error
    }

    for (int i = 0; i < tpdata->number_of_players; i++) {
        if (tpdata->players_id[i] == player_id) {
            // Update the player's response
            tpdata->player_points[i] += points_earned;
            tpdata->player_answered[i] = is_correct; // Mark as answered
            memset(tpdata->player_responses[i], 0, MAX_RESPONSE_LENGTH); // Clear previous response
            strncpy(tpdata->player_responses[i], response, MAX_RESPONSE_LENGTH);
            return 0; // Success
        }
    }
    return -1; // Player not found
}

ClientState handle_lobby_screen(ClientState cs, ClientPlayer *p, PlayersData **pdata) {
    FIELD *field[2];
	FORM  *form;
    WINDOW *left_window;
    WINDOW *right_window;
    pthread_t lobby_thread;
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    
    mq = mq_init();
    tpdata = init_tpdata(*pdata);
    pthread_create(&lobby_thread, NULL, lobby_screen_thread, (void*)p);

    if (cs != CS_WAITING_ROOM) {
        return CS_DISCONNECTED;
    }

    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    int left_cols = (int)(cols * 0.6);
    int right_cols = cols - left_cols - 1;
    left_window = newwin(rows, left_cols, 0, 0);
    right_window = newwin(rows, right_cols, 0, left_cols);

    box(left_window, 0, 0);
    box(right_window, 0, 0);

    wrefresh(left_window);
    wrefresh(right_window);
    refresh();


    WINDOW *question_window = derwin(left_window, 5, left_cols - 2, 1, 1);
    box(question_window, 0, 0);

    WINDOW *support_window = derwin(left_window, rows - 10, left_cols - 2, 6, 1);
    box(support_window, 0, 0);

    field[0] = new_field(1, left_cols - 7, 1, 1, 0, 0);
    field[1] = NULL;
    set_field_back(field[0], A_UNDERLINE); 
	field_opts_off(field[0], O_AUTOSKIP);


    WINDOW *answer_window = derwin(left_window, 3, left_cols - 2, rows - 4, 1);
    keypad(answer_window, TRUE);
    
    form = new_form(field);
    set_form_win(form, answer_window);
    WINDOW *sub = derwin(answer_window, 3, left_cols - 6, 0, 0);
    set_form_sub(form, sub);
    post_form(form);

    box(answer_window, 0, 0);

    wrefresh(question_window);
    wmove(support_window, 1, 1);
    wprintw(support_window, "If you're the owner of the lobby, Press ENTER to start the game!");
    wrefresh(support_window);
    wrefresh(answer_window);

    for (int i = 0; i < tpdata->number_of_players; i++) {
        wmove(right_window, i + 1, 1);
        wprintw(right_window, "%s - Points: %d\n\n", tpdata->player_names[i], tpdata->player_points[i]);
    }
    box(right_window, 0, 0);
    wrefresh(right_window);


    nodelay(answer_window, TRUE);
    int n_chars = 0;
    while (1) {

        int ch = wgetch(answer_window);

        switch (ch) {
        case ERR:   
            break;
        case KEY_BACKSPACE:
        case 127: // Handle backspace
        case 8:
            n_chars = n_chars - 1 < 0 ? 0 : n_chars - 1; 
            form_driver(form, REQ_DEL_PREV);
            break;
        case KEY_LEFT:
            form_driver(form, REQ_PREV_CHAR);
            break;
        case KEY_RIGHT:
            form_driver(form, REQ_NEXT_CHAR);        clear();
            break;
        case KEY_ENTER:
        case '\n':
            if (lobby_state == CS_WAITING_ROOM) {
                // We send a START_GAME message to the server
                Message *m = payload_to_message(START_GAME, NULL, p->player_id);
                uint32_t buffer_size;
                uint8_t *buffer;
                serialize_message(m, &buffer, &buffer_size);
                send_message(p->socket, buffer, buffer_size, NULL);
                wmove(support_window, 1, 1);
                werase(support_window);
                wprintw(support_window, "Game is starting...");
                wrefresh(support_window);
                free(buffer);
                free_message(m);
                form_driver(form, REQ_DEL_CHAR);
                break;
            }

            // SEND the answer
            form_driver(form, REQ_VALIDATION);
            char *answer = field_buffer(field[0], 0);
            if (strlen(answer) > 0) {
                SendResponse *sr = malloc(sizeof(SendResponse));
                if (sr == NULL) {
                    continue; // Error allocating memory
                }
                strncpy(sr->response, answer, MAX_RESPONSE_LENGTH);
                sr->response[n_chars] = '\0';
                Message *m = payload_to_message(SEND_RESPONSE, (void*)sr, p->player_id);

                uint32_t buffer_size;
                uint8_t *buffer;
                serialize_message(m, &buffer, &buffer_size);
                send_message(p->socket, buffer, buffer_size, NULL);

                free(buffer);
                free_message(m);

                set_field_buffer(field[0], 0, ""); // Clear the answer field
                form_driver(form, REQ_VALIDATION);
                n_chars = 0;
            }
            break;
        default:
            n_chars++;
            form_driver(form, ch);
            break;
        }

        MessageQueueItem *mqi = mq_dequeue(mq);
        if (mqi == NULL) {
            continue; // Exit if no message is received
        }
        

        switch (mqi->m->type) {
        case PLAYER_JOINED:
            PlayerJoined *pj = (PlayerJoined*)mqi->m->payload;
            add_player_to_tpdata(tpdata, pj->name, pj->player_id);

            wclear(right_window);
            
            for (int i = 0; i < tpdata->number_of_players; i++) {
                wmove(right_window, i + 1, 1);
                wprintw(right_window, "%s - Points: %d\n\n", tpdata->player_names[i], tpdata->player_points[i]);
            }
            box(right_window, 0, 0);
            wrefresh(right_window);
            break;
        case PLAYER_QUIT:
            PlayerQuit *pq = (PlayerQuit*)mqi->m->payload;
            remove_player_from_tpdata(tpdata, pq->player_id);

            wclear(right_window);

            for (int i = 0; i < tpdata->number_of_players; i++) {
                wmove(right_window, i + 1, 1);
                wprintw(right_window, "%s - Points: %d\n\n", tpdata->player_names[i], tpdata->player_points[i]);
            }
            box(right_window, 0, 0);
            wrefresh(right_window);
            break;
        case PLAYER_RESPONSE_CHANGED:
            PlayerResponseChanged *prc = (PlayerResponseChanged*)mqi->m->payload;
            // Handle player response change if needed
            update_player_response(tpdata, prc->player_id, prc->points_earned, prc->response, prc->is_correct);

            wclear(right_window);

            for (int i = 0; i < tpdata->number_of_players; i++) {
                wmove(right_window, i + 1, 1);
                wprintw(right_window, "%s - Points: %d\n\n", tpdata->player_names[i], tpdata->player_points[i]);
            }
            box(right_window, 0, 0);
            wrefresh(right_window);
            break;
        case QUESTION_SENT:
            lobby_state = CS_QUESTION;
            QuestionSent *qs = (QuestionSent*)mqi->m->payload;

            werase(question_window);
            wprintw(question_window, "%s", qs->question);
            wrefresh(question_window);
            // Clear previous support text
            werase(support_window);
            wprintw(support_window, "%s", qs->support);
            wrefresh(support_window);
            break;
        case ANSWER_SENT:
            lobby_state = CS_ANSWER;
            AnswerSent *as = (AnswerSent*)mqi->m->payload;

            // Clear previous support text
            werase(support_window);
            wprintw(support_window, "Answer: %s", as->answer);
            wrefresh(support_window);
            // refresh();
            break;
        case GAME_STARTS:
            break;
        case GAME_ENDED:
            werase(question_window);
            werase(support_window);

            // clear();
            int winner_id = ((GameEnded*)mqi->m->payload)->winner_id;
            char winner_username[MAX_USERNAME_LENGTH]; 
            for (int i = 0; i < tpdata->number_of_players; i++) {
                if(winner_id == tpdata->players_id[i]) {
                    strncpy(winner_username, tpdata->player_names[i], MAX_USERNAME_LENGTH);
                    break;
                }
            }

            wprintw(support_window, "Game Ended!\n");
            wprintw(support_window, "Winner: %s\n", winner_username);
            wrefresh(support_window);

            sleep(5);

            lobby_state = CS_WAITING_ROOM;
            return CS_DISCONNECTED;
        default:
            break;
        }

        free_message(mqi->m);
        free(mqi);


    }

    return CS_DISCONNECTED;
}