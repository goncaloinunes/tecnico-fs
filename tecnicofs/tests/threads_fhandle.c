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

char* path = "/f1";
int arr[2];

void* open(void* arg) {
    int fd = tfs_open(path, 0);
    assert(fd != -1);
    arr[*(int*)arg] = fd;

    return NULL;
}


int main() {
    pthread_t tid[2];
    int i1 = 0;
    int i2 = 1;
    int fd = -1;

    assert(tfs_init() != -1);

    assert((fd = tfs_open(path, TFS_O_CREAT)) != -1);
    assert(tfs_close(fd) != -1);

    if (pthread_create (&tid[0], NULL, open, (void *)&i1) != 0)
        exit(EXIT_FAILURE);
    
    if (pthread_create (&tid[1], NULL, open, (void *)&i2) != 0)
         exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    assert(arr[0] != arr[1]);

    assert(tfs_close(arr[0]) != -1);
    assert(tfs_close(arr[1]) != -1);

    printf("Sucessful test\n");

    return 0;
}