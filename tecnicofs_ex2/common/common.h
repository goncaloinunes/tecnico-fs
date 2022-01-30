#ifndef COMMON_H
#define COMMON_H

#include <config.h>
#include <sys/types.h>


/* tfs_open flags */
enum {
    TFS_O_CREAT = 0b001,
    TFS_O_TRUNC = 0b010,
    TFS_O_APPEND = 0b100,
};

/* operation codes (for client-server requests) */
enum {
    TFS_OP_CODE_MOUNT = 1,
    TFS_OP_CODE_UNMOUNT = 2,
    TFS_OP_CODE_OPEN = 3,
    TFS_OP_CODE_CLOSE = 4,
    TFS_OP_CODE_WRITE = 5,
    TFS_OP_CODE_READ = 6,
    TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED = 7
};


/*
 * Protocolo 
*/

typedef struct {
    char client_pipe_name[MAX_FILE_NAME];
} mount_args_t;

typedef struct {
    int session_id;
} unmount_args_t;

typedef struct {
    int session_id;
    char client_pipe_name[MAX_FILE_NAME];
    int flags;
} open_args_t;

typedef struct {
    int session_id;
    int fhandle;
} close_args_t;

typedef struct {
    int session_id;
    int fhandle;
    size_t len;
} write_args_t;

typedef struct {
    int session_id;
    int fhandle;
    size_t len;
} read_args_t;

typedef struct {
    int session_id;
} shutdown_args_t;




#endif /* COMMON_H */