#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

/*  This test is similar to test1.c from the 1st exercise.
    The main difference is that this one explores the
    client-server architecture of the 2nd exercise. */


// char server_pipe[40];

// void* test(void* client_pipe) {

//     char *str = "AAA!";
//     char *path = "/f1";
//     char buffer[40];

//     int f;
//     ssize_t r;



//     assert(tfs_mount((char*)client_pipe, server_pipe) == 0);

//     f = tfs_open(path, TFS_O_CREAT);
//     assert(f != -1);

//     r = tfs_write(f, str, strlen(str));
//     assert(r == strlen(str));

//     assert(tfs_close(f) != -1);

//     f = tfs_open(path, 0);
//     assert(f != -1);

//     r = tfs_read(f, buffer, sizeof(buffer) - 1);
//     assert(r == strlen(str));

//     buffer[r] = '\0';
//     assert(strcmp(buffer, str) == 0);

//     assert(tfs_close(f) != -1);

//     assert(tfs_unmount() == 0);
    
//     printf("Successful test.\n");

//     return NULL;
// }


// int main(int argc, char** argv) {
//     pthread_t tid[2];


//     if (argc < 2) {
//         printf("You must provide the following arguments: 'client_pipe_path "
//                "server_pipe_path'\n");
//         return 1;
//     }

//     strcpy(server_pipe, argv[1]);
//     char* client_pipe1 = "client_pipe1";
//     char* client_pipe2 = "client_pipe2";

//     if (pthread_create (&tid[0], NULL, test, client_pipe1) != 0)
//         return 1;
    
//     // if (pthread_create (&tid[1], NULL, test, client_pipe2) != 0)
//     //      return 1;

//     pthread_join(tid[0], NULL);
//     //pthread_join(tid[1], NULL);

//     printf("Sucessful test\n");

//     return 0;
// }


int main(int argc, char **argv) {

    char *str = "AAA!";
    char *path = "/f1";
    char buffer[40];

    int f;
    ssize_t r;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }



    assert(tfs_mount(argv[1], argv[2]) == 0);

    // sleep(1);

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);



    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str));

    buffer[r] = '\0';
    assert(strcmp(buffer, str) == 0);

    assert(tfs_close(f) != -1);

    assert(tfs_unmount() == 0);
    
    printf("Successful test.\n");

    return 0;
}