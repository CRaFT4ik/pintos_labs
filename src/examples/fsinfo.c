/* This file coded by Eldar Timraleev. */

/* Выводит информацию о файловой системе. */

#include <stdio.h>
#include <syscall.h>

int main(int argc UNUSED, const char* argv[] UNUSED)
{
    int fsinfo_buf[3];
    fsinfo(fsinfo_buf);

    printf("\nFSINFO:\n\n");
    printf("Files on disk: %d\n", fsinfo_buf[0]);
    printf("Disk size: %d kB (%d bytes)\n", fsinfo_buf[1] / 1024, fsinfo_buf[1]);
    printf("Occupied space: %d kB (%d bytes)\n", fsinfo_buf[2] / 1024, fsinfo_buf[2]);
    printf("Free space: %d kB (%d bytes)\n", fsinfo_buf[3] / 1024, fsinfo_buf[3]);
    printf("\n");

    return EXIT_SUCCESS;
}
