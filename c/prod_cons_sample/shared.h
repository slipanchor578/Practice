#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

extern int queue;
extern int has_data;
extern int quit;
extern int processing;
extern int temp_n;
extern pthread_mutex_t mutex;
extern pthread_cond_t cond;
extern pthread_cond_t cond_done;

#endif