#include "lobbylist_screen.h"

// choice == -1 => Creating lobby
// choice >= 0 => Joining lobby
ClientState handle_lobbylist_menu(ClientPlayer *p, LobbyList *ll, int *choice, PlayersData **_pdata, int *max_players) {
    ITEM **menu_items;
    MENU *menu;
    WINDOW *menu_win;
    int c;
    int rows, cols;

    getmaxyx(stdscr, rows, cols);
    
    char **names = malloc(sizeof(char*) * ll->__quantity);

    // Allocate menu items (+1 for NULL terminator)
    menu_items = (ITEM **)calloc(ll->__quantity + 2, sizeof(ITEM *));
    
    menu_items[0] = new_item(CREATE_LOBBY_STRING, "");
    for (int i = 0; i < (int)ll->__quantity; ++i) {
        names[i] = strdup(ll->names[i]);  // store copy of text
        menu_items[i + 1] = new_item(names[i], "");
    }

    menu_items[ll->__quantity + 1] = NULL;

    menu = new_menu(menu_items);

    int menu_height = ll->__quantity + 2;
    int menu_width = 30;

    menu_win = newwin(menu_height + 4, menu_width + 4, (rows - menu_height) / 2, (cols - menu_width) / 2);
    keypad(menu_win, TRUE);
    box(menu_win, 0, 0);
    set_menu_win(menu, menu_win);
    set_menu_sub(menu, derwin(menu_win, menu_height, menu_width, 2, 2));
    set_menu_mark(menu, " > ");
    set_menu_format(menu, 5, 1);
    post_menu(menu);

    mvwprintw(menu_win, 1, 2, "Select a lobby:");
    wrefresh(menu_win);

    while ((c = wgetch(menu_win)) != '\n') {
        switch (c) {
            case KEY_DOWN:
                menu_driver(menu, REQ_DOWN_ITEM);
                break;
            case KEY_UP:
                menu_driver(menu, REQ_UP_ITEM);
                break;
        }
        wrefresh(menu_win);
    }

    ITEM *cur = current_item(menu);
    if (strcmp(item_name(cur), CREATE_LOBBY_STRING) == 0) {
        unpost_menu(menu);
        for (int i = 0; i < (int)ll->__quantity + 1; ++i) {
            free_item(menu_items[i]);
            // if (names[i] != NULL) free(names[i]);
        } 
        free_menu(menu);
        delwin(menu_win);
        clear();
        refresh();
        *choice = -1;
        ClientState cs = show_create_lobby_screen(p, max_players);
        return cs;
    }

    // -1 because 'Create Lobby' is always the first option.
    int lobby_index = item_index(cur) - 1;
    *choice = lobby_index;

    JoinLobby *jl = malloc(sizeof(JoinLobby));
    if (jl == NULL) {
        unpost_menu(menu);
        for (int i = 0; i < (int)ll->__quantity + 1; ++i) {
            free_item(menu_items[i]);
            if (names[i] != NULL) free(names[i]);
        } 
        free_menu(menu);
        delwin(menu_win);
        return CS_DISCONNECTED;
    }

    jl->lobby_id = ll->ids[lobby_index];

    Message *m = payload_to_message(JOIN_LOBBY, (void*)jl, p->player_id);

    uint8_t *buffer;
    uint32_t buffer_size;
    serialize_message(m, &buffer, &buffer_size);

    Message *response;
    send_message(p->socket, buffer, buffer_size, &response);

    if (response->type == PLAYERS_DATA) {
        *_pdata = malloc(response->payload_size);
        if (_pdata == NULL) {
            return CS_DISCONNECTED;
        }

        memcpy(*_pdata, (PlayersData*)response->payload, response->payload_size);


        // Clean up
        unpost_menu(menu);
        for (int i = 0; i < (int)ll->__quantity + 1; ++i) {
            free_item(menu_items[i]);
            if (names[i] != NULL) free(names[i]);
        } 
        free_menu(menu);
        delwin(menu_win);

        return CS_WAITING_ROOM;
    } else {
        // TODO handle this better

        // Clean up
        unpost_menu(menu);
        for (int i = 0; i < (int)ll->__quantity + 1; ++i) {
            free_item(menu_items[i]);
            if (names[i] != NULL) free(names[i]);
        }  
        free_menu(menu);
        delwin(menu_win);

        *_pdata = NULL;
        return CS_LOBBY_SELECTION;
    }
}

ClientState show_create_lobby_screen(ClientPlayer *p, int *max_players) {
    FIELD *field[3];
	FORM  *form;
    WINDOW *form_win;
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    cbreak();
    noecho();

	/* Initialize the fields */
	field[0] = new_field(1, 20, 1, 1, 0, 0);
	field[1] = new_field(1, 5, 3, 1, 0, 0);
	field[2] = NULL;

	/* Set field options */
	set_field_back(field[0], A_UNDERLINE); 	/* Print a line for the option 	*/
	field_opts_off(field[0], O_AUTOSKIP);  	/* Don't go to next field when this */
    set_field_buffer(field[0], 0, "Lobby");

	set_field_back(field[1], A_UNDERLINE); 
	field_opts_off(field[1], O_AUTOSKIP);
    set_field_buffer(field[1], 0, "5");

    // Create a window big enough to hold labels and fields
    int form_height = 8;
    int form_width = 41;
    form_win = newwin(form_height, form_width, (rows - form_height) / 2, (cols - form_width) / 2);
    box(form_win, 0, 0);
    keypad(form_win, TRUE);

    form = new_form(field);

    set_form_win(form, form_win);
    WINDOW *sub = derwin(form_win, form_height - 2, form_width - 14, 1, 13);
    // box(sub, 0, 0);
    set_form_sub(form, sub);
    post_form(form);


	refresh();
    wrefresh(form_win);
	
    mvwprintw(form_win, 0, 14, "Create Lobby:");
    mvwprintw(form_win, 2, 1, "Lobby Name: ");
    mvwprintw(form_win, 4, 1, "Max players: ");
	refresh();

    // Move to the end of the first field
    set_current_field(form, field[0]);
    form_driver(form, REQ_END_LINE);

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
            case KEY_DOWN:
                form_driver(form, REQ_NEXT_FIELD);
                form_driver(form, REQ_END_LINE);
                break;
            case KEY_UP:
                form_driver(form, REQ_PREV_FIELD);
                form_driver(form, REQ_END_LINE);
                break;
            default:
                if (field_index(current_field(form)) == 1) {
                    if ((ch < '0') || (ch > '9') ) break;
                }

                form_driver(form, ch);
                break;
        }
        wrefresh(form_win);
    }

    char *lobby_name = field_buffer(field[0], 0);
    char *max_player_s = field_buffer(field[1], 0);
    sscanf(max_player_s, "%i", max_players);

    CreateLobby *cl = malloc(sizeof(CreateLobby));
    if (cl == NULL) {
        /* Un post form and free the memory */
        unpost_form(form);
        free_form(form);
        free_field(field[0]);
        free_field(field[1]);
        return CS_DISCONNECTED;
    }

    cl->max_players = *max_players;
    strncpy(cl->name, lobby_name, MAX_LOBBY_NAME);

	/* Un post form and free the memory */
	unpost_form(form);
	free_form(form);
	free_field(field[0]);
	free_field(field[1]);

    Message *m = payload_to_message(CREATE_LOBBY, (void*)cl, p->player_id);

    uint32_t buffer_size;
    uint8_t *buffer;
    serialize_message(m, &buffer, &buffer_size);

    Message *response;
    send_message(p->socket, buffer, buffer_size, &response);

    if (response->type == SUCCESS) {
        return CS_WAITING_ROOM;
    }
    return CS_LOBBY_SELECTION;
}