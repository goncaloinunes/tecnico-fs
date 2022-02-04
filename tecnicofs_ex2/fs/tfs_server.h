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
void free_clients();
int set_client(char path[MAX_FILE_NAME], int fd);
int handle_mount(mount_args_t args);
int handle_unmount(unmount_args_t args);
int handle_open(open_args_t args);
int handle_close(close_args_t args);
int handle_write(write_args_t args);
int handle_read(read_args_t args);
int handle_shutdown(shutdown_args_t args, char pipename[MAX_FILE_NAME], int fd);
void *consumer(void *client_id);


#endif // SERVER_H