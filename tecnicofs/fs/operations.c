#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    return find_in_dir(ROOT_DIR_INUM, name);
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
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                /* Free all blocks */
                for(int i = 0; i > NUMBER_DIRECT_BLOCKS; i++) {
                    if (data_block_free(inode->i_direct_data_blocks[i]) == -1) {
                        return -1;
                    }
                }
                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }


ssize_t tfs_write_block(open_file_entry_t *file, inode_t *inode, void const *buffer, size_t block_index, size_t offset, size_t to_write) {

    if(block_index >= TOTAL_NUMBER_BLOCKS - 1) {
        return -1;
    }

    if (to_write <= 0) {
        return 0;
    }

    if (inode->i_size == 0) {
        /* If empty file, allocate new block */
        inode->i_direct_data_blocks[block_index] = data_block_alloc();
    }

    /*
    printf("to_write: %ld\n", to_write);
    printf("offset: %ld\n", offset);
    */
    /* Determine how many bytes to write */
    if (to_write + offset >= BLOCK_SIZE) {
        to_write = BLOCK_SIZE - offset;

        /* Allocate new block when the current block is gonna be filled */
        if(block_index < TOTAL_NUMBER_BLOCKS - 1) {
            if(block_index < NUMBER_DIRECT_BLOCKS - 1) {
                inode->i_direct_data_blocks[block_index + 1] = data_block_alloc();

            } else {
                if(inode->i_indirect_data_block == -1) {
                    inode->i_indirect_data_block = data_block_alloc();
                }

                int *indirect_block = (int*)data_block_get(inode->i_indirect_data_block);
                indirect_block[block_index - (NUMBER_DIRECT_BLOCKS - 1)] = data_block_alloc();
            } 
        }
    }

    void *block = NULL;
    if(block_index < NUMBER_DIRECT_BLOCKS) {
        block = data_block_get(inode->i_direct_data_blocks[block_index]);
    } else {
        int *indirect_block = (int*)data_block_get(inode->i_indirect_data_block);
        block = data_block_get(indirect_block[block_index - NUMBER_DIRECT_BLOCKS]);
    }

    if (block == NULL) {
        return -1;
    }
    /*
    printf("block + offset: %p\n", block + offset);
    printf("buffer: %p\n", buffer);
    printf("to_write: %ld\n", to_write);
    puts("*******************");
    */
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
    if(len == 0) {
        return 0;
    }

    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    
    size_t starting_block = file->of_offset / BLOCK_SIZE;
    size_t block_offset = file->of_offset - (BLOCK_SIZE * starting_block);
    size_t blocks_to_use = ((len + block_offset - 1) / BLOCK_SIZE) + 1;
    ssize_t bytes_written = 0;
    ssize_t total_bytes_written = 0;
    /*
    puts("");
    puts("*******************");
    printf("len: %ld\n", len);
    printf("BLOCK_SIZE: %d\n", BLOCK_SIZE);
    printf("blocks_to_use: %ld\n", blocks_to_use);
    printf("starting_block: %ld\n", starting_block);
    puts("*******************");
    */
    for(size_t i = 0; i < blocks_to_use; i++) {
        bytes_written = tfs_write_block(file, inode, buffer + total_bytes_written, starting_block + i, block_offset, len - (size_t)total_bytes_written);
        if(bytes_written < 0) {
            return -1;
        }

        total_bytes_written += bytes_written;
        block_offset = 0;
    }

    return total_bytes_written;
}

ssize_t tfs_read_block(open_file_entry_t *file, inode_t *inode, void *buffer, size_t block_index, size_t block_offset, size_t to_read) {
    
    if(block_index >= TOTAL_NUMBER_BLOCKS - 1 || block_index >= get_number_total_blocks_used(*inode)) {
        return -1;
    } 

    if (block_offset + to_read >= BLOCK_SIZE) {
        to_read = BLOCK_SIZE - block_offset;
    }
    
    if (to_read <= 0) {
        return 0;
    }

    void *block = NULL;
    if(block_index < NUMBER_DIRECT_BLOCKS) {
        block = data_block_get(inode->i_direct_data_blocks[block_index]);
    } else {
        int *indirect_block = (int*)data_block_get(inode->i_indirect_data_block);
        block = data_block_get(indirect_block[block_index - NUMBER_DIRECT_BLOCKS]);
    }

    if (block == NULL) {
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
    if(len == 0) {
        return 0;
    }

    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }
    if(to_read == 0) {
        return 0;
    }

    size_t starting_block = file->of_offset / BLOCK_SIZE;
    size_t block_offset = file->of_offset - (BLOCK_SIZE * starting_block);
    size_t blocks_to_use = ((len + block_offset - 1) / BLOCK_SIZE) + 1;
    ssize_t bytes_read = 0;
    ssize_t total_bytes_read = 0;
    
    for(size_t i = 0; i < blocks_to_use; i++) {
        bytes_read = tfs_read_block(file, inode, buffer + total_bytes_read, starting_block + i, block_offset, to_read - (size_t)total_bytes_read);
        if(bytes_read < 0) {
            return -1;
        }
        
        total_bytes_read += bytes_read;
        block_offset = 0;
    }

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