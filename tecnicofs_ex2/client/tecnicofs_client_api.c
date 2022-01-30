#include "tecnicofs_client_api.h"
#include <stdio.h>
int session_id = -1;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    
    if(mkfifo(client_pipe_path, 0777) != 0) {
        printf("Erro ao abrir Pipe do cliente\n");
        return -1;
    }

    int fd_s = open(server_pipe_path, O_WRONLY);
    if(fd_s < 0) {
        printf("Erro ao abrir Pipe do servidor\n");
        return -1;
    }


    char buffer[MAX_FILE_NAME+1];
    memset(buffer, '\0', MAX_FILE_NAME+1);
    buffer[0] = TFS_OP_CODE_MOUNT;
    printf("buffer: %s", buffer);

    strcpy(buffer+1, client_pipe_path);
    printf("buffer: %s", buffer);

    if(write(fd_s, &buffer, sizeof(buffer)) < 0) {
        return -1;
    }
    printf("Mensagem Enviada: %s\n", buffer);

    int fd_c = open(client_pipe_path, O_RDONLY);
    if(fd_c < 0) {
        return -1;
    }

    if(read(fd_c, &session_id, sizeof(session_id)) < 0) {
        return -1;
    }

    printf("Session_id: %d\n", session_id);

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
