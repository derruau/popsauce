/*

To compile the program:
gcc -Iinclude -I../ -o tests/message_queue_test -g ./tests/message_queue.test.c src/message_queue.c ../common.c  

*/

#include "message_queue.h"
#include <stdio.h>

int main() {
    Message *game_starts = (Message*)malloc(sizeof(Message));
    if (game_starts == NULL) {
        exit(EXIT_FAILURE);
    }

    game_starts->uuid = SERVER_UUID;
    game_starts->type = GAME_STARTS;
    game_starts->payload_size = 0;
    game_starts->payload = NULL;

    printf("here\n");
    MessageQueue *mq = mq_init();
    printf("here2\n");

    mq_enqueue(mq, game_starts, 0);
    mq_enqueue(mq, game_starts, 0);
    mq_enqueue(mq, game_starts, 0);

    printf("here3\n");

    MessageQueueItem *mqi = mq_dequeue(mq);
    mq_dequeue(mq);
    mq_dequeue(mq);
    MessageQueueItem *mqi2 = mq_dequeue(mq);

    printf("here4\n");

    printf("%i\n", mqi->m->payload_size);
}