#ifndef PLANNER_FILE_H
#define PLANNER_FILE_H

#include <stdio.h>

void file_init(void);
bool fcreate(const char *, unsigned);
bool fremove(const char *);
int fopen(const char *);
int fsize(int);
int fread(int, void *, unsigned);
int fwrite(int, const void *, unsigned);
void fclose(int);

#endif
