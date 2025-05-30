/* 

Compile with: 
gcc -Iinclude -Ilib/sqlite3 -I../ -o tests/game_logic_test -g ./tests/game_logic.test.c ./src/game_logic.c src/message_queue.c src/questions.c ./lib/sqlite/sqlite3.c ../common.c -lpthread  
*/
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "game_logic.h"
#include "common.h"

#define N_PLAYERS 10

typedef struct {
    char username[MAX_USERNAME_LENGTH];
    int uuid;
} FictiousPlayers;

int main() {
    printf("P1\n");
    fflush(stdout);
    init_mutexes();
    printf("P2\n");
    fflush(stdout);

    init_database(DATABASE_PATH);

    printf("P3\n");
    fflush(stdout);
    char support_text[] =   "Les sirènes du port d'Alexandrie \n"
                            "Chantent encore la même mélodie wowo \n"
                            "La lumière du phare d'Alexandrie \n"
                            "Fait naufrager les papillons de ma jeunesse. \n";

    Question q = {
        .question = "Qui chante cette chanson ?",
        .support_type = STRING,
        .support = support_text,
        .number_of_valid_answers = 2,
        .valid_answers = (char *[]){"Claude François", "Clolo   "}
    };

    insert_question(DATABASE_PATH, &q, q.number_of_valid_answers);

    printf("P4\n");
    fflush(stdout);

    char usernames[MAX_LOBBY_LENGTH][N_PLAYERS] = {"Joueur1", "Joueur2", "Joueur3", "Joueur4", "Joueur5", "Joueur6", "Joueur7", "Joueur8", "Joueur9", "Joueur10"};
    FictiousPlayers *fictious_players = malloc(sizeof(FictiousPlayers)*N_PLAYERS);

    for (int i = 0; i < N_PLAYERS; i++) {
        strncpy(fictious_players[i].username,  usernames[i], MAX_USERNAME_LENGTH);
        fictious_players[i].uuid = i + 42;
        create_player(NO_SOCKET, fictious_players[i].uuid, fictious_players[i].username);        
    }

    for (int i = 0; i < N_PLAYERS; i++) {
        printf("Player %d created with username: %s and uuid: %d\n", i, players[get_player_space_from_id(fictious_players[i].uuid)]->username, players[get_player_space_from_id(fictious_players[i].uuid)]->player_id);
        fflush(stdout);
    }

    printf("P5\n");
    fflush(stdout);


    int lobby_id;
    create_lobby("Lobby de test 1", 5, fictious_players[0].uuid, &lobby_id);
    create_lobby("Lobby de test 2", 10, fictious_players[3].uuid, NULL);

    printf("Lobby created with id: %d\n", lobby_id);
    fflush(stdout);
    
    Message *player_joined;
    printf("join_lobby: %i\n", join_lobby(fictious_players[1].uuid, lobby_id, &player_joined));
    printf("join_lobby: %i\n", join_lobby(fictious_players[3].uuid, lobby_id, NULL));
    printf("join_lobby: %i\n", join_lobby(fictious_players[2].uuid, lobby_id, NULL));
    
    printf("P6\n");
    fflush(stdout);

    Message *players_data;
    get_players_data(lobby_id, &players_data);
    
    printf("P7\n");
    fflush(stdout);
    
    Message *player_quit;
    printf("quit_lobby: %i\n", quit_lobby(fictious_players[3].uuid, &player_quit));
    printf("quit_lobby: %i\n", quit_lobby(fictious_players[0].uuid, NULL));
    
    printf("P8\n");
    fflush(stdout);
    
    start_game(lobbies[lobby_id]->owner_id, lobby_id);

    
    sleep(10);

    SendResponse *sr = malloc(sizeof(SendResponse));
    strncpy(sr->response, "CECI EST UNE REPONSE", MAX_RESPONSE_LENGTH);

    Message *m = payload_to_message(SEND_RESPONSE, (void*)sr, fictious_players[2].uuid);
    lobby_enqueue(lobby_id, RECEIVE_QUEUE, m, NO_SOCKET);


    return 0;
}