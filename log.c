#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "log.h"

typedef struct LogDataNode {
    char* logData;
    struct LogDataNode* next;
}   LogDataNode;

// Opaque struct definition
struct Server_Log {
    LogDataNode* head;
    int readersInside;
    int writersInside;
    int writersWaiting;
    pthread_mutex_t mutexLock;
    pthread_cond_t readAllowed;
    pthread_cond_t writeAllowed;
};

// Creates a new server log instance (stub)
server_log create_log() {
    server_log serverLog = (server_log)malloc(sizeof(struct Server_Log));
    serverLog->head = NULL;
    serverLog->readersInside = 0;
    serverLog->writersInside = 0;
    serverLog->writersWaiting = 0;
    pthread_mutex_init(&serverLog->mutexLock, NULL);
    pthread_cond_init(&serverLog->readAllowed, NULL);
    pthread_cond_init(&serverLog->writeAllowed, NULL);
    return serverLog;
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    if (!log) {
        return;
    }
    pthread_mutex_destroy(&log->mutexLock);
    pthread_cond_destroy(&log->readAllowed);
    pthread_cond_destroy(&log->writeAllowed);
    LogDataNode* current = log->head;
    LogDataNode* temp;
    while (current != NULL) {
        temp = current->next;
        free(current->logData);
        free(current);
        current = temp;
    }
    free(log);
}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    // TODO: Return the full contents of the log as a dynamically allocated string
    // This function should handle concurrent access
    pthread_mutex_lock(&log->mutexLock);
    while (log->writersWaiting >= 1 || log->writersInside >= 1) {
        pthread_cond_wait(&log->readAllowed, &log->mutexLock);
    }
    log->readersInside++;

    int len = 0;
    LogDataNode* current = log->head;
    while (current != NULL) {
        len += (int)strlen(current->logData);
        current = current->next;
    }
    *dst = (char*)malloc(len + 1); // Allocate for caller
    *dst[0] = '\0';
    if (*dst != NULL) {
        current = log->head;
        while (current != NULL) {
            strcat(*dst, current->logData);
            current = current->next;
        }
    }

    pthread_mutex_lock(&log->mutexLock);
    log->readersInside--;
    if (log->readersInside == 0) {
        pthread_cond_signal(&log->writeAllowed);
    }
    pthread_mutex_unlock(&log->mutexLock);
    return len;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {
    // TODO: Append the provided data to the log
    // This function should handle concurrent access
}
