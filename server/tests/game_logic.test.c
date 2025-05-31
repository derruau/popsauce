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
#include <string.h>

#define N_PLAYERS 10

typedef struct {
    char username[MAX_USERNAME_LENGTH];
    int uuid;
} FictiousPlayers;

int main() {

    init_game_mutexes();
    printf("Lobbies & players mutex initialized!\n");
    fflush(stdout);

    init_database(DATABASE_PATH);

    printf("\nDatabase initialized!\n");
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
        .valid_answers = (char *[]){"Claude François", "Cloclo   "}
    };

    insert_question(DATABASE_PATH, &q, q.number_of_valid_answers);

    printf("\nQuestion added to database!\n");
    fflush(stdout);

    char usernames[MAX_LOBBY_LENGTH][N_PLAYERS] = {"Joueur1", "Joueur2", "Joueur3", "Joueur4", "Joueur5", "Joueur6", "Joueur7", "Joueur8", "Joueur9", "Joueur10"};
    FictiousPlayers *fictious_players = malloc(sizeof(FictiousPlayers)*N_PLAYERS);

    for (int i = 0; i < N_PLAYERS; i++) {
        strncpy(fictious_players[i].username,  usernames[i], MAX_USERNAME_LENGTH);
        fictious_players[i].uuid = i + 42;
        create_player(NO_SOCKET, fictious_players[i].uuid, fictious_players[i].username);        
    }

    for (int i = 0; i < N_PLAYERS; i++) {
        printf("\nPlayer %d created with username: %s and uuid: %d\n", i, players[get_player_space_from_id(fictious_players[i].uuid)]->username, players[get_player_space_from_id(fictious_players[i].uuid)]->player_id);    
    }
    fflush(stdout);

    int lobby_id;
    int lobby_2_id;
    create_lobby("Lobby de test 1", 5, fictious_players[0].uuid, &lobby_id);
    create_lobby("Lobby de test 2", 10, fictious_players[3].uuid, &lobby_2_id);

    printf("\nLobbies created with ids '%d' and '%d'\n", lobby_id, lobby_2_id);

    printf("\nPlayers in lobby %d: \n", lobby_id);
    for (int i = 0; i < lobbies[lobby_id]->max_players; i++) {    printf("Players in lobby %d: \n", lobby_id);
    for (int i = 0; i < lobbies[lobby_id]->max_players; i++) {
        if (lobbies[lobby_id]->players[i] != NULL) {
            printf("Player %d: %s\n", lobbies[lobby_id]->players[i]->public_player_id, lobbies[lobby_id]->players[i]->username);
        }
    }
    printf("Players in lobby %d: \n", lobby_2_id);
    for (int i = 0; i < lobbies[lobby_2_id]->max_players; i++) {
        if (lobbies[lobby_2_id]->players[i] != NULL) {
            printf("Player %d: %s\n", lobbies[lobby_2_id]->players[i]->public_player_id, lobbies[lobby_2_id]->players[i]->username);
        }
    }
    fflush(stdout);
        if (lobbies[lobby_id]->players[i] != NULL) {
            printf("Player %d: %s\n", lobbies[lobby_id]->players[i]->public_player_id, lobbies[lobby_id]->players[i]->username);
        }
    }
    printf("Players in lobby %d: \n", lobby_2_id);
    for (int i = 0; i < lobbies[lobby_2_id]->max_players; i++) {
        if (lobbies[lobby_2_id]->players[i] != NULL) {
            printf("Player %d: %s\n", lobbies[lobby_2_id]->players[i]->public_player_id, lobbies[lobby_2_id]->players[i]->username);
        }
    }
    fflush(stdout);
    
    printf("\nJoining lobbies...\n");
    Message *player_joined;
    int did_player_join[3] = {0, 0, 0};
    did_player_join[0] = join_lobby(fictious_players[1].uuid, lobby_id, &player_joined);
    did_player_join[1] = join_lobby(fictious_players[2].uuid, lobby_id, NULL);
    did_player_join[2] = join_lobby(fictious_players[3].uuid, lobby_id, NULL);

    printf("\nPlayers in lobby %d: \n", lobby_id);
    for (int i = 0; i < lobbies[lobby_id]->max_players; i++) {
        if (lobbies[lobby_id]->players[i] != NULL) {
            printf("Player %d: %s\n", lobbies[lobby_id]->players[i]->public_player_id, lobbies[lobby_id]->players[i]->username);
        }
    }
    printf("Players in lobby %d: \n", lobby_2_id);
    for (int i = 0; i < lobbies[lobby_2_id]->max_players; i++) {
        if (lobbies[lobby_2_id]->players[i] != NULL) {
            printf("Player %d: %s\n", lobbies[lobby_2_id]->players[i]->public_player_id, lobbies[lobby_2_id]->players[i]->username);
        }
    }
    fflush(stdout);
    

    Message *players_data;
    get_players_data(lobby_id, &players_data);
    PlayersData *pdata = (PlayersData*)players_data->payload;
    printf("\nPlayers data in lobby %d:\n", lobby_id);
    printf("Number of players: %d\n", pdata->number_of_players);
    for (int i = 0; i < pdata->number_of_players; i++) {
        if (pdata->players_id[i] < 0) continue;
        printf("Player %d: %s with points: %d and id: %d\n", i, pdata->player_names[i], pdata->player_points[i], pdata->players_id[i]);
    }
    free_message(players_data);
    
    printf("\nQuitting lobbies...\n");
    quit_lobby(fictious_players[3].uuid, NULL);
    quit_lobby(fictious_players[0].uuid, NULL);

    
    printf("\nPlayers in lobby %d: \n", lobby_id);
    if (lobbies[lobby_id] == NULL) {
        printf("Lobby %d does not exist.\n", lobby_id);
    } else {
        for (int i = 0; i < lobbies[lobby_id]->max_players; i++) {
            if (lobbies[lobby_id]->players[i] != NULL) {
                printf("Player %d: %s\n", lobbies[lobby_id]->players[i]->public_player_id, lobbies[lobby_id]->players[i]->username);
            }
        }
    }
    printf("Players in lobby %d: \n", lobby_2_id);
    if (lobbies[lobby_2_id] == NULL) {
        printf("Lobby %d does not exist.\n", lobby_2_id);
    } else {
        for (int i = 0; i < lobbies[lobby_2_id]->max_players; i++) {
            if (lobbies[lobby_2_id]->players[i] != NULL) {
                printf("Player %d: %s\n", lobbies[lobby_2_id]->players[i]->public_player_id, lobbies[lobby_2_id]->players[i]->username);
            }
        }
    }
    fflush(stdout);

    printf("\nLobbies status:\n");
    for (int i = 0; i < MAX_NUMBER_OF_LOBBIES; i++) {
        if (lobbies[i] != NULL) {
            printf("Lobby %d exists with owner id %d and %d players.\n", i, lobbies[i]->owner_id, lobbies[i]->players_in_lobby);
        } else {
            printf("Lobby %d does not exist.\n", i);
        }
    }

    printf("\nStarting game in lobby %d...\n", lobby_id);
    
    start_game(lobbies[lobby_id]->owner_id, lobby_id);

    printf("Game started in lobby %d.\n", lobby_id);

    sleep(10);

    printf("\nSending response...\n");
    SendResponse *sr = malloc(sizeof(SendResponse));
    strncpy(sr->response, "   cloCLô    ", MAX_RESPONSE_LENGTH);
    Message *m = payload_to_message(SEND_RESPONSE, (void*)sr, fictious_players[2].uuid);
    lobby_enqueue(lobby_id, RECEIVE_QUEUE, m, NO_SOCKET);


    printf("\nWaiting for game to end...\n");
    pthread_join(lobbies[lobby_id]->game_thread, NULL);


    printf("\nDequeuing the SEND_QUEUE for lobby %d...\n", lobby_id);
    MessageQueueItem *mqi = lobby_dequeue(lobby_id, SEND_QUEUE);
    while ((mqi != NULL) && (mqi->m->type != GAME_ENDED) ) {
        if (mqi->m->type == PLAYER_RESPONSE_CHANGED) {
            PlayerResponseChanged *prc = (PlayerResponseChanged*)mqi->m->payload;
            printf("Player %d answered: %s with points: %d\n", prc->player_id, prc->response, prc->points_earned);
        } else if (mqi->m->type == GAME_STARTS) {
            printf("Game starts message received.\n");
        } else if (mqi->m->type == QUESTION_SENT) {
            QuestionSent *qs = (QuestionSent*)mqi->m->payload;
            printf("Question sent: %s\n", qs->question);
            if (qs->support_type == STRING) {
                printf("Support: %s\n", qs->support);
            } else {
                printf("Support type: %d\n", qs->support_type);
            }
        } else if (mqi->m->type == ANSWER_SENT) {
            AnswerSent *as = (AnswerSent*)mqi->m->payload;
            printf("Answer sent: %s\n", as->answer);
        } else {
            printf("Message type: %d received.\n", mqi->m->type);
        }
        free_message(mqi->m);
        free(mqi);
        mqi = lobby_dequeue(lobby_id, SEND_QUEUE);
        fflush(stdout);
    }

    printf("Game ended message received.\n");
    if (mqi != NULL) {
        free_message(mqi->m);
        free(mqi);
    }

    return 0;   
}