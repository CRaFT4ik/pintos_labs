/* This file coded by CRaFT4ik (Eldar Timraleev) */

#include "threads/alarm.h"
#include <debug.h>
#include <stdio.h>
#include <inttypes.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "devices/timer.h"

static void sort(void);
static void add_to_list(struct thread *);

/* Массив спящих процессов SLEEPING_THREADS и их количество SLEEPING_NUM. */
static struct thread **sleeping_threads;
static int sleeping_num = 0;

/* Устанавливает будильник текущему потоку на время пробуждения
   WAKE_UP_TICKS, чтобы разбудить его в это время. */
void alarm_set(int64_t wake_up_ticks)
{
	struct thread *t;
	
	/* Критическая секция, прерывания должны быть выключены. */
	ASSERT (intr_get_level() == INTR_OFF);
	
	t = thread_current();
	ASSERT (t->status == THREAD_RUNNING);
	
	t->wake_up_ticks = wake_up_ticks;
	
	add_to_list(t);
}

/* Добавляет поток в массив спящих потоков. */
static void add_to_list(struct thread *t)
{
	if (sleeping_num == 0) sleeping_threads = malloc(sizeof(struct thread *));
	else sleeping_threads = realloc(sleeping_threads, sizeof(struct thread *) * (sleeping_num + 1));
	
	ASSERT (sleeping_threads != NULL);
	
	*(sleeping_threads + sleeping_num++) = t;
	sort();
}

/* Удаляет будильник для потока T.
   Поток должен быть в состоянии THREAD_SLEEPING.
   Прерывания должны быть выключены. */
void alarm_remove(struct thread *t)
{
	ASSERT (intr_get_level() == INTR_OFF);
	ASSERT (t->status == THREAD_SLEEPING);
	
	for (int i = sleeping_num - 1; i >= 0; i--)
	{
		if (*(sleeping_threads + i) != t) continue;
		
		sleeping_num--;
		if (i == sleeping_num) break;
		
		*(sleeping_threads + i) = *(sleeping_threads + (--sleeping_num));
		sort();
		break;
	}
}

/* Проверяет текущий массив спящих потоков на наличие
   потоков, готовых к пробуждению на данном тике.
   Вызывается обрабочиком прерываний timer_interrupt(). */
void alarm_check(void)
{
	ASSERT (intr_context()); /* Проверяем, что мы сейчас находимся в прерывании. */
	
	struct thread *t;
	
	/* Поскольку массив отсортирован по времени сна от большего к меньшему,
       проверку будем останавливать тогда, когда окажется, что следующий
	   проверяемый поток еще не должен быть разбужен. К примеру,
	   если необходимо разбудить поток N, имеет смысл проверить дальше
	   поток N-1. Но мы не станем этого делать, если поток N будить не нужно. */
	for (int i = sleeping_num - 1; i >= 0; i--)
	{
		t = *(sleeping_threads + i);
		if (t->wake_up_ticks <= timer_ticks()) thread_wake(t);
		else break;
	}
}

/* Сортирует массив спящих потоков по времени сна (тикам) от большего к меньшему. */
static void sort(void)
{
	struct thread *t_tmp;
	for (int i = 1; i < sleeping_num; i++)
		for (int j = i; j > 0 && (*(sleeping_threads + j - 1))->wake_up_ticks < (*(sleeping_threads + j))->wake_up_ticks; j--)
		{
			t_tmp = *(sleeping_threads + j - 1);
			*(sleeping_threads + j - 1) = *(sleeping_threads + j);
			*(sleeping_threads + j) = t_tmp;
		}
}
