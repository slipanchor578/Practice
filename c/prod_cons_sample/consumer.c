#include <stdio.h>
#include <unistd.h>
#include "consumer.h"
#include "shared.h"

void* consumer(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (!has_data && !quit) {
            pthread_cond_wait(&cond, &mutex);
        }
        if (quit) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        int n = queue;
        has_data = 0;
        processing = 1;
        pthread_mutex_unlock(&mutex);
        for (int i = 1; i <= n; ++i) {
            printf("%d\n", i);
            sleep(1);
        }
        pthread_mutex_lock(&mutex);
        processing = 0;
        pthread_cond_signal(&cond_done);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}