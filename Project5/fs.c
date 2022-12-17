#include "disk.h"
#include "fs.h"

#define MAX_F_NAME 15
#define MAX_F_NUM 64
#define MAX_FILDES 32
#define MAX_F_SIZE 16777216 // 16MB

typedef struct super_block
{
    int fat_idx;  // First block of the FAT
    int fat_len;  // Length of FAT in blocks
    int dir_idx;  // First block of directory
    int dir_len;  // Length of directory in blocks
    int data_idx; // First block of file-data
} super_block;

typedef struct dir_entry
{
    int used;                  // Is this file-”slot” in use
    char name[MAX_F_NAME + 1]; // DOH!
    int size;                  // file size in bytes
    int head;                  // first data block of file
    int ref_cnt;
    // how many open file descriptors are there?
    // ref_cnt > 0 -> cannot delete file
} dir_entry;

typedef struct file_descriptor
{
    int used; // fd in use
    int file; // the first block of the file
    // (f) to which fd refers too
    int offset; // position of fd within f
} file_descriptor;

static super_block *fs;
static file_descriptor fildes[MAX_FILDES]; // 32
static int FAT[DISK_BLOCKS];               // Will be populated with the FAT data
static dir_entry DIR[MAX_F_NUM];           // Will be populated with the directory data

static int mount = 0; // If there is a currently mounted file system

int make_fs(char *disk_name)
{
    // Create and open the disk
    if (make_disk(disk_name) == -1)
    {
        printf("ERROR: Failed to create the disk\n");
        return -1;
    }
    if (open_disk(disk_name) == -1)
    {
        printf("ERROR: Failed to open the disk\n");
        return -1;
    }

    // Initialize a new superblock for this disk
    super_block sb_init;

    // start the directory at block 1
    sb_init.dir_idx = 1;
    // Directory needs to hold MAX_FILES num of dir_entry, length is in # of blocks
    sb_init.dir_len = (sizeof(dir_entry) * MAX_F_NUM) / BLOCK_SIZE + 1;

    // FAT should go after the directory
    sb_init.fat_idx = sb_init.dir_idx + sb_init.dir_len;
    // FAT needs to hold DISK_BLOCKS num of integers
    // int (4) * DISK_BLOCKS (8192) / BLOCK_SIZE (4096) = 8 exactly
    sb_init.fat_len = (sizeof(int) * DISK_BLOCKS) / BLOCK_SIZE;

    // data should go after the FAT
    sb_init.data_idx = sb_init.fat_idx + sb_init.fat_len;

    // memcpy the new superblock into a buffer, and write it into block 0 in the disk
    char *buffer = malloc(BLOCK_SIZE);
    memcpy((void *)buffer, (void *)&sb_init, sizeof(super_block));
    if (block_write(0, buffer) == -1)
    {
        printf("ERROR: Failed to write super_block to disk\n");
        return -1;
    }

    // Initialize the directory with basic dir_entry
    // not being used, no name, 0 size, no head, no references
    dir_entry blank_dir;
    blank_dir.used = 0;
    blank_dir.name[0] = '\0';
    blank_dir.size = 0;
    blank_dir.head = -1;
    blank_dir.ref_cnt = 0;

    // Copy this entry to all entries in the directory, re-using the buffer
    memset(buffer, 0, BLOCK_SIZE);
    int i;
    for (i = 0; i < MAX_F_NUM; i++)
    {
        memcpy((void *)buffer + i * sizeof(dir_entry),
               (void *)&blank_dir, sizeof(dir_entry));
    }
    if (block_write(sb_init.dir_idx, buffer) == -1)
    {
        printf("ERROR: Failed to write directory to disk\n");
        return -1;
    }

    // Initialize the FAT table starting at data indx and copy it to disk, 0 for free -1 for eof
    // Buffer needs to hold 8 pages of ints
    free(buffer);
    buffer = malloc(sb_init.fat_len * BLOCK_SIZE);
    int zero = 0;
    int page;
    // Copy all the fat contents into 8 pages
    for (page = 0; page < sb_init.fat_len; page++)
    {
        for (i = 0; i < BLOCK_SIZE / sizeof(int); i++)
        {
            memcpy((void *)(buffer + page * BLOCK_SIZE + i * (sizeof(int))),
                   (void *)&zero, sizeof(int));
        }
    }
    for (page = 0; page < sb_init.fat_len; page++)
    {
        if (block_write(sb_init.fat_idx + page, buffer + page * BLOCK_SIZE) == -1)
        {
            printf("ERROR: Failed to write FAT to disk\n");
            return -1;
        }
    }

    // After writing all meta-information, close the disk
    if (close_disk() == -1)
    {
        printf("ERROR: Failed to close disk\n");
        return -1;
    }

    free(buffer);

    // On success
    return 0;
}

int mount_fs(char *disk_name)
{
    // Check if the disk exists
    if (open_disk(disk_name) == -1)
    {
        printf("ERROR: Failed to open the file system %s\n", disk_name);
        return -1;
    }

    // Buffer for reading from the disk size of BLOCK
    char *buffer = malloc(BLOCK_SIZE);

    // Pull superblock information stored at block 0
    fs = (super_block *)malloc(sizeof(super_block));
    if (block_read(0, buffer) == -1)
    {
        printf("ERROR: Failed to pull superblock\n");
        return -1;
    }
    memcpy((void *)fs, (void *)buffer, sizeof(super_block));

    // Pull the directory information
    memset(buffer, 0, BLOCK_SIZE);
    if (block_read(fs->dir_idx, buffer) == -1)
    {
        printf("ERROR: Failed to pull directory\n");
        return -1;
    }
    memcpy((void *)DIR, (void *)buffer, sizeof(dir_entry) * MAX_F_NUM);

    // Pull the FAT information
    free(buffer);
    buffer = malloc(fs->fat_len * BLOCK_SIZE);
    int page;
    for (page = 0; page < fs->fat_len; page++)
    {
        if (block_read(fs->fat_idx + page, buffer + page * BLOCK_SIZE) == -1)
        {
            printf("ERROR: Failed to pull FAT\n");
            return -1;
        }
    }
    memcpy((void *)FAT, (void *)buffer, sizeof(int) * DISK_BLOCKS);

    // the system has a mounted fs
    mount = 1;

    // initialize the file descriptor array
    int i;
    for (i = 0; i < MAX_F_NUM; i++)
    {
        fildes[i].used = 0;
    }

    free(buffer);

    // printf("DIR INDX: %d\n", fs->dir_idx);
    // printf("DIR LENG: %d\n", fs->dir_len);
    // printf("FAT INDX: %d\n", fs->fat_idx);
    // printf("FAT LENG: %d\n", fs->fat_len);
    // printf("DATA IDX: %d\n", fs->data_idx);

    // printf("DIR USED: %d\n", DIR->used);
    // printf("DIR SIZE: %d\n", DIR->size);
    // printf("DIR NAME: %s\n", DIR->head);
    // printf("DIR REFC: %d\n", DIR->ref_cnt);

    // On success
    return 0;
}

int umount_fs(char *disk_name)
{
    // Make sure there is a disk currently mounted
    if (!mount)
    {
        printf("ERROR: No disk is currently mounted\n");
        return -1;
    }

    // Buffer for reading & writing
    char* buffer = malloc(fs->dir_len * BLOCK_SIZE);

    // Update the directory contents in the disk
    memcpy((void *)buffer, (void *)DIR, sizeof(dir_entry) * MAX_F_NUM);
    if (block_write(fs->dir_idx, buffer) == -1)
    {
        printf("ERROR: Failed to write directory to disk\n");
        return -1;
    }

    // Update the FAT contents in the disk
    free(buffer);
    buffer = malloc(fs->fat_len * BLOCK_SIZE);
    int page, i;
    for (page = 0; page < fs->fat_len; page++)
    {
        for (i = 0; i < BLOCK_SIZE / sizeof(int); i++)
        {
            memcpy((void *)buffer + page * BLOCK_SIZE + i * sizeof(int),
                   (void *)(&FAT[i]), sizeof(int));
        }
    }
    for (page = 0; page < fs->fat_len; page++)
    {
        if (block_write(fs->fat_idx + page, buffer + page * BLOCK_SIZE) == -1)
        {
            printf("ERROR: Failed to write FAT to disk\n");
            return -1;
        }
    }

    // Close the disk
    if (close_disk() == -1)
    {
        printf("ERROR: Failed to close the disk\n");
        return -1;
    }

    mount = 0;
    free(buffer);
    free(fs);

    // On Success
    return 0;
}

int fs_open(char *name)
{
    // Search for the file in the directory
    int file = 0;
    while (file < MAX_F_NUM && strcmp(DIR[file].name, name)) // strcmp returns 0 if equal
    {
        file++;
    }
    if (file == MAX_F_NUM)
    {
        printf("ERROR: File %s does not exist!\n", name);
        return -1;
    }
    if (DIR[file].used == 0)
    {
        printf("ERROR: This file has been deleted\n");
        return -1;
    }

    // Search for an open file descriptor
    int new_fildes = 0;
    while (fildes[new_fildes].used && new_fildes < MAX_FILDES)
    {
        new_fildes++;
    }
    if (new_fildes == MAX_FILDES)
    {
        printf("ERROR: Maximum number of file descriptors reached\n");
        return -1;
    }

    // Initialize the new file descriptor for the opened file
    fildes[new_fildes].used = 1;
    fildes[new_fildes].file = DIR[file].head;
    fildes[new_fildes].offset = 0;

    // Increase the reference count of the file
    DIR[file].ref_cnt++;

    // return the file descriptor on success
    return new_fildes;
}

int fs_close(int fildes_i)
{
    // check if its being used
    if (fildes[fildes_i].used == 0 || fildes_i > MAX_FILDES || fildes_i < 0)
    {
        printf("ERROR: This file descriptor is invalid\n");
        return -1;
    }

    // Check if it exists in the directory
    int i = 0;
    while (DIR[i].head != fildes[fildes_i].file && i < MAX_F_NUM)
    {
        i++;
    }
    if (i == MAX_F_NUM)
    {
        printf("ERROR: This file descriptor's file does not exist in the directory\n");
        return -1;
    }
    if (DIR[i].used == 0)
    {
        printf("ERROR: This file descriptor points to an unused file\n");
        return -1;
    }

    fildes[fildes_i].used = 0;
    fildes[fildes_i].file = 0;
    fildes[fildes_i].offset = -1;

    DIR[i].ref_cnt--;

    return 0;
}

int fs_create(char *name)
{
    // Check if name <= 15 characters
    if (strlen(name) > 15)
    {
        printf("ERROR: The name %s is too long!\n", name);
        return -1;
    }
    // Check if the file name already exists, or if max files have been reached
    int i, unused = MAX_F_NUM;
    for (i = 0; i < MAX_F_NUM; i++)
    {
        if (strcmp(DIR[i].name, name) == 0)
        {
            printf("ERROR: The name %s already exists!\n", name);
            return -1;
        }
        if (DIR[i].used == 0)
        {
            unused = i;
        }
    }
    if (unused == MAX_F_NUM)
    {
        printf("ERROR: The maximum number of files has been reached\n");
        return -1;
    }

    // Find a free space in FAT
    for (i = fs->data_idx; i < DISK_BLOCKS; i++)
    {
        if (FAT[i] == 0)
            break;
    }
    FAT[i] = -1;

    DIR[unused].used = 1;
    strcpy(DIR[unused].name, name);
    DIR[unused].size = 0;
    DIR[unused].head = i;
    DIR[unused].ref_cnt = 0;

    return 0;
}

int fs_delete(char *name)
{
    // Check if the name exists
    int i = 0;
    while ((strcmp(DIR[i].name, name) != 0) && i < MAX_F_NUM)
    {
        i++;
    }
    if (i == MAX_F_NUM)
    {
        printf("ERROR: File %s does not exist!\n", name);
        return -1;
    }
    // Check if there are any reference counts / open file descriptors
    if (DIR[i].ref_cnt > 0)
    {
        printf("ERROR: This file has open file descriptors\n");
        return -1;
    }

    // Update the FAT
    int fat_index = DIR[i].head;
    int temp_fat;
    while (FAT[fat_index] != -1)
    {
        if (FAT[fat_index] == 0)
        {
            printf("ERROR: Fat is initialized wrong\n");
            return -1;
        }

        temp_fat = fat_index;
        fat_index = FAT[fat_index];
        FAT[temp_fat] = 0;
    }
    FAT[fat_index] = 0;

    DIR[i].used = 0;
    memset(DIR[i].name, '\0', 15);
    DIR[i].size = 0;
    DIR[i].head = 0;

    // On success
    return 0;
}

int fs_read(int fildes_i, void *buf, size_t nbyte)
{
    // Check if fildes is valid
    if (fildes[fildes_i].used == 0 || fildes_i > MAX_FILDES || fildes_i < 0)
    {
        printf("ERROR: This file descriptor is invalid\n");
        return -1;
    }
    // Check if nbyte is valid
    if (nbyte <= 0)
    {
        printf("ERROR: Can't read <= 0 bytes\n");
        return -1;
    }

    // Find the directory entry
    int dir_index = 0;
    while (DIR[dir_index].head != fildes[fildes_i].file)
    {
        dir_index++;
    }
    if (DIR[dir_index].used == 0)
    {
        printf("ERROR: This file is not being used\n");
        return -1;
    }

    // Create a larger than necessary buffer to read all of the bytes
    int num_blocks = DIR[dir_index].size / BLOCK_SIZE + 1;
    char *buffer = malloc(BLOCK_SIZE * num_blocks);

    // Read the full file
    int fat_index = fildes[fildes_i].file;
    int block_index = 0;
    while (fat_index != -1)
    {
        printf("fat index = %d\n", fat_index);
        if (block_read(fat_index, buffer + block_index * BLOCK_SIZE) == -1)
        {
            printf("ERROR: Failed to do preliminary read from file\n");
            return -1;
        }
        fat_index = FAT[fat_index];
        block_index++;
    }

    // Copy nbytes after the fildes offset into buf
    memcpy((void *)buf, (void *)(buffer + fildes[fildes_i].offset), nbyte);

    // Check offset + nbyte against the size of the file
    int bytes_read;
    if (fildes[fildes_i].offset + nbyte > DIR[dir_index].size)
    {
        // Set file to be the end of the file
        bytes_read = DIR[dir_index].size - fildes[fildes_i].offset;
        fildes[fildes_i].offset = DIR[dir_index].size;
    }
    else
    {
        // Increase offset by the number of bytes read
        bytes_read = nbyte;
        fildes[fildes_i].offset += nbyte;
    }

    // On success, return the number of bytes read
    return bytes_read;
}

int fs_write(int fildes_i, void *buf, size_t nbyte)
{
    // Check if the file desciptor is valid
    if (fildes[fildes_i].used == 0 || fildes_i > MAX_FILDES || fildes_i < 0)
    {
        printf("ERROR: This file descriptor is not being used\n");
        return -1;
    }
    // Check if nbyte is valid
    if (nbyte < 0)
    {
        printf("ERROR: Can't write < 0 bytes\n");
        return -1;
    }

    // Make sure we do not exceed the maximum file size
    if ((fildes[fildes_i].offset + nbyte) > MAX_F_SIZE)
    {
        nbyte = MAX_F_SIZE - fildes[fildes_i].offset;
    }

    // Search for the directory index
    int dir_index = 0;
    while (DIR[dir_index].head != fildes[fildes_i].file && dir_index < MAX_F_NUM)
    {
        dir_index++;
    }
    if (dir_index == MAX_F_NUM || DIR[dir_index].used == 0)
    {
        printf("ERROR: fs_write - File not found\n");
        return -1;
    }

    // Update the FAT as necessary
    int fat_index = fildes[fildes_i].file;
    int i, j;
    for (i = 0; i < (fildes[fildes_i].offset + nbyte) / BLOCK_SIZE; i++)
    {
        // If there is not already a next page, make the next one
        if (FAT[fat_index] == -1)
        {
            for (j = fs->data_idx; j < DISK_BLOCKS; j++)
            {
                if (FAT[j] == 0)
                {
                    FAT[fat_index] = j;
                    fat_index = j;
                    FAT[fat_index] = -1;
                    break;
                }
            }
            if (j == DISK_BLOCKS)
            {
                printf("ERROR: There are no more empty blocks\n");
            }
        }
    }

    // Update the size of the directory if necessary
    if (fildes[fildes_i].offset + nbyte > DIR[dir_index].size)
    {
        DIR[dir_index].size = fildes[fildes_i].offset + nbyte;
    }

    // buffer for writing to the disk
    int num_blocks = DIR[dir_index].size / BLOCK_SIZE + 1;
    char *buffer = malloc(BLOCK_SIZE * num_blocks);

    // In order to write to the file, pull the file down then overwrite the necessary offset
    // Read the file
    fat_index = fildes[fildes_i].file;
    int block_index = 0;
    while (fat_index != -1)
    {
        if (block_read(fat_index, buffer + block_index * BLOCK_SIZE) == -1)
        {
            printf("ERROR: Failed to do preliminary read from file\n");
            return -1;
        }
        fat_index = FAT[fat_index];
        block_index++;
    }

    // Write to our read file buffer
    memcpy((void *)buffer + fildes[fildes_i].offset, (void *)buf, nbyte);

    // re-write into the file
    fat_index = fildes[fildes_i].file;
    block_index = 0;
    while (fat_index != -1)
    {
        if (block_write(fat_index, buffer + block_index * BLOCK_SIZE) == -1)
        {
            printf("ERROR: Failed to write back to the file\n");
            return -1;
        }
        fat_index = FAT[fat_index];
        block_index++;
    }

    // Increase the offset by number of written bytes
    fildes[fildes_i].offset += nbyte;

    return nbyte;
}

int fs_get_filesize(int fildes_i)
{
    // Check the validity of the file descriptor
    if (fildes[fildes_i].used == 0)
    {
        printf("ERROR: This file descriptor is invalid\n");
        return -1;
    }
    
    // Find the corresponding directory entry
    int i = 0;
    while (DIR[i].head != fildes[fildes_i].file)
    {
        i++;
    }

    // Return size
    return DIR[i].size;
}

int fs_listfiles(char ***files)
{
    // Allocate space for the number of files in DIR
    int i, counter = 0;
    for (i = 0; i < MAX_F_NUM; i++)
    {
        if (DIR[i].used)
        {
            counter++;
        }
    }
    printf("%d\n", counter);

    // allocate size for a char**
    files = (char***) malloc(sizeof(char**));

    // dereference to get the array of strings
    *files = (char**) calloc(counter + 1, sizeof(char*));
    printf("1.1\n");

    // For each string in the array, allocate MAX_F_NAME chars
    for (i = 0; i < counter + 1; i++)
    {
        (*files)[i] = (char*) malloc(sizeof(char) * MAX_F_NAME);
    }

    // Last array element should be nullchar
    printf("1.2\n");
    (*files)[counter] = '\0';
    printf("1.3\n");
    
    // iterate through the dirctory and copy the filenames
    counter = 0;
    for (i = 0; i < MAX_F_NUM; i++)
    {
        if (DIR[i].used)
        {
            strncpy((*files)[counter], DIR[i].name, sizeof(char) * MAX_F_NAME);
            counter++;
        }
    }
    printf("1.4\n");

    return 0;
}

int fs_lseek(int fildes_i, off_t offset)
{
    // Check validity of the file descriptor
    if (fildes[fildes_i].used == 0)
    {
        printf("ERROR: This file descriptor is not valid\n");
        return -1;
    }
    if (offset < 0)
    {
        printf("ERROR: offset cannot be less than 0!\n");
        return -1;
    }

    // Locate the directory index to check the size
    int i = 0;
    while (DIR[i].head != fildes[fildes_i].file)
    {
        i++;
    }

    if (offset > DIR[i].size)
    {
        printf("ERROR: This will set the pointer past the eof\n");
        return -1;
    }

    fildes[fildes_i].offset = offset;
    // printf("The new offset is %d\n", fildes[fildes_i].offset);

    // On success
    return 0;
}

int fs_truncate(int fildes_i, off_t length)
{
    // Check validity of fildes_i
    if (fildes[fildes_i].used == 0 || fildes_i > MAX_FILDES || fildes_i < 0)
    {
        printf("ERROR: This file descriptor is invalid\n");
        return -1;
    }
    // Find corresponding entry in DIR
    int file = 0;
    while (DIR[file].head != fildes[fildes_i].file && file < MAX_F_NUM)
    {
        file++;
    }
    if (file == MAX_F_NUM || DIR[file].used == 0)
    {
        printf("ERROR: The file descriptor points to an invalid file");
        return -1;
    }
    // Make sure length is less than size
    if (length > DIR[file].size)
    {
        printf("ERROR: The provided length is greater than the size of the file\n");
        return -1;
    }

    // free the extra blocks that come AFTER length and update the FAT
    int start_block = length / BLOCK_SIZE;
    int new_last = DIR[file].head, temp_fat, fat_index, i;
    for (i = 0; i < start_block + 1; i++)
    {
        new_last = FAT[new_last];
    }
    fat_index = FAT[new_last];
    FAT[new_last] = -1;

    char* buf = malloc(BLOCK_SIZE);

    while (fat_index != -1)
    {
        if (block_write(fat_index, buf) == -1)
        {
            printf("ERROR: Failed to free the block\n");
            return -1;
        }
        temp_fat = fat_index;
        fat_index = FAT[fat_index];
        FAT[temp_fat] = 0;
    }

    // Free the bytes that come after length
    if (block_read(new_last, buf) == -1)
    {
        printf("ERROR: fs_truncate - Failed to read from disk\n");
        return -1;
    }
    int leftover = BLOCK_SIZE - (length % BLOCK_SIZE);
    char* blank = malloc(leftover);
    memcpy((void*) (buf + length), (void*) blank, leftover);

    // write back the new deleted data file
    if (block_write(new_last, buf) == -1)
    {
        // printf("RROR: fs_truncate - Failed to write back to disk\n");
        return -1;
    }

    // If offset was larger than the new length, set it to new length
    if (fildes[fildes_i].offset > length)
    {
        fildes[fildes_i].offset = length;
    }

    free(buf);
    free(blank);

    return 0;
}