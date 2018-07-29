/* Coded by Arina. */

#include "threads/planner.h"
#include "threads/file.h"

void add_task_(int, char *, time_t);
void write_txt(void);
void task_remove(struct task *);
void sort_tasks(void);
void show(void);

#define filename "planner.txt"

static struct task **list = NULL;
static int last_id = 0;
static int list_num = 0;

/* Первичная инициализация. */
void planner_init(void)
{
	int fd = open(filename);
	if (fd == -1) return;

	int size = filesize(fd);
	char *txt = (char *) malloc(sizeof(char)* (size + 1));
	ASSERT (txt != NULL);
	memset(txt, 0, size + 1);
	ASSERT (read(fd, txt, size) != -1);
	close(fd);

	time_t time;
	char *name;
	int id;

	char *save_ptr1, *save_ptr2;
	char *tmp = strtok_r(txt, "\n", &save_ptr1);
	while (tmp != NULL)
	{
		id = atoi(strtok_r(tmp, "::", &save_ptr2));
		name = strtok_r(NULL, "::", &save_ptr2);
		time = atoi(strtok_r(NULL, "::", &save_ptr2));
		add_task_(id, name, time);

		save_ptr2 = NULL;
		tmp = strtok_r(NULL, "\n", &save_ptr1);
	}
}

void write_txt(void)
{
	char *buf = NULL;
	int total_len = 0;
	for (int i = 1; i < list_num; i++)
	{
		char str[256];
		snprintf(str, sizeof(str), "%d::%s::%ul\n", list[i]->id, list[i]->name, list[i]->time);
		total_len += strlen(str);
		buf = (char *) realloc(buf, sizeof(char) * (total_len + 1));
		memcpy(buf + total_len - strlen(str), str, strlen(str) + 1);
	}

	remove(filename);
	ASSERT (create(filename, total_len) != false);
	int fd = open(filename);
	ASSERT (fd != -1);
    ASSERT (write(fd, buf, total_len) != -1);
	close(fd);
}

/* Возвращает местное время в секундах с 1 Января 1970 года. */
time_t get_local_time(void)
{
	return rtc_get_time() + 3 * 60 * 60;
}

void add_task_(int id, char *name, time_t seconds)
{
	struct task *t = malloc(sizeof(struct task));
	ASSERT (t != NULL);

	t->id = id;
	t->time = seconds;
	ASSERT (strlen(name) < 128);
	memcpy(t->name, name, strlen(name) + 1);

	list = realloc(list, sizeof(struct task *) * (list_num + 1));
	ASSERT (list != NULL);

	*(list + list_num++) = t;
	sort_tasks();
	write_txt();
	show();
}

/* Добавляет задачу в массив спящих потоков.
   Строка TIME имеет формат: гггг:мм:дд:чч:мм:сс. */
void add_task(char *name, char *time)
{
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

	add_task_(++last_id, name, seconds);
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

void show(void)
{
	printf("****************************************\n");

	for (int i = 0; i < list_num; i++)
	{
		printf("%s\n", list[i]->name);
	}
}
