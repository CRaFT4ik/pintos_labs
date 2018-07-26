/* This file coded by CRaFT4ik (Eldar Timraleev) */

/* Создает 2 потока с уникальными именами. Каждый из
   потоков исполняется в бесконечном цикле, периодически (раз в секунду) распечатывает в
   консоль свое имя. Основная функция теста после создания потоков ожидает 4 секунды, затем
   приостанавливает поток 1, ожидает еще 4 секунды, возобновляет его выполнение, ожидает
   еще 4 секунды и завершается. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "devices/timer.h"

static void thread_function(void *);

void test_threads_pause_resume(void) 
{
    const int num_of_threads = 2;
    const int time_to_sleep = 4; // в секундах
    
    tid_t tid_first;
    enum intr_level old_level;
	
    /* Этот тест не будет работать с MLFQS. */
    ASSERT (!thread_mlfqs);
    
    /* Создание и запуск потоков. */
    tid_first = thread_create("thread 1", PRI_DEFAULT, thread_function, NULL);
    		 	thread_create("thread 2", PRI_DEFAULT, thread_function, NULL);
	
    /* Выполнение задачи основной функции. */
    timer_msleep(time_to_sleep * 1000);
	printf("sleep: thread 1\n");
	thread_pause(tid_first);
	
	timer_msleep(time_to_sleep * 1000);
	printf("wake: thread 1\n");
	thread_resume(tid_first);
	
	timer_msleep(time_to_sleep * 1000);
}

/* Поток. */
static void thread_function(void *_t UNUSED) 
{
    while (true)
	{
		printf("%s running, tick %d\n", thread_name(), timer_ticks());
		timer_msleep(1000);
	}
}
