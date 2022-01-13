#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define COUNT 1
#define SIZE 5


/**
   This test fills in a new file up to 10 blocks via multiple writes, 
   where some calls to tfs_write may imply filling in 2 consecutive blocks, 
   then checks if the file contents are as expected. 
   This is done in two simultanious threads.
 */

char input1[SIZE]; 
char input2[SIZE];


char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}


void* write(void* input) {

    char *path = "/f1";

    int fd = tfs_open(path, TFS_O_APPEND);
    assert(fd != -1);

    for (int i = 0; i < COUNT; i++) {
        assert(tfs_write(fd, (char*)input, SIZE-1) == SIZE-1);
    }

    assert(tfs_close(fd) != -1);

    return NULL;
}


int main() {
    pthread_t tid[2];
    char *path1 = "/f1";
    char *path2 = "external_file.txt";
    memset(input1, 'A', SIZE);
    input1[SIZE-1] = '\0';
    memset(input2, 'B', SIZE);
    input2[SIZE-1] = '\0';
    
    //char* result = concat(input1, input2);
    char output[SIZE*2];
    memset(output, '\0', SIZE*2);

    assert(tfs_init() != -1);

    int fd = tfs_open(path1, TFS_O_CREAT);
    assert(fd != -1);
    

    
    if (pthread_create (&tid[0], NULL, write, (void*)input1) != 0)
       exit(EXIT_FAILURE);
    
    if (pthread_create (&tid[1], NULL, write, (void*)input2) != 0)
       exit(EXIT_FAILURE);

    
    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);


    
    assert(tfs_copy_to_external_fs(path1, path2) != -1);

    assert(tfs_read(fd, output, SIZE*2-2) == SIZE*2-2);
    assert(strcmp(concat(input1, input2), output) == 0 || strcmp(concat(input2, input1), output) == 0);
    puts(output);
    assert(tfs_close(fd) != -1);

    printf("Sucessful test\n");

    return 0;
}