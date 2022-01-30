#ifndef SERVER_H
#define SERVER_H

#include <string.h>
#include "operations.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_CLIENTS 5


int find_free_client();
void initialize_clients();
void free_client(int client);
int set_client(char path[MAX_FILE_NAME]);
int handle_mount(mount_args_t args);

#endif // SERVER_H