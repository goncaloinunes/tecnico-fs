#ifndef STATE_H
#define STATE_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>

/*
 * Directory entry
 */
typedef struct {
    char d_name[MAX_FILE_NAME];
    int d_inumber;
    pthread_rwlock_t rwlock;
} dir_entry_t;

typedef enum { T_FILE, T_DIRECTORY } inode_type;

/*
 * I-node
 */
typedef struct {
    inode_type i_node_type;
    size_t i_size;
    int i_direct_data_blocks[NUMBER_DIRECT_BLOCKS];
    int i_indirect_data_block;
    pthread_rwlock_t rwlock;
    /* in a real FS, more fields would exist here */
} inode_t;

typedef enum { FREE = 0, TAKEN = 1 } allocation_state_t;

/*
 * Open file entry (in open file table)
 */
typedef struct {
    int of_inumber;
    size_t of_offset;
    int append;
} open_file_entry_t;

#define MAX_DIR_ENTRIES (BLOCK_SIZE / sizeof(dir_entry_t))

size_t get_number_direct_blocks_used(inode_t* inode);
size_t get_number_indirect_blocks_used(inode_t* inode);
size_t get_number_total_blocks_used(inode_t* inode);

void lock_inode_table(char arg);
void lock_open_file_table(char arg);
void lock_open_file_table_entry(char arg, int i);
void unlock_inode_table();
void unlock_open_file_table();
void unlock_open_file_table_entry(int i);


void state_init();
void state_destroy();

int inode_create(inode_type n_type);
int inode_delete(int inumber);
inode_t *inode_get(int inumber);
int inode_datablock_alloc(inode_t* inode, size_t current_block_index);
void* inode_datablock_get(inode_t* inode, size_t current_block_index);
int inode_datablock_free(inode_t* inode);

int clear_dir_entry(int inumber, int sub_inumber);
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name);
int find_in_dir(int inumber, char const *sub_name);

int data_block_alloc();
int data_block_free(int block_number);
void *data_block_get(int block_number);

int add_to_open_file_table(int inumber, size_t offset, int append);
int remove_from_open_file_table(int fhandle);
open_file_entry_t *get_open_file_entry(int fhandle);

#endif // STATE_H
