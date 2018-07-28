/* Coded by Arina. */

#include "threads/planner.h"

void task_remove(struct task *);
void sort_tasks(void);
void show(void);

static struct task **list = NULL;
static int list_num = 0;

void planner_init(void)
{
	
}

/* Возвращает местное время в секундах с 1 Января 1970 года. */
time_t get_local_time(void)
{
	return rtc_get_time() + 3 * 60 * 60;
}

/* Добавляет задачу в массив спящих потоков.
   Строка TIME имеет формат: гггг:мм:дд:чч:мм:сс. */
void add_task(char *name, char *time)
{
	struct task *t = malloc(sizeof(struct task));
	ASSERT (t != NULL);

	static const int days_per_month[12] =
	  {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	  };
	int sec, min, hour, mday, mon, year;
	time_t seconds = 0;
	char *save_ptr;

	char *tmp = strtok_r(time, ":", &save_ptr);
    year = (atoi(tmp) - 1970);
	seconds = (year * 365 + (year - 1) / 4) * 24 * 60 * 60;

	tmp = strtok_r(NULL, ":", &save_ptr);
	mon = atoi(tmp);
    for (int i = 1; i <= mon; i++)
      seconds += days_per_month[i - 1] * 24 * 60 * 60;
    if (mon > 2 && year % 4 == 0)
      seconds += 24 * 60 * 60;

	tmp = strtok_r(NULL, ":", &save_ptr);
  	mday = atoi(tmp);
    seconds += (mday - 1) * 24 * 60 * 60;

	tmp = strtok_r(NULL, ":", &save_ptr);
  	hour = atoi(tmp);
    seconds += hour * 60 * 60;

	tmp = strtok_r(NULL, ":", &save_ptr);
  	min = atoi(tmp);
    seconds += min * 60;

	tmp = strtok_r(NULL, ":", &save_ptr);
  	sec = atoi(tmp);
    seconds += sec;

	t->time = seconds;
	ASSERT (strlen(name) < 128);
	memcpy(t->name, name, strlen(name) + 1);

	list = realloc(list, sizeof(struct task *) * (list_num + 1));
	ASSERT (list != NULL);

	*(list + list_num++) = t;
	sort_tasks();
	show();
}

void sort_tasks(void)
{
	struct task *t_tmp;
	for (int i = 1; i < list_num; i++)
		for (int j = i; j > 0 && (*(list + j - 1))->time > (*(list + j))->time; j--)
		{
			t_tmp = *(list + j - 1);
			*(list + j - 1) = *(list + j);
			*(list + j) = t_tmp;
		}
}

/* Удаляет задачу из списка. */
void task_remove(struct task *t)
{

}

void show(void)
{
	printf("****************************************\n");

	for (int i = 0; i < list_num; i++)
	{
		printf("%s\n", list[i]->name);
	}
}
