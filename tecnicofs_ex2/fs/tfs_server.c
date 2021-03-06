#include "tfs_server.h"

static client_t clients[MAX_CLIENTS];

// Trick to mantain clients' ids in memory, so that threads work correctly.
static int ids[MAX_CLIENTS];

int find_free_client() {

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
            return ids[i];
        }
    }

    return -1;
}

void initialize_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ids[i] = i;
        initialize_client(i);
    }
}

void initialize_client(int client) {


    memset(clients[client].pipe_name, '\0', MAX_FILE_NAME);
    clients[client].fd = -1;
    clients[client].count = 0;
    clients[client].consptr = 0;
    clients[client].prodptr = 0;

    if(pthread_mutex_init(&clients[client].mutex, NULL) < 0) {
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&clients[client].consume, NULL) < 0) {
        exit(EXIT_FAILURE);
    }
    
    if(pthread_cond_init(&clients[client].produce, NULL) < 0) {
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&clients[client].thread, NULL, consumer, &ids[client]) < 0) {
        exit(EXIT_FAILURE);
    }

}

void free_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        free_client(i);
        
        pthread_mutex_lock(&clients[i].mutex);

        close(clients[i].fd);

        if(pthread_cond_destroy(&clients[i].consume) < 0) {
            exit(EXIT_FAILURE);
        }
        
        if(pthread_cond_destroy(&clients[i].produce) < 0) {
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&clients[i].mutex);

        if(pthread_mutex_destroy(&clients[i].mutex) < 0) {
            exit(EXIT_FAILURE);
        }   
    }
}


void free_client(int client) {
    
    pthread_mutex_lock(&clients[client].mutex);

    memset(clients[client].pipe_name, '\0', MAX_FILE_NAME);
    clients[client].fd = -1;
    clients[client].count = 0;
    clients[client].consptr = 0;
    clients[client].prodptr = 0;

    pthread_mutex_unlock(&clients[client].mutex);
}


int set_client(char path[MAX_FILE_NAME], int fd) {
    int client = find_free_client();
    if (client < 0) {
        return -1;
    }

    pthread_mutex_lock(&clients[client].mutex);
    memcpy(clients[client].pipe_name, path, MAX_FILE_NAME);
    clients[client].fd = fd;
    pthread_mutex_unlock(&clients[client].mutex);

    return client;
}

int handle_mount(mount_args_t args) {


    int fd = open(args.client_pipe_name, O_WRONLY);
    if (fd < 0) {
        return -1;
    }

    int session_id = set_client(args.client_pipe_name, fd);

    if (write(fd, &session_id, sizeof(session_id)) < 0) {
        return -1;
    }

    printf("Mounted session number: %d\n", session_id);

    return 0;
}

int handle_unmount(unmount_args_t args) {
    pthread_mutex_lock(&clients[args.session_id].mutex);

    if (close(clients[args.session_id].fd) < 0) {
        pthread_mutex_unlock(&clients[args.session_id].mutex);
        return -1;
    }
    pthread_mutex_unlock(&clients[args.session_id].mutex);

    free_client(ids[args.session_id]);

    printf("Unmounted session number: %d\n", args.session_id);

    return 0;
}

int handle_open(open_args_t args) {
    int fd = tfs_open(args.name, args.flags);

    if (write(clients[args.session_id].fd, &fd, sizeof(fd)) < 0) {
        return -1;
    }

    printf("Openned: %s with fhandle: %d\n", args.name, fd);

    return 0;
}

int handle_close(close_args_t args) {
    int ret = tfs_close(args.fhandle);

    if ((write(clients[args.session_id].fd, &ret, sizeof(int))) < 0) {
        return -1;
    }

    printf("Closed file with fhandle: %d\n", args.fhandle);

    return ret;
}

int handle_write(write_args_t args) {
    int id = args.session_id;
    int bytes_read = (int)tfs_write(args.fhandle, clients[id].buffer[clients[id].consptr] + 1 + sizeof(args), args.len);

    if (write(clients[id].fd, &bytes_read,
            sizeof(bytes_read)) < 0) {
        return -1;
    }

    printf("Writing stuff... done!\n");

    return 0;
}

int handle_read(read_args_t args) {
    char *buffer = (char *)malloc(args.len * sizeof(char));
    if (buffer == NULL) {
        return -1;
    }
    memset(buffer, 0, args.len * sizeof(char));

    int bytes_read;

    bytes_read = (int)tfs_read(args.fhandle, buffer, (size_t)args.len);
    if (bytes_read < 0) {
        free(buffer);
        return -1;
    }

    char ret_message[sizeof(bytes_read) + (unsigned)bytes_read * sizeof(char)];

    memcpy(ret_message, &bytes_read, sizeof(bytes_read));
    memcpy(ret_message + sizeof(bytes_read), buffer, (unsigned)bytes_read);

    if (write(clients[args.session_id].fd, &ret_message, sizeof(ret_message)) < 0) {
        free(buffer);
        return -1;
    }

    printf("Reading stuff... done!\n");
    free(buffer);

    return 0;
}

int handle_shutdown(shutdown_args_t args, char pipename[MAX_FILE_NAME], int fd) {

    printf("Request to shutdown from client: %d\n", args.session_id);

    int ret = tfs_destroy_after_all_closed();
    
    
    if(write(clients[args.session_id].fd, &ret, sizeof(ret)) < 0) {
        return -1;
    }

    if(ret == 0) {
        puts("Shutting down...");
        free_clients();
        close(fd);
        unlink(pipename);
        exit(EXIT_SUCCESS);
    }

    puts("Problem trying to shutdown! Resuming...");

    return 0;
}

void *consumer(void *client_id) {

    int id = *(int *)client_id;

    while (1) {
        open_args_t open_args;
        close_args_t close_args;
        read_args_t read_args;
        write_args_t write_args;
        char op_code;

        pthread_mutex_lock(&clients[id].mutex);
        while (clients[id].count == 0) {
            pthread_cond_wait(&clients[id].consume, &clients[id].mutex);
        }

        char *args = clients[id].buffer[clients[id].consptr];
        op_code = args[0];

        switch (op_code) {

            case TFS_OP_CODE_OPEN:;

                memcpy(&open_args, args + 1, sizeof(open_args));

                handle_open(open_args);
                break;

            case TFS_OP_CODE_CLOSE:;
                memcpy(&close_args, args + 1, sizeof(close_args));

                handle_close(close_args);
                break;

            case TFS_OP_CODE_WRITE:;
                memcpy(&write_args, args + 1, sizeof(write_args));

                handle_write(write_args);
                break;

            case TFS_OP_CODE_READ:;
                memcpy(&read_args, args + 1, sizeof(read_args));

                handle_read(read_args);
                break;
            

            default:
                break;

        }

        free(clients[id].buffer[clients[id].consptr]);
        clients[id].consptr++;
        if (clients[id].consptr == MAX_REQUESTS_PER_CLIENT) {
            clients[id].consptr = 0;
        }

        clients[id].count--;

        pthread_cond_signal(&clients[id].produce);
        pthread_mutex_unlock(&clients[id].mutex);
    }

    return NULL;
}

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    int fd;
    char op_code;
    int id;

    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    initialize_clients();
    if(tfs_init() < 0) {
        exit(EXIT_FAILURE);
    }

    unlink(pipename);
    if (mkfifo(pipename, 0777) != 0) {
        fprintf(stderr, "Erro ao criar pipe!");
        exit(EXIT_FAILURE);
    }

    fd = open(pipename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Erro ao abrir pipe!");
        exit(EXIT_FAILURE);
    }

    while (1) {

        if (read(fd, &op_code, 1) <= 0) {
            continue;
        }

        switch (op_code) {

        case TFS_OP_CODE_MOUNT:;
            mount_args_t mount_args;
            if (read(fd, &mount_args, sizeof(mount_args_t)) < 0) {
                break;
            }

            handle_mount(mount_args);
            break;

        case TFS_OP_CODE_UNMOUNT:;
            unmount_args_t unmount_args;
            if (read(fd, &unmount_args, sizeof(unmount_args_t)) < 0) {
                break;
            }

            handle_unmount(unmount_args);
            break;

        case TFS_OP_CODE_OPEN:;
            open_args_t open_args;
            if (read(fd, &open_args, sizeof(open_args)) < 0) {
                break;
            }

            id = open_args.session_id;

            pthread_mutex_lock(&clients[id].mutex);
            while (clients[id].count) {
                pthread_cond_wait(&clients[id].produce, &clients[id].mutex);
            }

            clients[id].buffer[clients[id].prodptr] =
                (char *)malloc(1 + sizeof(open_args));
            if (clients[id].buffer[clients[id].prodptr] == NULL) {
                break;
            }

            clients[id].buffer[clients[id].prodptr][0] = TFS_OP_CODE_OPEN;
            memcpy(clients[id].buffer[clients[id].prodptr] + 1, &open_args,
                   sizeof(open_args));

            clients[id].prodptr++;
            if (clients[id].prodptr == MAX_REQUESTS_PER_CLIENT) {
                clients[id].prodptr = 0;
            }
            clients[id].count++;

            pthread_cond_signal(&clients[id].consume);
            pthread_mutex_unlock(&clients[id].mutex);
            break;

        case TFS_OP_CODE_CLOSE:;
            close_args_t close_args;
            if (read(fd, &close_args, sizeof(close_args)) < 0) {
                break;
            }

            id = close_args.session_id;

            pthread_mutex_lock(&clients[id].mutex);
            while (clients[id].count) {
                pthread_cond_wait(&clients[id].produce, &clients[id].mutex);
            }

            clients[id].buffer[clients[id].prodptr] =
                (char *)malloc(1 + sizeof(close_args));
            if (clients[id].buffer[clients[id].prodptr] == NULL) {
                break;
            }

            clients[id].buffer[clients[id].prodptr][0] = TFS_OP_CODE_CLOSE;
            memcpy(clients[id].buffer[clients[id].prodptr] + 1, &close_args,
                   sizeof(close_args));

            clients[id].prodptr++;
            if (clients[id].prodptr == MAX_REQUESTS_PER_CLIENT) {
                clients[id].prodptr = 0;
            }
            clients[id].count++;

            pthread_cond_signal(&clients[id].consume);
            pthread_mutex_unlock(&clients[id].mutex);
            break;

        case TFS_OP_CODE_WRITE:;
            write_args_t write_args;
            if (read(fd, &write_args, sizeof(write_args)) < 0) {
                break;
            }

            id = write_args.session_id;

            pthread_mutex_lock(&clients[id].mutex);
            while (clients[id].count) {
                pthread_cond_wait(&clients[id].produce, &clients[id].mutex);
            }

            clients[id].buffer[clients[id].prodptr] =
                (char *)malloc(1 + sizeof(write_args) + write_args.len);
            if (clients[id].buffer[clients[id].prodptr] == NULL) {
                break;
            }

            clients[id].buffer[clients[id].prodptr][0] = TFS_OP_CODE_WRITE;
            memcpy(clients[id].buffer[clients[id].prodptr] + 1, &write_args,
                   sizeof(write_args));
            if(read(fd, clients[id].buffer[clients[id].prodptr] + 1 + sizeof(write_args), write_args.len) < 0) {
                break;
            }

            clients[id].prodptr++;
            if (clients[id].prodptr == MAX_REQUESTS_PER_CLIENT) {
                clients[id].prodptr = 0;
            }
            clients[id].count++;

            pthread_cond_signal(&clients[id].consume);
            pthread_mutex_unlock(&clients[id].mutex);
            break;

        case TFS_OP_CODE_READ:;
            read_args_t read_args;
            if (read(fd, &read_args, sizeof(read_args)) < 0) {
                break;
            }

            id = read_args.session_id;

            pthread_mutex_lock(&clients[id].mutex);
            while (clients[id].count) {
                pthread_cond_wait(&clients[id].produce, &clients[id].mutex);
            }

            clients[id].buffer[clients[id].prodptr] =
                (char *)malloc(1 + sizeof(read_args));
            if (clients[id].buffer[clients[id].prodptr] == NULL) {
                break;
            }

            clients[id].buffer[clients[id].prodptr][0] = TFS_OP_CODE_READ;
            memcpy(clients[id].buffer[clients[id].prodptr] + 1, &read_args,
                   sizeof(read_args));

            clients[id].prodptr++;
            if (clients[id].prodptr == MAX_REQUESTS_PER_CLIENT) {
                clients[id].prodptr = 0;
            }
            clients[id].count++;

            pthread_cond_signal(&clients[id].consume);
            pthread_mutex_unlock(&clients[id].mutex);
            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:;
            shutdown_args_t shutdown_args;
            if (read(fd, &shutdown_args, sizeof(shutdown_args_t)) < 0) {
                break;
            }

            handle_shutdown(shutdown_args, pipename, fd);
            break;

        default:
            break;
        }
    }

    close(fd);
    return 0;
}