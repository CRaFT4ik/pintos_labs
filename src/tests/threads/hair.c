/*
  Implemented by Eldar Timraleev, 2017.
  Solution by semaphores;
*/

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/timer.h"

struct semaphore chair;     // Кресло. На нем спит парикмахер.
struct semaphore clients;   // Очередь клиентов.
int clients_count;
bool sleeping;

static void init(void)
{
    sema_init(&chair, 0);
    sema_init(&clients, 0);

    clients_count = 0;
    sleeping = 0;
}

static void hairdresser(void* arg UNUSED)
{
    msg("Создан парикмахер.");

    while (true)
    {
        msg("Парикмахер уснул.");
        sleeping = 1;
        sema_down(&chair);
        sleeping = 0;
        msg("Парикмахер разбужен.");
        while (clients_count)
        {
            msg("Начал обслуживать клиента [%d].", timer_ticks());
            timer_sleep(10);
            sema_up(&clients);
        }
    }
}

static void client(void* arg UNUSED)
{
    msg("Клиент %d создан.", (int) arg);

    clients_count++;
    if (sleeping) sema_up(&chair);

    sema_down(&clients);
    msg(" * %s обслужен [%d].", thread_name(), timer_ticks());
    clients_count--;
}

void test_hair(unsigned int num_clients, unsigned int interval)
{
  unsigned int i;
  init();

  thread_create("hairdresser", PRI_DEFAULT, &hairdresser, NULL);

  for(i = 0; i < num_clients; i++)
  {
    char name[32];
    timer_sleep(interval);
    snprintf(name, sizeof(name), "client_%d", i + 1);
    thread_create(name, PRI_DEFAULT, &client, (void*) (i+1) );
  }

  timer_msleep(5000);
  pass();
}
