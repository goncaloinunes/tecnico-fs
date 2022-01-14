#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    
    lock_inode_table('r');
    pthread_rwlock_rdlock(&(inode_get(ROOT_DIR_INUM))->rwlock);

    int ret = find_in_dir(ROOT_DIR_INUM, name);

    pthread_rwlock_unlock(&(inode_get(ROOT_DIR_INUM))->rwlock);
    unlock_inode_table();

    return ret;
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);

    if (inum >= 0) {
        /* The file already exists */
        
        lock_inode_table('r');
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }
        pthread_rwlock_rdlock(&(inode->rwlock));
        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                /* Free all blocks */
                if(inode_datablock_free(inode) < 0) {
                    return -1;
                }
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }

        pthread_rwlock_unlock(&(inode->rwlock));
        unlock_inode_table();

    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        lock_inode_table('w');
        inum = inode_create(T_FILE);
        
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        pthread_rwlock_wrlock(&(inode_get(inum)->rwlock));
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
        pthread_rwlock_unlock(&(inode_get(inum)->rwlock));
        unlock_inode_table();
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    lock_open_file_table('w');
    int ret = add_to_open_file_table(inum, offset);
    unlock_open_file_table();

    return ret;
    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }


ssize_t tfs_write_block(open_file_entry_t *file, inode_t *inode, void const *buffer, size_t to_write) {

    size_t block_index = file->of_offset / BLOCK_SIZE;
    size_t offset = file->of_offset % BLOCK_SIZE;

    if(block_index >= TOTAL_NUMBER_BLOCKS) {
        return -1;
    }

    if(inode->i_size == 0) {
        /* If empty file, allocate new block */
       if((inode->i_direct_data_blocks[block_index] = data_block_alloc()) < 0) {
           return -1;
       }
    }
    
    /* Determine how many bytes to write */
    if (to_write + offset >= BLOCK_SIZE) {
        to_write = BLOCK_SIZE - offset;

        
        if(block_index < TOTAL_NUMBER_BLOCKS - 1) {
            /* Allocate new block when the current block is gonna be filled */
            if(inode_datablock_alloc(inode, block_index) < 0) {
                return -1;
            }
        
        }
    }

    void* block = inode_datablock_get(inode, block_index);
    if(block == NULL) {
        return -1;
    }
    
    /* Perform the actual write */
    memcpy(block + offset, buffer, to_write);
    
    /* The offset associated with the file handle is
        * incremented accordingly */
    file->of_offset += to_write;
    if (file->of_offset > inode->i_size) {
        inode->i_size = file->of_offset;
    }
    
    return (ssize_t)to_write;
}


ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {

    lock_open_file_table_entry('w', fhandle);
    
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    
    /* From the open file table entry, we get the inode */
    lock_inode_table('r');
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }
    unlock_inode_table();
    
    size_t block_offset = file->of_offset % BLOCK_SIZE;
    size_t blocks_to_use = ((len + block_offset -1) / BLOCK_SIZE) + 1;
    ssize_t bytes_written = 0;
    ssize_t total_bytes_written = 0;
    
    pthread_rwlock_wrlock(&(inode->rwlock));
    
    for(size_t i = 0; i < blocks_to_use; i++) {
        bytes_written = tfs_write_block(file, inode, buffer + total_bytes_written, len - (size_t)total_bytes_written);
        if(bytes_written < 0) {
            return -1;
        }

        total_bytes_written += bytes_written;
       
    }
    
    pthread_rwlock_unlock(&(inode->rwlock));
    unlock_open_file_table_entry(fhandle);
    return total_bytes_written;
}

ssize_t tfs_read_block(open_file_entry_t *file, inode_t *inode, void *buffer, size_t to_read) {
    
    size_t block_index = file->of_offset / BLOCK_SIZE;
    size_t block_offset = file->of_offset % BLOCK_SIZE;

    if(block_index >= TOTAL_NUMBER_BLOCKS || block_index >= get_number_total_blocks_used(inode)) {
        return -1;
    } 

    if (block_offset + to_read >= BLOCK_SIZE) {
        to_read = BLOCK_SIZE - block_offset;
    }
    
    void *block = inode_datablock_get(inode, block_index);
    if(block == NULL) {
        return -1;
    }

    /* Perform the actual read */
    memcpy(buffer, block + block_offset, to_read);
    /* The offset associated with the file handle is
        * incremented accordingly */
    file->of_offset += to_read;
    

    return (ssize_t)to_read;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    lock_open_file_table_entry('w', fhandle);
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    lock_inode_table('r');
    inode_t *inode = inode_get(file->of_inumber);
    unlock_inode_table();
    if (inode == NULL) {
        return -1;
    }
   
    /* Determine how many bytes to read */
    pthread_rwlock_rdlock(&(inode->rwlock));
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if(to_read == 0) {
        return 0;
    }

    size_t block_offset = file->of_offset % BLOCK_SIZE;
    size_t blocks_to_use = ((len + block_offset - 1) / BLOCK_SIZE) + 1;
    ssize_t bytes_read = 0;
    ssize_t total_bytes_read = 0;
    
    for(size_t i = 0; i < blocks_to_use; i++) {
        bytes_read = tfs_read_block(file, inode, buffer + total_bytes_read, to_read - (size_t)total_bytes_read);
        if(bytes_read < 0) {
            return -1;
        }
        
        total_bytes_read += bytes_read;
    }

    pthread_rwlock_unlock(&(inode->rwlock));
    unlock_open_file_table_entry(fhandle);
    return total_bytes_read;
}


int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    
    /* Open the source file */
    int fh_source = tfs_open(source_path, TFS_O_READ);
    if(fh_source < 0) {
        return -1;
    }

    /* Open the destination file */
    FILE* fh_dest = fopen(dest_path, "w");
    if (fh_dest == NULL) {
        return -1;
    }

    /* Buffer to be used in the tfs_read operation */
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    /* Read the contents of the file */
    ssize_t bytes_read;
    while((bytes_read = tfs_read(fh_source, (void*)buffer, sizeof(buffer)))) {
        if (bytes_read < 0) {
            return -1;
        }
        
        /* Write the contents read to the destination file */
        fwrite(buffer, sizeof(*buffer), (size_t)bytes_read, fh_dest);
    }

    /* Close both files */
    fclose(fh_dest);
    tfs_close(fh_source);

    return 0;
}