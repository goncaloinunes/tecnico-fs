#include "tecnicofs_client_api.h"
#include <stdio.h>

int session_id = -1;
int fd_s = -1;
int fd_c = -1;
char client_pipe_name[MAX_FILE_NAME];

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    
    if(mkfifo(client_pipe_path, 0777) != 0) {
        printf("Erro ao abrir Pipe do cliente\n");
        return -1;
    }

    fd_s = open(server_pipe_path, O_WRONLY);
    if(fd_s < 0) {
        printf("Erro ao abrir Pipe do servidor\n");
        return -1;
    }


    char buffer[sizeof(char) + sizeof(mount_args_t)];

    mount_args_t args;
    memset(args.client_pipe_name, '\0', sizeof(mount_args_t));
    strcpy(args.client_pipe_name, client_pipe_path);

    buffer[0] = TFS_OP_CODE_MOUNT;
    memcpy(buffer+1, args.client_pipe_name, sizeof(args));


    if(write(fd_s, buffer, sizeof(buffer)) < 0) {
        return -1;
    }

    fd_c = open(client_pipe_path, O_RDONLY);
    if(fd_c < 0) {
        return -1;
    }

    strcpy(client_pipe_name, client_pipe_path);

    if(read(fd_c, &session_id, sizeof(session_id)) < 0) {
        return -1;
    }

    printf("Session_id: %d\n", session_id);

    return 0;
}

int tfs_unmount() {
    
    char buffer[sizeof(char) + sizeof(unmount_args_t)];
    
    unmount_args_t args;
    args.session_id = session_id; 

    buffer[0] = TFS_OP_CODE_UNMOUNT;
    memcpy(buffer+1, &args, sizeof(args));

    if(write(fd_s, buffer, sizeof(buffer)) < 0) {
        return -1;
    }

    if(close(fd_c) < 0) {
        return -1;
    }

    if(unlink(client_pipe_name) < 0) {
        return -1;
    }

    if(close(fd_s) < 0) {
        return -1;
    }

    return 0;
}

int tfs_open(char const *name, int flags) {
    
    char buffer[sizeof(char) + sizeof(open_args_t)];

    open_args_t args;
    args.flags = flags;
    args.session_id = session_id;
    strcpy(args.name, name);

    buffer[0] = TFS_OP_CODE_OPEN;
    memcpy(buffer+1, &args, sizeof(args));

    if(write(fd_s, buffer, sizeof(buffer)) < 0) {
        return -1;
    }

    int fd;
    if(read(fd_c, &fd, sizeof(fd)) < 0) {
        return -1;
    }

    printf("fd: %d\n", fd);

    return fd;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    char buff[sizeof(char) + sizeof(write_args_t) + len * sizeof(char)];

    write_args_t args;
    args.fhandle = fhandle;
    args.len = len;
    args.session_id = session_id;

    buff[0] = TFS_OP_CODE_WRITE;
    memcpy(buff+1, &args, sizeof(args));
    memcpy(buff+1+sizeof(args), buffer, len);

    if(write(fd_s, buff, sizeof(buff)) < 0) {
        return -1;
    }

    int bytes_written;
    if(read(fd_c, &bytes_written, sizeof(bytes_written)) < 0) {
        return -1;
    }


    return bytes_written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
