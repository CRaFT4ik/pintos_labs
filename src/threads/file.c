#include "threads/file.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/string.h"
#include <lib/user/syscall.h>
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/file.h"
#include <filesys/filesys.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/loader.h"
#include "devices/block.h"
#include <filesys/inode.h>
#include <filesys/directory.h>

struct fd_elem
{
    int fd;
    struct file *file;
    struct list_elem elem;
    struct list_elem elem_thread;
};

static struct list file_list;

static struct file *find_file_by_fd(int fd);
static struct fd_elem *find_fd_elem_by_fd(int fd);
static struct fd_elem *find_fd_elem_by_fd_in_process(int fd);

/* Создает новый файл с именем file и размером initial_size.
   Возвращает “true”, если файл успешно создан, и “false” в
   противном случае. */
bool create(const char *file, unsigned initial_size)
{
    ASSERT (file != NULL);
    return filesys_create(file, initial_size);
}

/* Удаляет файл с указанным именем с диска. Если файл не
   существует — возвращает false. */
bool remove(const char *file)
{
    if (file == NULL) return false;

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

    enum intr_level old_level = intr_disable();

    if (fd == STDIN_FILENO) // Чтение с клавиатуры.
    {
        for (unsigned i = 0; i != size; ++i)
            *(uint8_t *) (buffer + i) = input_getc();
        ret_value = size;
        goto end;
    } else if (fd == STDOUT_FILENO) // Вывод в консоль. Данная функция не производит этого действия.
    {
        goto end;
    } else
    {
        file = find_file_by_fd(fd);
        if (!file) goto end;
        ret_value = file_read(file, buffer, size);
    }

    end: intr_set_level(old_level);
    return ret_value;
}

/* Записывает size байт в файл с дескриптором fd.
   Возвращает число записанных байт или -1, если запись в
   файл невозможна. */
int write(int fd, const void *buffer, unsigned size)
{
    struct file *file;
    int ret_value = -1;

    enum intr_level old_level = intr_disable();
    if (fd == STDOUT_FILENO) // Вывод в консоль.
    {
        putbuf(buffer, size);
    } else if (fd == STDIN_FILENO) // Чтение с клавиатуры. Данная функция не производит этого действия.
    {
        goto end;
    } else
    {
        file = find_file_by_fd(fd);
        if (!file) goto end;
        ret_value = file_write(file, buffer, size);
    }

    end: intr_set_level(old_level);
    return ret_value;
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
