/* This file coded by CRaFT4ik (Eldar Timraleev) */

#ifndef THREADS_ALARM_H
#define THREADS_ALARM_H

#include <inttypes.h>
#include "threads/thread.h"

void alarm_set(int64_t);
void alarm_remove(struct thread *);
void alarm_check(void);

#endif /* threads/alarm.h */