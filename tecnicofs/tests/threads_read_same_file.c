#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define COUNT 400
#define SIZE 250

#define PATH "/f1"

/**
   This test fills in a new file up to 10 blocks via multiple writes, 
   where some calls to tfs_write may imply filling in 2 consecutive blocks, 
   then checks if the file contents are as expected. 
   This is done in two simultanious threads.
 */

char input[SIZE]; 


void* read() {

    char output [SIZE];
    char *path = "/f1";

    int fd = tfs_open(path, 0);
    assert(fd != -1 );

    for (int i = 0; i < COUNT; i++) {
        assert(tfs_read(fd, output, SIZE) == SIZE);
        assert(memcmp(input, output, SIZE) == 0);
    }

    assert(tfs_close(fd) != -1);

    return NULL;
}


int main() {
    pthread_t tid[2];
    char *path = "/f1";
    memset(input, 'A', SIZE);

    assert(tfs_init() != -1);

    /* Write input COUNT times into a new file */
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);

    for (int i = 0; i < COUNT; i++) {
        assert(tfs_write(fd, input, SIZE) == SIZE);
    }
    assert(tfs_close(fd) != -1);


    if (pthread_create (&tid[0], NULL, read, NULL) != 0)
        exit(EXIT_FAILURE);
    
     if (pthread_create (&tid[1], NULL, read, NULL) != 0)
         exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);


    printf("Sucessful test\n");

    return 0;
}