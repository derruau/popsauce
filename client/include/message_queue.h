#ifndef MESSAGE_QUEUE_HEADER
#define MESSAGE_QUEUE_HEADER

#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include "common.h"

#define MAX_QUEUE_SIZE 20 // TODO: ne pas mettre une valeur au pif

typedef struct {
    time_t time;
    int socket;
    Message *m;
} MessageQueueItem;

typedef struct {
    MessageQueueItem *items[MAX_QUEUE_SIZE];
    int rear;
    int front;
    pthread_mutex_t mutex;
} MessageQueue;

// Initializes a message queue
// Returns a pointer to the initialized queue, or NULL if memory allocation fails.
MessageQueue *mq_init();

// Returns 1 when message queue is empty and 0 when it's not.
// Returns 1 if the queue is NULL
int mq_is_empty(MessageQueue* q);

// Returns 1 when message queue is full and 0 when it's not.
// Returns 1 if the queue is NULL
int mq_is_full(MessageQueue* q);

// Enqueues an item into the message queue
// If the queue is full, it does nothing
void mq_enqueue(MessageQueue* mq, Message *m, int socket);

// Dequeues an item from the message queue
// Returns a pointer to the dequeued item, or NULL if the queue is empty
MessageQueueItem *mq_dequeue(MessageQueue* mq);

// Frees the message queue and all its items
void mq_free(MessageQueue *mq);

#endif