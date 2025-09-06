#ifndef QUEUE_H
#define QUEUE_H

typedef struct QNode {
    int idx;               // index into Process array
    struct QNode *next;
} QNode;

typedef struct {
    QNode *head, *tail;
    int size;
} Queue;

void q_init(Queue *q);
int  q_empty(Queue *q);
void q_push(Queue *q, int idx);
int  q_pop(Queue *q);      // returns -1 if empty
void q_clear(Queue *q);

#endif
