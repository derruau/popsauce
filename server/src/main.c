/*

TODO: Important: Put every action that is sent to the server in a queue and execute them in order

J'ai besoin de:
- Un moyen de se connecter et gérer les connections
    - Server multithread avec broadcast
- Un moyen de choisir des questions
    - Une connection à sqlite qui cherche dans la db de question
- Un moyen de communiquer avec le(s) client(s).
    - Protocol de communicatigetAvailableThreadIdon et fonctions pour faciliter ça.


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

#include "common.h"

// TODO: deplacer dans le fichier de config
#define PORT "7677" // PSQ ça fait 'POPS' sur un clavier de téléphone 3x3
#define BUF_SIZE 1024
#define MAX_CLIENT_CONNECTIONS 16

int listen_socket;
pthread_t ThreadId[MAX_CLIENT_CONNECTIONS] = {0};
int AvailableThreadId[MAX_CLIENT_CONNECTIONS] = {0};

volatile int server_online = 1;

typedef struct {
    int canal;
    int connection_id;
} session_thread_args;

char *stringIP(uint32_t entierIP) {
    struct in_addr ia;
    ia.s_addr = htonl(entierIP); 
    return inet_ntoa(ia);
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
    printf("\x1b7\rVoulez vous vraiment quitter? [y/N] ");
    fflush(stdout);
    c = getchar();
    if (c == 'y' || c == 'Y') {
        server_online = 0;
        printf("[SERVEUR]: Arret du serveur...\n");
        // On ferme bien toutes les connections avant de fermer le serveur
        for (int i=0; i<MAX_CLIENT_CONNECTIONS; i++) {
            if (AvailableThreadId[i] != 0) {
                pthread_cancel(ThreadId[AvailableThreadId[i]]);
            }
        }
    
        close(listen_socket);
        exit(EXIT_SUCCESS);
    }
    else {
        printf("\x1b8\r\x1b[0J");
        signal(SIGINT, INThandler);
    }
    return;
}

int getAvailableThreadId() {
    for (int i = 0; i < MAX_CLIENT_CONNECTIONS; i++) {
        if (AvailableThreadId[i] == 0) {
            return i;
        }
    }

    return -1;
}

void *client_session(void *arg) {
    session_thread_args *a = (session_thread_args*)arg;

    char *buf = malloc(sizeof(char)*BUF_SIZE);
    while(1) {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        //int nbOctetsRecus = lireLigne(a->canal, buf);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        // TODO: interpréter les messages du client
        break;
    }

    AvailableThreadId[a->connection_id] = 0;
    close(a->canal);
    free(buf);
    free(a);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in client_socket;

    int listen_fd = server_listen(PORT);

    signal(SIGINT, INThandler);
    
    while (1) {
        unsigned int client_socket_size = sizeof(client_socket);
        int canal = accept(listen_fd, &client_socket, &client_socket_size);
        if (canal < 0) {
            // TODO: impossible de se connecter au client
        };

        int available_thread = getAvailableThreadId();
        if (available_thread == -1) {
            printf("[ERREUR]: Impossible d'accepter une connection supplémentaire, le serveur est plein!\n");
            continue;
        }

        printf("[INFO]: Nouvelle connection %s:%hu assigné à l'ID: %i\n",
            stringIP(ntohl(client_socket.sin_addr.s_addr)),
            ntohs(client_socket.sin_port),
            available_thread
        );

        session_thread_args *arg = malloc(sizeof(session_thread_args));
        arg->canal = canal;
        arg->connection_id = available_thread;

        int ret = pthread_create(&ThreadId[available_thread], NULL, client_session, arg);
        if (ret != 0) {
            printf("[ERREUR]: Impossible de créer le thread pour la communication avec l'IP: %s", stringIP(ntohl(client_socket.sin_addr.s_addr)));
        }

        AvailableThreadId[available_thread] = 1;

    }

    printf("[SERVEUR]: Arret du serveur...\n");
    // On ferme bien toutes les connections avant de fermer le serveur
    for (int i=0; i<MAX_CLIENT_CONNECTIONS; i++) {
        if (AvailableThreadId[i] != 0) {
            pthread_join(ThreadId[AvailableThreadId[i]], NULL);
        }
    }

    close(listen_socket);

    exit(EXIT_SUCCESS);
}