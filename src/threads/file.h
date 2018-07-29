#ifndef PLANNER_FILE_H
#define PLANNER_FILE_H

#include "filesys/filesys.h"

bool create(const char *, unsigned);
bool remove(const char *);
int open(const char *);
int filesize(int);
int read(int, void *, unsigned);
int write(int, const void *, unsigned);
void close(int);

#endif
