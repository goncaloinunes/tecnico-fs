#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define SIZE 120000

#define PATH "/f1"

/**
   This test writes to a file and creates two threads that will read the contents of the file simultaneously.
   Both threads have to read the contents of the file correctly in order to pass the test. This happens when
   one thread read all the contents of the file and the other could not read anything.
 */

char input[SIZE];
char output1[SIZE];
char output2[SIZE];


void* read1(void* fd) {
    
    
    tfs_read(*(int*)fd, output1, SIZE-1);
    
    return NULL;
}

void* read2(void* fd) {

    tfs_read(*(int*)fd, output2, SIZE-1);

    return NULL;
}

int main() {
    pthread_t tid[2];
    char *path = "/f1";
    memset(input, 'A', SIZE);
    memset(output1, '\0', SIZE);
    memset(output2, '\0', SIZE);


    assert(tfs_init() != -1);

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    assert(tfs_write(fd, input, SIZE-1) == SIZE-1);
    assert(tfs_close(fd) != -1);

    fd = tfs_open(path, 0);
    
    if (pthread_create (&tid[0], NULL, read1, (void*)&fd) != 0)
        exit(EXIT_FAILURE);
    
     if (pthread_create (&tid[1], NULL, read2, (void*)&fd) != 0)
         exit(EXIT_FAILURE);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    assert((strlen(output1) == SIZE-1 && strlen(output2) == 0) || (strlen(output2) == SIZE-1 && strlen(output2) == 0));    

    printf("Sucessful test\n");

    return 0;
}