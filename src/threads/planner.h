/* Coded by Arina. */

#include <devices/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/io.h"

struct task
{
	int id;
	char name[128];
	time_t time; // Время в секундах с 1 Января 1970 года.
};

void add_task(char *, char *);
time_t get_local_time(void);
void planner_init(void);
