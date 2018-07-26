/* This file coded by Eldar Timraleev. */

/* Выводит информацию о RAM. */

#include <stdio.h>
#include <syscall.h>

int main(int argc UNUSED, const char* argv[] UNUSED)
{
    int memstat_buf[3];
    memstat(memstat_buf);

    printf("\nMEMSTAT INFO:\n\n");
    printf("Total RAM volume: %d kB (%d bytes)\n", memstat_buf[0] / 1024, memstat_buf[0]);
    printf("Free RAM volume: %d kB (%d bytes)\n", memstat_buf[1] / 1024, memstat_buf[1]);
    printf("Occupied RAM volume: %d kB (%d bytes)\n", memstat_buf[2] / 1024, memstat_buf[2]);
    printf("\n");

    return EXIT_SUCCESS;
}
