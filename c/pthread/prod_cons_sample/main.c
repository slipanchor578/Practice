#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "shared.h"
#include "consumer.h"

typedef void (*update_func)(void);

void update_quit(void) {
    quit = 1;
}

void update_queue(void) {
    queue = temp_n;
    has_data = 1;
    processing = 1;
}

void with_lock_and_signal(update_func update, pthread_cond_t *cv) {
    pthread_mutex_lock(&mutex);
    update();
    pthread_cond_signal(cv);
    pthread_mutex_unlock(&mutex);
}

int main(void) {
    pthread_t th;
    pthread_create(&th, NULL, consumer, NULL);
    char buf[32];
    while (1) {
        printf("数字を入力 (qで終了): ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin) || buf[0] == 'q') {
            with_lock_and_signal(update_quit, &cond);
            break;
        }
        int n = atoi(buf);
        if (n <= 0) continue;
        temp_n = n;
        with_lock_and_signal(update_queue, &cond);
        pthread_mutex_lock(&mutex);
        while (processing) {
            pthread_cond_wait(&cond_done, &mutex);
        }
        pthread_mutex_unlock(&mutex);
    }
    pthread_join(th, NULL);
    return EXIT_SUCCESS;
}
