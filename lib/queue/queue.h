#ifndef QUEUE_H
#define QUEUE_H
#include <stdio.h>
#include <stdlib.h>

typedef struct QueueNode {
    void* data;
    struct QueueNode* next;
} QueueNode;

void enqueue(QueueNode** head, void* data);
void* dequeue(QueueNode** head);

#endif