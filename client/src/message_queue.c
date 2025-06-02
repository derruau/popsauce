// Cette structure doit contenir pour chaque message le moment où il est arrivé
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
    int item_count;
    pthread_mutex_t mutex;
} MessageQueue;

// Initializes a message queue
// Returns a pointer to the initialized queue, or NULL if memory allocation fails.
MessageQueue *mq_init() {
    MessageQueue *q = malloc(sizeof(MessageQueue));
    if (q == NULL) return NULL;

    q->front = 0;
    q->rear = 0;
    q->item_count = 0;

    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        q->items[i] = NULL;
    }

    pthread_mutex_init(&q->mutex, NULL);

    return q;
}

// Returns 1 when message queue is empty and 0 when it's not.
// Returns 1 if the queue is NULL
int mq_is_empty(MessageQueue* q) { 
    if (q == NULL) return 1;

    pthread_mutex_lock(&q->mutex);
    int res = q->item_count == 0;
    pthread_mutex_unlock(&q->mutex);

    return res; 
}

// Returns 1 when message queue is full and 0 when it's not.
// Returns 1 if the queue is NULL
int mq_is_full(MessageQueue* q) { 
    if (q == NULL) return 1;

    pthread_mutex_lock(&q->mutex);
    int res = q->item_count == MAX_QUEUE_SIZE;
    pthread_mutex_unlock(&q->mutex);

    return res; 
}

// Enqueues an item into the message queue
// If the queue is full, it does nothing
void mq_enqueue(MessageQueue* mq, Message *m, int socket) {
    if (mq_is_full(mq)) {
        return;
    }

    pthread_mutex_lock(&mq->mutex);

    MessageQueueItem *mqi = (MessageQueueItem*)malloc(sizeof(MessageQueueItem));
    if (mqi == NULL) {
        pthread_mutex_unlock(&mq->mutex);
        return;}

    mqi->m = m;
    mqi->time = time(NULL);
    mqi->socket = socket;

    mq->items[mq->rear] = mqi;
    mq->rear = (mq->rear + 1) % MAX_QUEUE_SIZE;
    mq->item_count++;

    pthread_mutex_unlock(&mq->mutex);
}

// Dequeues an item from the message queue
// Returns a pointer to the dequeued item, or NULL if the queue is empty
MessageQueueItem *mq_dequeue(MessageQueue* mq) {
    if (mq_is_empty(mq)) return NULL;
    
    pthread_mutex_lock(&mq->mutex);

    MessageQueueItem *mqi = malloc(sizeof(MessageQueueItem));
    if (mqi == NULL) {
        pthread_mutex_unlock(&mq->mutex);
        return NULL;
    }
    memcpy(mqi, mq->items[mq->front], sizeof(MessageQueueItem));

    mq->front = (mq->front + 1) % MAX_QUEUE_SIZE;
    mq->item_count--;

    pthread_mutex_unlock(&mq->mutex);

    return mqi;
}

// Frees the message queue and all its items
void mq_free(MessageQueue *mq) {
    if (mq == NULL) return;

    pthread_mutex_lock(&mq->mutex);

    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        if (mq->items[i] == NULL) continue;
        if (mq->items[i]->m != NULL) {
            free_message(mq->items[i]->m);
        }
        free(mq->items[i]);
    }

    pthread_mutex_unlock(&mq->mutex);

    free(mq);
}