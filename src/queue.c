#include "queue.h"
#include <stdlib.h>

void q_init(Queue *q){ q->head=q->tail=NULL; q->size=0; }
int  q_empty(Queue *q){ return q->size==0; }
void q_push(Queue *q, int idx){
    QNode *n = (QNode*)malloc(sizeof(QNode));
    n->idx = idx; n->next = NULL;
    if (!q->tail) q->head = q->tail = n;
    else { q->tail->next = n; q->tail = n; }
    q->size++;
}
int q_pop(Queue *q){
    if (!q->head) return -1;
    QNode *n = q->head;
    int idx = n->idx;
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    free(n);
    q->size--;
    return idx;
}
void q_clear(Queue *q){
    while(!q_empty(q)) q_pop(q);
}
