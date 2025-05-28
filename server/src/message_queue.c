// Cette structure doit contenir pour chaque message le moment où il est arrivé#include <time.h>
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

void mq_init(MessageQueue* q) {
    q->front = -1;
    q->rear = -1;
}

int mq_is_empty(MessageQueue* q) { return (q->front == q->rear - 1); }

int mq_is_full(MessageQueue* q) { return ((q->rear + 1) % MAX_QUEUE_SIZE) == q->front; }

void mq_enqueue(MessageQueue* mq, Message *m) {
    if (mq_is_full(mq)) {
        return;
    }

    if (mq->front == -1) {
        mq->front = 0;
    }

    MessageQueueItem *mqi = (MessageQueueItem*)malloc(sizeof(MessageQueueItem));
    if (mqi == NULL) return;

    mqi->m = m;
    mqi->time = time(NULL);

    mq->rear = (mq->rear + 1) % MAX_QUEUE_SIZE;
    mq->items[mq->rear] = mqi;
}

MessageQueueItem *mq_dequeue(MessageQueue* mq) {
    if (mq_is_empty(mq)) return;
    
    MessageQueueItem *mqi = mq->items[mq->front + 1];
    
    if (mq->front == mq->rear) {
        mq->front = mq->rear = -1;
    }
    else {
        mq->front = (mq->front + 1) % MAX_QUEUE_SIZE;
    }

    return mqi;
}