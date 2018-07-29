/* Coded by Arina. */

#include "userprog/planner.h"
#include "userprog/file.h"
#include "threads/thread.h"
#include "devices/timer.h"

void add_task_(int, char *, time_t);
void write_txt(void);
void task_remove(struct task *);
void sort_tasks(void);
void list_show(void);
void planner_thread(void);
void add_test_tasks(void);

#define filename "planner.txt"

static struct task **list = NULL;
static int last_id = 0;
static int list_num = 0;

static tid_t tid;

/* Первичная инициализация. */
void planner_init(void)
{
 	int fd = fopen(filename);
	if (fd == -1) // Не удалось открыть файл.
	{
		printf("[PLANNER] Config file isn't detected on this disk.\n");
		goto s;
	}

	int size = fsize(fd);
	char *txt = (char *) malloc(sizeof(char)* (size + 1));
	ASSERT (txt != NULL);
	memset(txt, 0, size + 1);
	ASSERT (fread(fd, txt, size) != -1);
	fclose(fd);

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

s:	//add_test_tasks();
	list_show();

	printf("[PLANNER] Planner loaded.\n");

	/* Создание демона. */
	tid = thread_create("planner", PRI_DEFAULT, planner_thread, NULL);
	printf("[PLANNER] Planner daemon executed.\n");
}

/* Тело демона (планировщика).
   Интервал выполнения и проверки задач - 1 раз в секунду. */
void planner_thread(void)
{
	while (true)
	{
		//process_execute(name);
		printf("[PLANNER DAEMON] %s %ld\n", thread_current()->name, get_local_time());
		thread_sleep(TIMER_FREQ); // Интервал проверки задач таймером.
	}
}

void write_txt(void)
{
	char *buf = NULL;
	int total_len = 0;
	for (int i = 0; i < list_num; i++)
	{
		char str[256];
		snprintf(str, sizeof(str), "%d::%s::%ld\n", list[i]->id, list[i]->name, list[i]->time);
		total_len += strlen(str);
		buf = (char *) realloc(buf, sizeof(char) * (total_len + 1));
		memcpy(buf + total_len - strlen(str), str, strlen(str) + 1);
	}

	fremove(filename);
	ASSERT (fcreate(filename, total_len) != false);

	int fd = fopen(filename);
	ASSERT (fd != -1);

    ASSERT (fwrite(fd, buf, total_len) != -1);
	fclose(fd);
}

/* Возвращает московское время в секундах с 1 Января 1970 года. */
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

void list_show(void)
{
	printf("[PLANNER] List of planned tasks:\n");
	for (int i = 0; i < list_num; i++)
		printf("[PLANNER] %s %ld\n", list[i]->name, list[i]->time);
	printf("[PLANNER] End of listing.\n");
}

/* Создает задачи в демонстрационных целях. */
void add_test_tasks(void)
{
	/* Coded by Arina. */
    char bufn[128], buft[128];
    snprintf(bufn, sizeof(bufn), "Arina_1");
    snprintf(buft, sizeof(buft),"1999:05:05:15:02:00");
    add_task(bufn, buft);

    snprintf(bufn, sizeof(bufn), "Arina_0");
    snprintf(buft, sizeof(buft),"1999:05:05:15:01:59");
    add_task(bufn, buft);

    snprintf(bufn, sizeof(bufn), "Arina_2");
    snprintf(buft, sizeof(buft), "1999:05:05:15:02:01");
    add_task(bufn, buft);
}
