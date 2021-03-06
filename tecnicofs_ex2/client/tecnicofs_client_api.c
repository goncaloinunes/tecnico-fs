#include "tecnicofs_client_api.h"
#include <stdio.h>


int session_id = -1;
int fd_s = -1;
int fd_c = -1;
char client_pipe_name[MAX_FILE_NAME];


int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    unlink(client_pipe_path);
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

    printf("Opened file with fd: %d\n", fd);

    return fd;
}


int tfs_close(int fhandle) {
    char buffer[sizeof(char) + sizeof(close_args_t)];

    close_args_t args;
    args.fhandle = fhandle;
    args.session_id = session_id;

    buffer[0] = TFS_OP_CODE_CLOSE;
    memcpy(buffer+1, &args, sizeof(args));

    if(write(fd_s, buffer, sizeof(buffer)) < 0) {
        return -1;
    }

    int ret;
    if(read(fd_c, &ret, sizeof(ret)) < 0) {
        return -1;
    }

    printf("Closed file with fhandle: %d\n", fhandle);

    return ret;
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

    printf("Wrote %d bytes: %s\n", bytes_written, (char*)buffer);

    return bytes_written;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    char buff[sizeof(char) + sizeof(read_args_t)];

    read_args_t args;
    args.fhandle = fhandle;
    args.len = len;
    args.session_id = session_id;

    buff[0] = TFS_OP_CODE_READ;
    memcpy(buff + 1, &args, sizeof(args));

    if(write(fd_s, buff, sizeof(buff)) < 0) {
        return -1;
    }

    int bytes_read;
    if(read(fd_c, &bytes_read, sizeof(bytes_read)) < 0) {
        return -1;
    }
    
    if(bytes_read > 0) {
        if(read(fd_c, buffer, (size_t)bytes_read) < 0) {
            return -1;
        }
    }

    printf("Read %d bytes: %s\n", bytes_read, (char*)buffer);

    return bytes_read;
}


int tfs_shutdown_after_all_closed() {
    char buffer[sizeof(char) + sizeof(shutdown_args_t)];

    shutdown_args_t args;
    args.session_id = session_id;

    buffer[0] = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    memcpy(buffer+1, &args, sizeof(args));

    if(write(fd_s, buffer, sizeof(buffer)) < 0) {
        return -1;
    }

    int ret;
    if(read(fd_c, &ret, sizeof(ret)) < 0) {
        return -1;
    }
    
    if(unlink(client_pipe_name) < 0) {
        return -1;
    }

    printf("Server is shutting down!\n");

    return ret;
}
