#ifndef FS_H
#define FS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

int make_fs(char *disk_name);
/*
    Create a new file system with the name disk_name
    invoke make_disk in disk.c
    open the disk and write the necessary meta-information

    return 0 on success, -1 on failure (if disk could not be made, opened, or properly initialized)
*/

int mount_fs(char *disk_name);
/*
    Mount the file system on the disk with disk_name
    open the disk and load the meta-information that is necessary to handle the fs operations

    return 0 on success, -1 on failure (if disk_name could not be opened, or if it does not contain a valid fs)
*/

int umount_fs(char *disk_name);
/*
    Unmount the file system from the virtual disk disk_name
    Write back all meta information so that the disk persistently reflects all changes
    Close the disk

    return 0 on success, -1 on failure (if disk_name could not be closed, or if data could not be written)
*/

int fs_open(char *name);
/*
    Open the file name for reading/writing
    The same file should be able to be opened multiple times
    If opened multiple times, it should provide multiple independent file descriptors 
    On open, the file offset should point to 0

    return a non-negative integer on success (which should be the file descriptor that can be used to access the system)
    return -1 on failure (if the file name cannot be found, or if there are already MAX32 file descriptors)
*/

int fs_close(int fildes);
/*
    Close the file descriptor fildes
    A closed file descriptor should not be able to access the corresponding file

    return 0 on success, -1 on failure (if fildes does not exist or is not open)
*/

int fs_create(char *name);
/*
    Create a new file with the name name in the root directory of the file system
    This file should be initially empty, and name <= 15 characters
    There can be at most 64 files in a directory

    return 0 on success, -1 on failure (if name already exists, if name is too long, if max files is reached)
*/

int fs_delete(char *name);
/*
    Delete the file name from the root directory
    Free all data blocks and meta information that correspond to that file
    If the file is open (has a fildes), then the call should fail and the file should not be deleted

    return 0 on success, -1 on failure (if name does not exist, if there is at least one fildes associated with this file)
*/

int fs_read(int fildes, void *buf, size_t nbyte);
/*
    Read nbytes of data from the file referenced to by fildes into buf
    Assume buf is large enough for nbyte bytes
    When the file tries to read past eof, read all bytes until the eof
    Also increments the file pointer by the number of read bytes

    returns the number of bytes that were actually read on success (could be less than nbyte)
    return -1 on failure (if fildes is invalid)
*/

int fs_write(int fildes, void *buf, size_t nbyte);
/*
    Write nbytes of data into the file referenced to by fildes from buf
    Assume buf can hold nbytes of data
    When the file tries to write past eof, then automatically extend the file to hold the additional bytes
    If the disk runs out of space while trying to write, then write as many bytes as possible
    Increment the file pointer by the number of written bytes

    return the number of bytes that were actually written to the file on success (could be less than nbytes)
    return -1 on failure (if fildes is invalid)
*/

int fs_get_filesize(int fildes);
/*
    return the current size of the file referenced by fildes
    return -1 if fildes is invalid
*/

int fs_listfiles(char ***files);
/*
    Create and populate an array of all the file names currently known in the file system
    There should be a NULL pointer after the last element
    
    return 0 on success, -1 on failure
*/

int fs_lseek(int fildes, off_t offset);
/*
    Sets file pointer associated with fildes to the argument offset
    Trying to set the file pointer past the eof is an error
    To append to a file, set the file pointer to the end of the file

    return 0 on success, -1 on failure (if fildes is invalid, if offset > filesize, if offset < 0)
*/

int fs_truncate(int fildes, off_t length);
/*
    Truncate the file referenced by fildes to be length in size
    If the file was previously larger than length, then the extra data is lost and should be freed
    It is not possible to extend a file using truncate
    If the file pointer is currently set to >length, then it should be set to length

    return 0 on success, -1 on failure (if fildes is invalid, or if length > filesize)
*/

#endif