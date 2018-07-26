/* This file coded by CRaFT4ik (Eldar Timraleev) */

/* Создает 5 процессов для демонстрации работы алгоритма планирования процессов SJF
   с установленным CPU burst: 10, 2, 7, 3, 6 и приоритетом: 5, 15, 29, 60, 59 для каждого процесса соответственно.
   Каждый процесс выполняется некоторое время, выводя при этом свое имя и статистику (сколько «тиков» процесс выполняется, сколько осталось).
   Работы теста завершается тогда, когда все процессы завершат свое выполнение. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/timer.h"

static thread_func process;

const unsigned time_to_work   = 10;                     // Общее время работы каждого процесса (0, если = CPU burst)
const unsigned threads_num    = 5;                     // Количество процессов
const unsigned th_CPU_burst[] = { 10, 2, 7, 3, 6 };    // CPU burst для каждого процесса
const unsigned th_priority[]  = { 5, 15, 60, 60, 59 }; // Приоритет для каждого процесса

struct lock lock;

void test_new_alg(void)
{
    printf("Running the test (see description in 'test_new_alg.c').\n");

    lock_init(&lock);
    lock_acquire(&lock);

    thread_set_priority(PRI_MIN);

    for (unsigned i = 0; i < threads_num; i++)
    {
        char name[16];
        snprintf(name, sizeof name, "Proc%d", i);
        thread_create_(name, th_priority[i], th_CPU_burst[i], process, NULL);
    }

    lock_release(&lock);
}

static void process(void *_data UNUSED)
{
    lock_acquire(&lock);
    printf("%s: running with priority %2d and CPU burst %d\n", thread_name(), thread_current()->priority, thread_current()->CPU_burst);

    int64_t start_ticks = timer_ticks();
    int64_t timer = 0;
    int tick_count = 0;
    const int proc_time_to_work = (time_to_work > 0) ? time_to_work : thread_current()->CPU_burst;

    while (tick_count < proc_time_to_work)
    {
        if (timer != timer_ticks())
        {
            tick_count++;
            timer = timer_ticks();
            printf("%s: running, cur_ticks %d\n", thread_name(), (int) timer_ticks());
        }
    }

    printf("%s: EXIT, timer_elapsed %d\n", thread_name(), (int) timer_elapsed(start_ticks));
    lock_release(&lock);
}
