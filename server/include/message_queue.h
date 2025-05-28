#ifndef MESSAGE_QUEUE_HEADER
#define MESSAGE_QUEUE_HEADER

#include <time.h>
#include <stdlib.h>
#include "common.h"

#define MAX_QUEUE_SIZE 20 // TODO: ne pas mettre une valeur au pif

typedef struct {
    time_t time;
    Message *m;
} MessageQueueItem;

typedef struct {
    MessageQueueItem *items[MAX_QUEUE_SIZE];
    int rear;
    int front;
} MessageQueue;

void mq_init(MessageQueue* q);

int mq_is_empty(MessageQueue* q);

int mq_is_full(MessageQueue* q);

void mq_enqueue(MessageQueue* mq, Message *m);

MessageQueueItem *mq_dequeue(MessageQueue* mq);

#endif