#ifndef SERVER_H
#define SERVER_H

#include <string.h>
#include "operations.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>


#define MAX_CLIENTS 5
#define BUFFER_SIZE 512
#define MAX_REQUESTS_PER_CLIENT 5


typedef struct {
    char pipe_name[MAX_FILE_NAME];
    int fd;
    char* buffer[MAX_REQUESTS_PER_CLIENT];
    pthread_mutex_t mutex;
    pthread_cond_t consume;
    pthread_cond_t produce;
    int prodptr;
    int consptr;
    int count;
    pthread_t thread;
} client_t;


int find_free_client();
void initialize_clients();
void initialize_client(int client);
void free_client(int client);
int set_client(char path[MAX_FILE_NAME], int fd);
int handle_mount(mount_args_t args);
void *consumer(void *client_id);

#endif // SERVER_H