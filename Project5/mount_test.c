#include <stdio.h>
#include "fs.h"

int main()
{
    char *disk1 = "firstdisk";
    char *file = "firstfile";
    char *file2 = "secondfile";
    // char* file3 = "thirdfile";

    // if (make_fs(disk1) == -1) return -1;
    // else printf("make Completed successfully\n");
    if (mount_fs(disk1) == -1)
        return -1;
    else
        printf("mount Completed successfully\n");

    // if (fs_create(file) == -1) return -1;
    // else printf("file %s created successfully\n", file);
    // if (fs_create(file2) == -1) return -1;
    // else printf("file %s created successfully\n", file2);
    // if (fs_create(file3) == -1) return -1;
    // else printf("file %s created successfully\n", file3);

    // if (fs_delete(file) == -1) return -1;
    // else printf("file %s deleted successfully\n", file);
    // if (fs_delete(file2) == -1) return -1;
    // else printf("file %s deleted successfully\n", file2);
    // if (fs_delete(file3) == -1) return -1;
    // else printf("file %s deleted successfully\n", file3);

    int fildes = fs_open(file);
    if (fildes == -1)
        return -1;
    // char *buf = "1234567890 0987654321 ";


    // int i = 0;
    // for (i = 0; i < 200; i++)
    // {
    //     if (fs_write(fildes, (void *)buf, 23) == -1)
    //         return -1;
    //     else
    //         printf("write: %s\n", buf);
    // }

    if (fs_lseek(fildes, 4095) == -1)
        return -1;
    else
        printf("offset set\n");

    char *receive = (char *)malloc(sizeof(char) * 100);
    if (fs_read(fildes, (void *)receive, 15) == -1)
        return -1;
    else
        printf("read: %s\n", receive);

    // int fildes2 = fs_open(file2);
    // if (fildes2 == -1) return -1;
    // char* buf2 = "this is not a string";

    // if (fs_write(fildes2, (void*) buf2, 21) == -1) return -1;
    // else printf("write: %s\n", buf2);

    // if (fs_lseek(fildes2, 0) == -1) return -1;
    // else printf("offset set to 0\n");

    // char* receive2 = (char*) malloc(sizeof(char) * 100);
    // if (fs_read(fildes2, (void*) receive2, 21) == -1) return -1;
    // else printf("read: %s\n", receive2);

    if (fs_close(fildes) == -1)
        return -1;
    else
        printf("closed file descriptor\n");
    // if (fs_close(fildes2) == -1) return -1;
    // else printf("closed file descriptor\n");
    // char*** files;
    // int i = 0;
    // fs_listfiles(files);
    // printf("after func\n");

    // while ((*files)[i] != '\0')
    // {
    //     printf("%s ", (*files)[i]);
    //     i++;
    // }
    // printf("\n");
    // free(files);

    if (umount_fs(disk1) == -1)
        return -1;
    else
        printf("umount Completed successfully\n");

    return 0;
}