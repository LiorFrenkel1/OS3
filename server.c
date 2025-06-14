#include "segel.h"
#include "request.h"
#include "log.h"

#include "queue.c"

#include <pthread.h>
#include <semaphore.h>

#define NUM_OF_WORKERS 10
#define MAX_QUEUE_SIZE 100

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
void getargs(int *port, int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
}

typedef struct {
    Queue* queue;
    server_log log;
} WorkerArgs;

void* handle_requests(void* arg) {
    WorkerArgs* args = (WorkerArgs*)arg;
    Queue* requests_queue = args->queue;
    server_log log = args->log;

    while(1) {
        int* connfd_ptr = (int*)dequeue(requests_queue); //workers will wait here when queue is empty

        int connfd = *connfd_ptr;
        free(connfd_ptr);

        threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = 0;             // Thread ID (placeholder)
        t->stat_req = 0;       // Static request count
        t->dynm_req = 0;       // Dynamic request count
        t->total_req = 0;      // Total request count

        struct timeval arrival, dispatch;
        arrival.tv_sec = 0; arrival.tv_usec = 0;   // DEMO: dummy timestamps
        dispatch.tv_sec = 0; dispatch.tv_usec = 0; // DEMO: dummy timestamps
        // gettimeofday(&arrival, NULL);

        // Call the request handler (immediate in main thread — DEMO ONLY)
        requestHandle(connfd, arrival, dispatch, t, log);

        free(t); // Cleanup
        Close(connfd); // Close the connection
    }
}

void initialize_workers_threads(pthread_t arr[NUM_OF_WORKERS], Queue* q, server_log log) {
    for (int i = 0; i < NUM_OF_WORKERS; i++) {
        WorkerArgs* args = malloc(sizeof(WorkerArgs));
        args->queue = q;
        args->log = log;
        pthread_create(&arr[i], NULL, handle_requests, args);
    }
}

void clear_worker_threads(pthread_t arr[NUM_OF_WORKERS]) {
    for(int i = 0; i < NUM_OF_WORKERS; i++) {
        pthread_cancel(arr[i]);
    }
}

int main(int argc, char *argv[])
{
    // Create the global server log
    server_log log = create_log();

    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    getargs(&port, argc, argv);

    Queue requests_queue;
    initialize(&requests_queue, MAX_QUEUE_SIZE);

    pthread_t worker_threads[NUM_OF_WORKERS];
    initialize_workers_threads(worker_threads, &requests_queue, log);

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        int *connfd_ptr = malloc(sizeof(int));
        *connfd_ptr = connfd;  //pass pointer to connection fd

        enqueue(&requests_queue, connfd_ptr); //will wait here if queue is full

        // TODO: HW3 — Record the request arrival time here
    }

    // Clean up the server log before exiting
    destroy_log(log);
    clear_worker_threads(worker_threads);
    freeQueue(&requests_queue);
}
