#include "queue.h"

void enqueue(QueueNode** head, void* data) {
    QueueNode* new_node = (QueueNode*)malloc(sizeof(QueueNode));
    new_node->data = data;
    new_node->next = NULL;

    if (*head == NULL) {
        *head = new_node;
    } else {
        QueueNode* current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
}

void* dequeue(QueueNode** head) {
    if (*head == NULL) {
        return NULL;
    }

    QueueNode* aux = *head;
    void* data = aux->data;
    *head = (*head)->next;
    free(aux);
    return data;
}