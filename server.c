#include "segel.h"
#include "request.h"
#include "log.h"

#include "queue.c" //also has requestEntry

#include <pthread.h>
#include <semaphore.h>

//#define NUM_OF_WORKERS 10 //TODO CHANGE IF NEEDED
//#define MAX_QUEUE_SIZE 100

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// Parses command-line arguments
void getargs(int *port, int *num_threads, int *queue_size, int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *num_threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);

    if (*num_threads <= 0 || *queue_size <= 0) {
        fprintf(stderr, "threads and queue_size must be positive integers.\n");
        exit(1);
    }
}

typedef struct {
    Queue* queue;
    server_log log;
    int id;
} WorkerArgs;

void* handle_requests(void* arg) {
    WorkerArgs* args = (WorkerArgs*)arg;
    Queue* requests_queue = args->queue;
    server_log log = args->log;
    int id = args->id;
    threads_stats t = malloc(sizeof(struct Threads_stats));
    t->id = id;             // Thread ID (placeholder)
    t->stat_req = 0;       // Static request count
    t->dynm_req = 0;       // Dynamic request count
    t->total_req = 0;      // Total request count
    t->post_req = 0;

    while(1) {
        requestEntry* reqEnt = (requestEntry*)dequeue(requests_queue); //workers will wait here when queue is empty

        int connfd = reqEnt->connfd;

        requestHandle(connfd, reqEnt->arrival, reqEnt->dispatch, t, log);

        free(reqEnt);
        Close(connfd); // Close the connection
    }
    free(t); // Cleanup
    free(args);
    return NULL;
}

void initialize_workers_threads(pthread_t* arr, Queue* q, server_log log, int num_threads) {
    for (int i = 0; i < num_threads; i++) {
        WorkerArgs* args = malloc(sizeof(WorkerArgs));
        args->queue = q;
        args->log = log;
        args->id = i+1;
        pthread_create(&arr[i],NULL, handle_requests, args);
    }
}

void clear_worker_threads(pthread_t* arr, int num_threads) {
    for(int i = 0; i < num_threads; i++) {
        pthread_cancel(arr[i]);
    }
}

int main(int argc, char *argv[])
{
    // Create the global server log
    server_log log = create_log();

    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    int num_threads, queue_size;
    getargs(&port, &num_threads, &queue_size, argc, argv);

    Queue requests_queue;
    initialize(&requests_queue, queue_size);

    pthread_t worker_threads[num_threads];
    initialize_workers_threads(worker_threads, &requests_queue, log, num_threads);

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        requestEntry* reqEnt = malloc(sizeof(requestEntry ));
        reqEnt->connfd = connfd;

        enqueue(&requests_queue, reqEnt); //will wait here if queue is full

        // TODO: HW3 — Record the request arrival time here
    }

    // Clean up the server log before exiting
    destroy_log(log);
    clear_worker_threads(worker_threads, num_threads);
    freeQueue(&requests_queue);
}
