#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <semaphore.h>

pthread_mutex_t m;
sem_t items; //num of items in queue
sem_t spaces; //num of free spaces in queue

typedef struct Node {
    void* data;
    struct Node* next;
} Node;

typedef struct {
    Node* front;
    Node* back;
} Queue;

typedef struct {
    struct timeval arrival, dispatch;
    int connfd;
} requestEntry;

void initialize(Queue* q, int max_q_size) {
    sem_init(&items, 0, 0);
    sem_init(&spaces, 0, max_q_size);
    pthread_mutex_init(&m, NULL);

    q->front = q->back = NULL;
}

void enqueue(Queue* q, void* data) {
    sem_wait(&spaces);
    pthread_mutex_lock(&m);

    Node* new_node = malloc(sizeof(Node));
    new_node->data = data;
    new_node->next = NULL;

    if (q->back == NULL) {
        q->front = q->back = new_node;
    } else {
        q->back->next = new_node;
        q->back = new_node;
    }

    requestEntry* reqEnt = (requestEntry*)data; //update arrival time
    struct timeval arrival;
    gettimeofday(&arrival, NULL);
    reqEnt->arrival = arrival;

    pthread_mutex_unlock(&m);
    sem_post(&items);
}

void* dequeue(Queue* q) {
    sem_wait(&items);
    pthread_mutex_lock(&m);

    Node* temp = q->front;
    void* data = temp->data;

    requestEntry* reqEnt = (requestEntry*)data;
    // Calculate dispatch time: time from arrival to when worker picks up the request (dequeue)
    struct timeval current, dispatch;
    gettimeofday(&current, NULL);
    timersub(&current, &reqEnt->arrival, &dispatch);
    reqEnt->dispatch = dispatch;

    q->front = temp->next;
    if (q->front == NULL)
        q->back = NULL;

    free(temp);
    pthread_mutex_unlock(&m);
    // DON'T post to spaces here - wait until request is completed
    return data;
}

// New function to signal request completion
void request_completed() {
    sem_post(&spaces);
}

void freeQueue(Queue *q) {
    pthread_mutex_lock(&m);
    while (q->front != NULL) {
        Node *temp = q->front;
        q->front = q->front->next;
        free(temp->data);
        free(temp);
    }
    q->back = NULL;
    pthread_mutex_unlock(&m);

    pthread_mutex_destroy(&m);
    sem_destroy(&items);
    sem_destroy(&spaces);
}