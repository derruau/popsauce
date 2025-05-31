#ifndef MAIN_HEADER
#define MAIN_HEADER

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "game_logic.h"
#include "message_queue.h"
#include "questions.h"
#include "common.h"

#define PORT "7677" // Because it writes POPS on a 3x3 phone keyboard

typedef struct {
    int socket;
    int connection_id;
} session_thread_args;

char *stringIP(uint32_t entierIP);

// Sends a message to every client of a lobby at once.
// Retuns 0 when successfull and a nonzero value when an error occured
int broadcast(Message *m, int lobby_id);

void *handle_server_broadcast(void *args);

int server_listen(char *port);

void INThandler(int sig);

int getavailable_client_threads();

// Tries to send a message with a lot of failsafe mechanisms.
// When something fails, this function still tries to send a EC_INTERNAL_ERROR message.
// When even this fails the server crashes because something went very wrong.
int safe_send_message(int socket, Message *m);

void *handle_client_thread(void *arg);

void print_help();

#endif