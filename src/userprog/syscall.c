/* This file coded by Eldar. */

#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/string.h"
#include <lib/user/syscall.h>
#include "devices/shutdown.h"
#include "devices/input.h"
#include "process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/loader.h"
#include "devices/block.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "userprog/planner.h" // Coded by Arina.

static void syscall_handler(struct intr_frame *);

static struct file *find_file_by_fd(int fd);
static struct fd_elem *find_fd_elem_by_fd(int fd);
static struct fd_elem *find_fd_elem_by_fd_in_process(int fd);

void halt(void);
void exit(int);
pid_t exec(const char *);
int wait(pid_t);
bool create(const char *, unsigned);
bool remove(const char *);
int open(const char *);
int filesize(int);
int read(int, void *, unsigned);
int write(int, const void *, unsigned);
void seek(int, unsigned);
unsigned tell(int);
void close(int);
mapid_t mmap(int, void *);
void munmap(mapid_t);
bool chdir(const char *);
bool mkdir(const char *);
bool readdir(int, char name[READDIR_MAX_LEN + 1]);
bool isdir(int);
int inumber(int);
void memstat(int buffer[3]);
void fsinfo(int buffer[3]);
int planner_add(const char *name, const char *time);

#define vec_size 64
typedef int (*sys_func) (uint32_t, uint32_t, uint32_t); // Объявление типа sys_func, который определяет вызываемую функцию и ее параметры.
static sys_func vector[vec_size];                       // Массив с именами вызываемых функций по их ID.

struct fd_elem
{
    int fd;
    struct file *file;
    struct list_elem elem;
    struct list_elem elem_thread;
};

static struct list file_list;
static struct lock file_lock;

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");

    memset(vector, 0, sizeof(sys_func) * vec_size);
    vector[SYS_HALT]		= (sys_func) halt;
    vector[SYS_EXIT]		= (sys_func) exit;
    vector[SYS_EXEC]		= (sys_func) exec;
    vector[SYS_WAIT]		= (sys_func) wait;
    vector[SYS_CREATE]		= (sys_func) create;
    vector[SYS_REMOVE]		= (sys_func) remove;
    vector[SYS_OPEN]		= (sys_func) open;
    vector[SYS_FILESIZE]	= (sys_func) filesize;
    vector[SYS_READ]		= (sys_func) read;
    vector[SYS_WRITE]		= (sys_func) write;
    vector[SYS_SEEK]		= (sys_func) seek;
    vector[SYS_TELL]		= (sys_func) tell;
    vector[SYS_CLOSE]		= (sys_func) close;
    vector[SYS_MMAP]		= (sys_func) mmap;
    vector[SYS_MUNMAP]		= (sys_func) munmap;
    vector[SYS_CHDIR]		= (sys_func) chdir;
    vector[SYS_MKDIR]		= (sys_func) mkdir;
    vector[SYS_READDIR]		= (sys_func) readdir;
    vector[SYS_ISDIR]		= (sys_func) isdir;
    vector[SYS_INUMBER]		= (sys_func) inumber;

    /* Eldar's syscalls. */
    vector[SYS_MEMSTAT]		= (sys_func) memstat;
    vector[SYS_FSINFO]		= (sys_func) fsinfo;

	/* Arina's syscalls. */
	vector[SYS_PLANNER]		= (sys_func) planner_add;

    list_init(&file_list);
    lock_init(&file_lock);
}

/* Обработчик системных вызовов. */
static void syscall_handler(struct intr_frame *f)
{
    sys_func func;      // Вызываемая функция.
    int sys_func_id;    // ID вызываемой функции.

    int *p_esp = (int *) f->esp;

    /* Проверка на принадлежность адресов сегменту пользователя. */
    if (!is_user_vaddr(p_esp) || !is_user_vaddr(p_esp + 1) || !is_user_vaddr(p_esp + 2) || !is_user_vaddr(p_esp + 3))
        goto terminate;

    sys_func_id = *p_esp;
    if (sys_func_id < 0 || sys_func_id >= vec_size)
    {
        printf("Incorrect system call ID [%d]!\n", sys_func_id);
        goto terminate;
    }

    /* Определяем вызываемую функцию по её ID. */
    func = vector[sys_func_id];
    if (func == NULL)
    {
        printf("Unhandled system call! ID: %d\n", sys_func_id);
        goto terminate;
    }

    /* Вызываем функцию и заносим возвращаемое значение в регистр. */
    f->eax = func(*(p_esp + 1), *(p_esp + 2), *(p_esp + 3));
    return;

terminate:
    exit(-1);
}

/* Завершение работы системы. */
void halt(void)
{
    shutdown_power_off();
    NOT_REACHED();
}

/* Завершение работы процесса. */
void exit(int code)
{
    /* Закрываем все файлы. */
    struct thread *thread_cur = thread_current();
    while (!list_empty(&thread_cur->open_files))
        close(list_entry(list_begin(&thread_cur->open_files), struct fd_elem, elem_thread)->fd);

    thread_exit_(code);
    NOT_REACHED();
}

/* Запускает исполняемый файл, имя которого задано в
   cmd_line (с учетом всех аргументов) и возвращает
   уникальный идентификатор нового пользовательского
   процесса, который создается в результате запуска
   исполняемого файла. */
pid_t exec(const char *file)
{
    int pid;

    if (file == NULL || !is_user_vaddr(file))
        return -1;

    lock_acquire(&file_lock);
    pid = process_execute(file);
    lock_release(&file_lock);

    return pid;
}

/* Ожидает завершения процесса с заданным PID. */
int wait(pid_t pid)
{
    return process_wait(pid);
}

/* Создает новый файл с именем file и размером initial_size.
   Возвращает “true”, если файл успешно создан, и “false” в
   противном случае. */
bool create(const char *file, unsigned initial_size)
{
    if (file == NULL) exit(-1);

    return filesys_create(file, initial_size);
}

/* Удаляет файл с указанным именем с диска. Если файл не
   существует — возвращает false. */
bool remove(const char *file)
{
    if (file == NULL) return false;
    if (!is_user_vaddr(file)) exit(-1);

    return filesys_remove(file);
}

/* Открывает файл с именем file. Возвращает
   целочисленное значение дескриптора файла fd или -1,
   если файл не может быть открыт. */
int open(const char *file_name)
{
    static int fid_count = 2;

    struct file *file;
    struct fd_elem *fde;
    int ret_value = -1;

    if (file_name == NULL) return -1;
    if (!is_user_vaddr(file_name)) exit(-1);

    file = filesys_open(file_name);
    if (file == NULL) goto end;

    fde = (struct fd_elem *) malloc(sizeof(struct fd_elem));
    if (fde == NULL)
    {
        file_close(file);
        goto end;
    }

    fde->file = file;
    fde->fd = fid_count++;
    list_push_back(&file_list, &fde->elem);
    list_push_back(&thread_current()->open_files, &fde->elem_thread);
    ret_value = fde->fd;

  end:
    return ret_value;
}

/* Возвращает размер открытого файла в байтах. */
int filesize(int fd)
{
    struct file *file;

    file = find_file_by_fd(fd);
    if (file == NULL) return -1;

    return file_length(file);
}

/* Читает size байт из файла с дескриптором fd в буфер.
   Возвращает число считанных байт или -1, если файл не
   может быть прочитан.

   fd=0 выполняет чтение с клавиатуры с помощью
   функции input_getc(), определенной в devices/input.h. */
int read(int fd, void *buffer, unsigned size)
{
    struct file *file;
    int ret_value = -1;

    lock_acquire(&file_lock);
    if (fd == STDIN_FILENO) // Чтение с клавиатуры.
    {
        for (unsigned i = 0; i != size; ++i)
            *(uint8_t *) (buffer + i) = input_getc();
        ret_value = size;
        goto end;
    } else if (fd == STDOUT_FILENO) // Вывод в консоль. Данная функция не производит этого действия.
    {
        goto end;
    } else if (!is_user_vaddr(buffer) || !is_user_vaddr(buffer + size)) // Ошибочные указатели.
    {
        lock_release(&file_lock);
        exit(-1);
    } else
    {
        file = find_file_by_fd(fd);
        if (!file) goto end;
        ret_value = file_read(file, buffer, size);
    }

    end: lock_release(&file_lock);
    return ret_value;
}

/* Записывает size байт в файл с дескриптором fd.
   Возвращает число записанных байт или -1, если запись в
   файл невозможна. */
int write(int fd, const void *buffer, unsigned size)
{
    struct file *file;
    int ret_value = -1;

    lock_acquire(&file_lock);
    if (fd == STDOUT_FILENO) // Вывод в консоль.
    {
        putbuf(buffer, size);
    } else if (fd == STDIN_FILENO) // Чтение с клавиатуры. Данная функция не производит этого действия.
    {
        goto end;
    } else if (!is_user_vaddr(buffer) || !is_user_vaddr(buffer + size)) // Ошибочные указатели.
    {
        lock_release(&file_lock);
        exit(-1);
    } else
    {
        file = find_file_by_fd(fd);
        if (!file) goto end;
        ret_value = file_write(file, buffer, size);
    }

    end: lock_release(&file_lock);
    return ret_value;
}

void seek(int fd UNUSED, unsigned position UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

unsigned tell(int fd UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

/* Закрывает файл с дескриптором fd. Если программа
   пытается закрыть выходной поток (fd=1) то, ядро должно
   завершить ее с ошибкой (код выхода -1). */
void close(int fd)
{
    struct fd_elem *fde = NULL;

    fde = find_fd_elem_by_fd_in_process(fd);
    if (fde == NULL) return;

    file_close(fde->file);
    list_remove(&fde->elem);
    list_remove(&fde->elem_thread);
    if (fde != NULL) free(fde);
}

mapid_t mmap(int fd UNUSED, void *addr UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

void munmap(mapid_t mapid UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

bool chdir(const char *dir UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

bool mkdir(const char *dir UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

bool readdir(int fd UNUSED, char name[READDIR_MAX_LEN + 1] UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

bool isdir(int fd UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

int inumber(int fd UNUSED)
{
    printf("Not implemented yet :(\n");
    exit(-1);
}

/* Локальные функции. */

static struct file * find_file_by_fd(int fd)
{
    struct fd_elem *ret_value = find_fd_elem_by_fd(fd);
    return (ret_value != NULL) ? ret_value->file : NULL;
}

static struct fd_elem * find_fd_elem_by_fd(int fd)
{
    struct fd_elem *ret_value;

    for (struct list_elem *list_el = list_begin(&file_list); list_el != list_end(&file_list); list_el = list_next(list_el))
    {
        ret_value = list_entry(list_el, struct fd_elem, elem);
        if (fd == ret_value->fd) return ret_value;
    }

    return NULL;
}

static struct fd_elem * find_fd_elem_by_fd_in_process(int fd)
{
    struct fd_elem *tmp = NULL;
    struct thread *t = thread_current();

    for (struct list_elem *list_el = list_begin(&t->open_files); list_el != list_end(&t->open_files); list_el = list_next(list_el))
    {
        tmp = list_entry(list_el, struct fd_elem, elem_thread);
        if (tmp->fd == fd) return tmp;
    }

    return NULL;
}

/* Eldar's syscalls.
   Дополнительное индивидуальное задание №8. */

/* Заполняет массив BUFFER следующими данными (в байтах):
    * buffer[0] - общий объем RAM пользовательских программ;
    * buffer[1] - свободная RAM пользовательских программ;
    * buffer[2] - занятая RAM пользовательских программ.
   Если происходит ошибка, данные заполняются -1. */
void memstat(int buffer[3])
{
    if (buffer == NULL || (buffer + 1) == NULL || (buffer + 2) == NULL) return;

    void *page = palloc_get_page(PAL_USER);                 // Пытаемся получить страницу, чтобы определить начало свободной памяти.
    if (page != NULL) palloc_free_page(page);

    size_t user_page_limit = get_user_page_limit();         // Ограничение на макс. число страниц пользователя.

    uint8_t *mem_start = ptov(1024 * 1024);                 // Начало виртуальной памяти (смещение 1 Мб от начала).
    uint8_t *free_end = ptov(init_ram_pages * PGSIZE);      // Конец ВП.
    size_t free_pages = (free_end - mem_start) / PGSIZE;    // Максимальное число страниц в памяти (ядро + пользователь).
    size_t user_pages = free_pages / 2;                     // Максимальное число пользовательских страниц.
    size_t kernel_pages;                                    // Максимальное число страниц ядра в памяти.
    if (user_pages > user_page_limit)
      user_pages = user_page_limit;
    kernel_pages = free_pages - user_pages;
    uint8_t *user_mem_start  = mem_start + kernel_pages * PGSIZE;   // Начало памяти пользователя.
    uint8_t *user_free_start = (uint8_t *) page;                    // Начало свободной памяти пользователя.

    buffer[0] = free_end - user_mem_start;
    buffer[1] = (user_free_start != NULL) ? free_end - user_free_start : 0;
    buffer[2] = (user_free_start != NULL) ? user_free_start - user_mem_start : buffer[0];
}

/* Заполняет массив BUFFER следующими данными:
    * buffer[0] - количество файлов на диске;
    * buffer[1] - общий размер диска (в байтах);
    * buffer[2] - размер занятого пространства (в байтах);
    * buffer[3] - размер свободного пространства (в байтах).
   Если происходит ошибка, данные заполняются -1. */
void fsinfo(int buffer[5])
{
    if (buffer == NULL || (buffer + 1) == NULL || (buffer + 2) == NULL || (buffer + 3) == NULL) return;

    int files_count = 0;
    off_t size = 0;

    char name[NAME_MAX + 1];                // Имя текущего файла, считанное с его дескриптора.
    struct inode *inode = NULL;             // Дескриптор файла.
    struct dir *dir = dir_open_root();      // Открываем директорию для чтения с диска.
    if (dir == NULL)
    {
        buffer[0] = buffer[1] = buffer[2] = buffer[3] = -1;
        return;
    }

    while (dir_readdir(dir, name))          // Перебираем дескрипторы файлов поочередно, пока файлы на диске не закончатся.
    {
        files_count++;                      // Увеличиваем счетчик найденных файлов.
        dir_lookup(dir, name, &inode);      // Ищем следующий дескриптор.
        if (inode != NULL)
            size += inode_length(inode);    // Увеличиваем размер занятого пространства.
    }

    dir_close(dir);

    buffer[0] = files_count;
    buffer[1] = block_size(fs_device) * BLOCK_SECTOR_SIZE;
    buffer[2] = size;
    buffer[3] = buffer[1] - buffer[2];
}

/* Coded by Arina. */

/* Добавляет задачу в планировщик.
    * NAME - путь (имя) использняемой программы;
	* TIME - время запуска в формате: ГГГГ:ММ:ДД:чч:мм:сс.
   Возвращает:
    * 0 в случае успеха;
	* -1 в случае неудачи по различным причинам;
	* -2 в случае неудачи по причине неверного формата. */
int planner_add(const char *name, const char *time)
{
	if (name == NULL || !is_user_vaddr(name) || time == NULL || !is_user_vaddr(time))
	    goto unknown;
	
	if (strlen(time) != 19 || strlen(name) < 3 || strlen(name) > 127) goto wrong_format;
	for (int i = 0; i < strlen(time); i++)
	{
		switch (i)
		{
			case 4:
			case 7:
			case 10:
			case 13:
			case 16:
				if (time[i] != ':') goto wrong_format;
				continue;
		}

		if (!('0' <= time[i] && time[i] <= '9')) goto wrong_format;
	}

	if (planner_add_task(name, time) == 0)
	{
		printf("Task '%s %s' added successfully!\n", name, time);
		return 0;
	} else
	{
		printf("Unexpected error while adding task.\n");
		goto unknown;
	}

	unknown:
	return -1;
	wrong_format:
	return -2;
}
