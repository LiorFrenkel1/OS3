#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "log.h"
#include "segel.h"

typedef struct LogDataNode {
    char* logData;
    struct LogDataNode* next;
}   LogDataNode;

// Opaque struct definition
struct Server_Log {
    LogDataNode* head;
    LogDataNode* tail;
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
    if (serverLog == NULL) {
        unix_error("Malloc failed");
    }
    serverLog->head = NULL;
    serverLog->tail = NULL;
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

// Return the full contents of the log as a dynamically allocated string
int get_log(server_log log, char** dst) {
    pthread_mutex_lock(&log->mutexLock);
    while (log->writersWaiting >= 1 || log->writersInside >= 1) {
        pthread_cond_wait(&log->readAllowed, &log->mutexLock);
    }
    log->readersInside++;
    pthread_mutex_unlock(&log->mutexLock);

    int len = 0;
    int nodeCount = 0;
    LogDataNode* current = log->head;
    while (current != NULL) {
        len += (int)strlen(current->logData);
        nodeCount++;
        current = current->next;
    }
    //Add space for newlines between entries (nodeCount - 1 newlines)
    if (nodeCount > 1) {
        len += nodeCount - 1;
    }

    *dst = (char*)malloc(len + 1);
    if (*dst == NULL) {
        unix_error("Malloc failed");
    }
    (*dst)[0] = '\0';
    int idx = 0;
    int length;
    if (*dst != NULL) {
        current = log->head;
        int isFirst = 1;
        while (current != NULL) {
            length = (int)strlen(current->logData);
            if (!isFirst) {
                (*dst)[idx] = '\n';  //add newline before each entry except the first
                idx++;
            }
            for (int i = 0; i < length; i++) {
                (*dst)[idx] = current->logData[i];
                idx++;
            }
            current = current->next;
            isFirst = 0;
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

// Append the provided data to the log
void add_to_log(server_log log, const char* data, int data_len) {
    pthread_mutex_lock(&log->mutexLock);
    log->writersWaiting++;
    while (log->writersInside + log->readersInside >= 1) {
        pthread_cond_wait(&log->writeAllowed, &log->mutexLock);
    }
    log->writersWaiting--;
    log->writersInside++;

    //create and add the log entry while still in critical section
    LogDataNode* newData = (LogDataNode*)malloc(sizeof(LogDataNode));
    if (newData == NULL) {
        unix_error("Malloc failed");
    }
    newData->logData = (char*)malloc(data_len + 1);
    if (newData->logData == NULL) {
        unix_error("Malloc failed");
    }
    for (int i = 0; i < data_len; i++) {
        newData->logData[i] = data[i];
    }
    newData->logData[data_len] = '\0';
    newData->next = NULL;

    if (log->head == NULL) {
        log->head = newData;
        log->tail = newData;
    } else {
        log->tail->next = newData;
        log->tail = log->tail->next;
    }

    log->writersInside--;
    if (log->writersInside == 0) {
        pthread_cond_broadcast(&log->readAllowed);
        pthread_cond_signal(&log->writeAllowed);
    }
    pthread_mutex_unlock(&log->mutexLock);
}
