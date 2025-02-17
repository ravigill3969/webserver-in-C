#include "queue.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define MAX_THREADS 5

int clientQueue[MAX_THREADS];
int queueLength = 0;

pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueCond = PTHREAD_COND_INITIALIZER;

void enqeueClient(int client_fd) {
    pthread_mutex_lock(&queueLock);
    if (queueLength < MAX_THREADS) {
        clientQueue[queueLength++] = client_fd;
        pthread_cond_signal(&queueCond);
    } else {
        close(client_fd);
    }

    pthread_mutex_unlock(&queueLock);
}
int deqeueClient() {
    pthread_mutex_lock(&queueLock);
    while (queueLength == 0) {
        pthread_cond_wait(&queueCond, &queueLock);  // Wait if queue is empty
    }

    int client_fd = clientQueue[0];
    memmove(clientQueue, clientQueue + 1, (queueLength - 1) * sizeof(int));
    queueLength--;

    pthread_mutex_unlock(&queueLock);
    return client_fd;
}