#include "shared.h"

int queue = 0;
int has_data = 0;
int quit = 0;
int processing = 0;
int temp_n;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_done = PTHREAD_COND_INITIALIZER;