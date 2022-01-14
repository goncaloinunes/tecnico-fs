#include "../fs/operations.h"
#include <assert.h>
#include <string.h>


#define SIZE 100025


/**
   This test creates a file and passes the file handler to two threads. 
   Each thread writes a buffer to the file.
   The test is sucessful if the contents of the writen file are a concatenation of the buffers writen by the threads.
 */

char input1[SIZE]; 
char input2[SIZE];
char *path1 = "/f1";


char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}


void* write1(void* arg) {

    
    assert(tfs_write(*(int*)arg, input1, SIZE-1) == SIZE-1);
    

    return NULL;
}

void* write2(void* arg) {

    
    assert(tfs_write(*(int*)arg, input2, SIZE-1) == SIZE-1);
    

    return NULL;
}

int main() {
    pthread_t tid[2];

    memset(input1, 'A', SIZE);
    input1[SIZE-1] = '\0';
    memset(input2, 'B', SIZE);
    input2[SIZE-1] = '\0';
    
    
    char output[SIZE*2];
    memset(output, '\0', SIZE*2);

    assert(tfs_init() != -1);

    int fd = tfs_open(path1, TFS_O_CREAT);
    assert(fd != -1);
    

    
    if (pthread_create (&tid[0], NULL, write1, (void*)&fd) != 0)
       exit(EXIT_FAILURE);
    
    if (pthread_create (&tid[1], NULL, write2, (void*)&fd) != 0)
       exit(EXIT_FAILURE);

    
    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    assert(tfs_close(fd) != -1);

    fd = tfs_open(path1, 0);
    assert(fd != -1);

    assert(tfs_read(fd, output, SIZE*2-2) == SIZE*2-2);
    assert(strcmp(concat(input1, input2), output) == 0 || strcmp(concat(input2, input1), output) == 0);
    
    assert(tfs_close(fd) != -1);
    

    printf("Sucessful test\n");

    return 0;
}