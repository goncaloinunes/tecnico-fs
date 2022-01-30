#include "tfs_server.h"


char clients[MAX_CLIENTS][MAX_FILE_NAME];

int find_free_client() {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i][0] == '\0') {
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
    memset(clients[client], '\0', MAX_FILE_NAME);
}

int set_client(char path[MAX_FILE_NAME]) {
    int client = find_free_client();
    if(client < 0) {
        return -1;
    }

    memcpy(clients[client], path, MAX_FILE_NAME);

    return 0;
}


int handle_mount(mount_args_t args) {
    int session_id = set_client(args.client_pipe_name);
    printf("cliente_pipe_name: %s\n", args.client_pipe_name);

    int fd = open(args.client_pipe_name, O_WRONLY);
    if(fd < 0) {
        return -1;
    }
    printf("Pipe do cliente aberta!\n");

    if(write(fd, &session_id, sizeof(session_id)) < 0) {
        return -1;
    }

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

    if(mkfifo(pipename, 0777) != 0) {
        fprintf(stderr, "Erro ao criar pipe!");
    }

   while(1) {
       fd = open(pipename, O_RDONLY);
       printf("Pipe aberta\n");
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
                printf("Mensagem Recebida: %s\n", mount_args.client_pipe_name);
                handle_mount(mount_args);
                break;

            case TFS_OP_CODE_UNMOUNT: ; 
                unmount_args_t unmount_args;
                if(read(fd, &unmount_args, sizeof(unmount_args_t)) < 0) {
                    break;
                }

                break;

            case TFS_OP_CODE_OPEN: ;
                open_args_t open_args;
                if(read(fd, &open_args, sizeof(open_args_t)) < 0) {
                    break;
                }
                break;

            case TFS_OP_CODE_CLOSE: ;
                close_args_t close_args;
                if(read(fd, &close_args, sizeof(close_args_t)) < 0) {
                    break;
                }
                break;   

            case TFS_OP_CODE_WRITE: ;
                write_args_t write_args;
                if(read(fd, &write_args, sizeof(write_args_t)) < 0) {
                    break;
                }
                break;

            case TFS_OP_CODE_READ: ;
                read_args_t read_args;
                if(read(fd, &read_args, sizeof(read_args_t)) < 0) {
                    break;
                }
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