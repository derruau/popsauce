#include "username_screen.h"

#define MAX_INPUT_LENGTH 64

ClientState handle_set_username(ClientState cs, ClientPlayer *p, LobbyList **lobbylist) {
    if (cs != CS_CONNECTED) {
        return CS_DISCONNECTED;
    }
    
    FIELD *fields[2];
    FORM *form;
    WINDOW *form_win;
    int rows, cols;

    getmaxyx(stdscr, rows, cols);

    // Create fields
    fields[0] = new_field(1, MAX_INPUT_LENGTH, 0, 0, 0, 0);
    fields[1] = NULL;

    set_field_back(fields[0], A_UNDERLINE);
    field_opts_off(fields[0], O_AUTOSKIP); // Prevent skipping on full input

    // Create form
    form = new_form(fields);

    // Create a window to contain the form
    int form_height = 3;
    int form_width = MAX_INPUT_LENGTH + 2;
    int starty = (rows - form_height) / 2;
    int startx = (cols - form_width) / 2;

    form_win = newwin(form_height, form_width, starty, startx);
    keypad(form_win, TRUE);
    box(form_win, 0, 0);

    // Create subwindow inside box for the field
    set_form_win(form, form_win);
    set_form_sub(form, derwin(form_win, 1, MAX_INPUT_LENGTH, 1, 1));
    post_form(form);

    mvprintw(starty - 2, (cols - 20) / 2, "Enter your username:");
    refresh();
    wrefresh(form_win);

    int ch;
    while ((ch = wgetch(form_win)) != '\n') {
        switch (ch) {
            case KEY_BACKSPACE:
            case 127:
            case 8:
                form_driver(form, REQ_DEL_PREV);
                break;
            case KEY_LEFT:
                form_driver(form, REQ_PREV_CHAR);
                break;
            case KEY_RIGHT:
                form_driver(form, REQ_NEXT_CHAR);
                break;
            case KEY_DC:
                form_driver(form, REQ_DEL_CHAR);
                break;
            default:
                form_driver(form, ch);
                break;
        }
        wrefresh(form_win);
    }


    p->username = malloc(sizeof(char)*(MAX_INPUT_LENGTH + 1));
    if (p->username == NULL) {
        return CS_DISCONNECTED;
    }

    // Finalize input
    form_driver(form, REQ_VALIDATION);
    strncpy(p->username, field_buffer(fields[0], 0), MAX_INPUT_LENGTH);
    p->username[MAX_INPUT_LENGTH] = '\0';

    // Trim trailing spaces
    for (int i = MAX_INPUT_LENGTH - 1; i >= 0; i--) {
        if (p->username[i] == ' ')
            p->username[i] = '\0';
        else
            break;
    }

    
    
    // Clean up
    unpost_form(form);
    free_form(form);
    free_field(fields[0]);
    delwin(form_win);
    
    clear();
    refresh();

    char text[] = "Please wait...";
    startx = (cols - strlen(text))/2;
    starty = rows / 2;
    WINDOW *transition_window = newwin(1, cols, starty, startx);
    

    wprintw(transition_window, "Please wait...");
    refresh();
    wrefresh(transition_window);


    Connect *connect = malloc(sizeof(Connect));
    strncpy(connect->username, p->username, MAX_USERNAME_LENGTH);

    Message *m = payload_to_message(CONNECT, (void*)connect, p->player_id);

    uint8_t *buffer;
    uint32_t buffer_size;
    serialize_message(m, &buffer, &buffer_size);

    Message *response;
    send_message(p->socket, buffer, buffer_size, &response);

    if (response->type == LOBBYLIST) {
        *lobbylist = malloc(response->payload_size);
        if (*lobbylist == NULL) {
            return CS_DISCONNECTED;
        }

        memcpy(*lobbylist, response->payload, response->payload_size);
        free(buffer);
        free_message(m);
        free_message(response);
        return CS_LOBBY_SELECTION;
    } else if (response->type == ERROR) {
        // TODO: handle error better
        *lobbylist = NULL;
        return CS_DISCONNECTED;
    }

    return CS_DISCONNECTED;
}