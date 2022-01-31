#include "tfs_server.h"


client_t clients[MAX_CLIENTS];

int find_free_client() {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].pipe_name[0] == '\0') {
            return i;
        }
    }

    return -1;
}

void initialize_clients() {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        free_client(i);
    }
}

void free_client(int client) {
    memset(clients[client].pipe_name, '\0', MAX_FILE_NAME);
    clients[client].fd = -1;
}

int set_client(char path[MAX_FILE_NAME], int fd) {
    int client = find_free_client();
    if(client < 0) {
        return -1;
    }

    memcpy(clients[client].pipe_name, path, MAX_FILE_NAME);
    clients[client].fd = fd;

    return 0;
}


int handle_mount(mount_args_t args) {
    int fd = open(args.client_pipe_name, O_WRONLY);
    if(fd < 0) {
        return -1;
    }

    int session_id = set_client(args.client_pipe_name, fd);

    if(write(fd, &session_id, sizeof(session_id)) < 0) {
        return -1;
    }

    printf("Mounted session number: %d\n", session_id);

    return 0;
}


int handle_unmount(unmount_args_t args) {
    if(close(clients[args.session_id].fd) < 0) {
        return -1;
    }

    free_client(args.session_id);

    printf("Unmounted session number: %d\n", args.session_id);

    return 0;
}


int handle_open(open_args_t args) {
    int fd = tfs_open(args.name, args.flags);

    if(write(clients[args.session_id].fd, &fd, sizeof(fd)) < 0) {
        return -1;
    }

    printf("Openned: %s with fhandle: %d\n", args.name, fd);

    return 0;
}


int handle_close(close_args_t args) {
    int ret = tfs_close(args.fhandle);

    if(write(clients[args.session_id].fd, &ret, sizeof(ret)) < 0) {
        return -1;
    }

    printf("Closed file with fhandle: %d\n", args.fhandle);

    return ret;
}


// int handle_write(write_args_t args, int fd) {
//     char* ptr = (char*)malloc(args.len * sizeof(char));
//     if(ptr == NULL) {
//         return -1;
//     }

//     if(read(fd, ptr, args.len * sizeof(char)) < 0) {
//         return -1;
//     }

//     tfs_write

//     free(ptr);
//     return 0;
// }


int handle_write(write_args_t args, int fd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    int total_bytes_read = 0;
    int bytes_to_read;
    int bytes_read;

    //printf("fhandle: %d\n", args.fhandle);
    //printf("session_id: %d\n", args.session_id);

    while(total_bytes_read < args.len) {
        if(BUFFER_SIZE > args.len-(size_t)total_bytes_read) {
            bytes_to_read = (int)(args.len-(size_t)total_bytes_read);
        } else {
            bytes_to_read = BUFFER_SIZE;
        }

        bytes_read = (int)read(fd, buffer, (size_t)bytes_to_read);
        //printf("bytes_to_read: %d\n", bytes_read);
        //printf("buffer: %s", buffer);
        if(bytes_read < 0) {
            return -1;
        }

        total_bytes_read += bytes_read;

        if(tfs_write(args.fhandle, buffer, (size_t)bytes_to_read) < 0) {
            total_bytes_read = -1;
            break;
        }

    }

    if(write(clients[args.session_id].fd, &total_bytes_read, sizeof(total_bytes_read)) < 0) {
        return -1;
    }

    printf("Writing stuff... done!\n");

    return 0;
}


int handle_read(read_args_t args) {
    char* buffer = (char*) malloc(args.len * sizeof(char));
    if(buffer == NULL) {
        return -1;
    }
    memset(buffer, 0, args.len * sizeof(char));

    int bytes_read;
    
    bytes_read = (int)tfs_read(args.fhandle, buffer, (size_t)args.len);
    if(bytes_read < 0) {
        free(buffer);
        return -1;
    }

    char buff[sizeof(bytes_read) + (unsigned)bytes_read * sizeof(char)];

    memcpy(buff, &bytes_read, sizeof(bytes_read));
    memcpy(buff + sizeof(bytes_read), buffer, (unsigned)bytes_read);

    if(write(clients[args.session_id].fd, &buff, sizeof(buff)) < 0) {
        free(buffer);
        return -1;
    }

    printf("Reading stuff... done!\n");
    free(buffer);

    return 0;
}


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char* pipename = argv[1];
    int fd;
    char op_code;

    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    initialize_clients();
    tfs_init();

    if(mkfifo(pipename, 0777) != 0) {
        fprintf(stderr, "Erro ao criar pipe!");
    }

    while(1) {
        fd = open(pipename, O_RDONLY);
        if(fd < 0) {
            fprintf(stderr, "Erro ao abrir pipe!");
        }

        if(read(fd, &op_code, 1) <= 0) {
            continue;
        }
            
        
        switch (op_code) {   

            case TFS_OP_CODE_MOUNT: ;
                mount_args_t mount_args;
                if(read(fd, &mount_args, sizeof(mount_args_t)) < 0) {
                    break;
                }

                handle_mount(mount_args);
                break;

            case TFS_OP_CODE_UNMOUNT: ; 
                unmount_args_t unmount_args;
                if(read(fd, &unmount_args, sizeof(unmount_args_t)) < 0) {
                    break;
                }

                handle_unmount(unmount_args);
                break;

            case TFS_OP_CODE_OPEN: ;
                open_args_t open_args;
                if(read(fd, &open_args, sizeof(open_args_t)) < 0) {
                    break;
                }

                handle_open(open_args);
                break;

            case TFS_OP_CODE_CLOSE: ;
                close_args_t close_args;
                if(read(fd, &close_args, sizeof(close_args_t)) < 0) {
                    break;
                }

                handle_close(close_args);
                break;   

            case TFS_OP_CODE_WRITE: ;
                write_args_t write_args;
                if(read(fd, &write_args, sizeof(write_args_t)) < 0) {
                    break;
                }
                
                handle_write(write_args, fd);
                break;

            case TFS_OP_CODE_READ: ;
                read_args_t read_args;
                if(read(fd, &read_args, sizeof(read_args_t)) < 0) {
                    break;
                }

                handle_read(read_args);
                break;   

            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED: ;
                shutdown_args_t shutdown_args;
                if(read(fd, &shutdown_args, sizeof(shutdown_args_t)) < 0) {
                    break;
                }
                break;

            default:
                break;
        }

        close(fd);   
    }

    return 0;
}