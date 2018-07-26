
/* File for 'narrow_bridge' task implementation.
   SPbSTU, IBKS, 2017.

   Implemented by Eldar Timraleev. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "narrow-bridge.h"

const int N = 2;                        // Кол-во машин разрешенных для одновременного пересечения моста.
const int64_t eps = 1;                  // Погрешность времени, в течении которого машина успевает въехать на мост после начала смены сигнала светофора.

/* Структура обеспечивает синхронизированный доступ к переменной VAR. */
struct sync_var
{
    struct lock lock;
    int var;
};

/* Структура организовывает все необходимое для создания потока машин в какую-либо сторону и управления им. */
struct stream
{
    struct sync_var on_bridge_count;    // Количество машин данного потока, находящихся на мосту.
    struct sync_var waiters_count;      // Количество новоприбывших машин в поток с данным направлением, ожидающих очереди на переправу.

    struct semaphore queue;             // Очередь движения в данную сторону.
};

static void way_schedule(void);
static void refreshVarsValues(void);

static void stream_init(struct stream *);
static void stream_enter(struct stream *);
static void stream_leave(struct stream *);

static void sync_var_init(struct sync_var *, int);
static int sync_increment(struct sync_var *);
static int sync_decrement(struct sync_var *);
static int sync_read(struct sync_var *);

/* -------------------------------------------------------------------------- */
/* ---------------------------------  VARS  --------------------------------- */
/* -------------------------------------------------------------------------- */

struct stream stream_left_normal;       // Поток обычных машин в левую сторону.
struct stream stream_left_emergency;    // Поток скорых машин в левую сторону.
struct stream stream_right_normal;      // Поток обычных машин в правую сторону.
struct stream stream_right_emergency;   // Поток скорых машин в правую сторону.

struct lock h1, h2, h3;                 // Вспомогательные замки.

/* Названия следующих переменных говорят за себя. Необходимы для управления светофором. */

static int on_bridge_left_normal_count, on_bridge_left_emergency_count;
static int on_bridge_right_normal_count, on_bridge_right_emergency_count;
static int on_bridge_left_count, on_bridge_right_count, on_bridge_count;

static int stream_left_normal_waiters, stream_left_emergency_waiters;
static int stream_right_normal_waiters, stream_right_emergency_waiters;
static int stream_left_waiters, stream_right_waiters;

/* -------------------------------------------------------------------------- */
/* ---------------------------------  CODE  --------------------------------- */
/* -------------------------------------------------------------------------- */

void narrow_bridge_init(void)
{
    stream_init(&stream_left_normal);
    stream_init(&stream_left_emergency);
    stream_init(&stream_right_normal);
    stream_init(&stream_right_emergency);

    lock_init(&h1);
    lock_init(&h2);
    lock_init(&h3);
}

void arrive_bridge(enum car_priority prio, enum car_direction dir)
{
    if (dir == dir_left)
    {
        if (prio == car_normal)
            stream_enter(&stream_left_normal);
        else if (prio == car_emergency)
            stream_enter(&stream_left_emergency);
    } else if (dir == dir_right)
    {
        if (prio == car_normal)
            stream_enter(&stream_right_normal);
        else if (prio == car_emergency)
            stream_enter(&stream_right_emergency);
    }
}

void exit_bridge(enum car_priority prio, enum car_direction dir)
{
    if (dir == dir_left)
    {
        if (prio == car_normal)
            stream_leave(&stream_left_normal);
        else if (prio == car_emergency)
            stream_leave(&stream_left_emergency);
    } else if (dir == dir_right)
    {
        if (prio == car_normal)
            stream_leave(&stream_right_normal);
        else if (prio == car_emergency)
            stream_leave(&stream_right_emergency);
    }
}

/* Регулировщик движения по мосту. Определяет поток для выполнения с учетом особенностей моста. */
static void way_schedule(void)
{
    static int64_t signal_change_ticks;

    lock_acquire(&h1);
    refreshVarsValues();

    if (on_bridge_count >= N) goto end; // Проверяем, что на мосту сейчас находится не больше N машин.

    /* Организуем светофор на мосту. Скорая помощь имеет приоритет над обычными машинами,
       но для двунаправленного потока из скорых также действует светофор. */

    static enum car_direction direction = -1; // Светофор. Определяет направление движения машин.

    /* Первоначально инициализируем переменную STREAM_DIRECTION.
       Важно изначально правильно определить направление (для теста в параметрах с одной машиной). */
    if (direction == -1)
    if (stream_right_emergency_waiters + stream_left_emergency_waiters == 0)
    {
        if (stream_right_normal_waiters) direction = dir_right;
        else direction = dir_left;
    } else
    {
        if (stream_right_emergency_waiters) direction = dir_right;
        else direction = dir_left;
    }

    /* Условимся не переключать сигнал светофора, если на другой стороне нет машин скорой помощи,
       а на данной стороне они еще есть. */
    if (stream_left_emergency_waiters > 0 && stream_right_emergency_waiters == 0) direction = dir_left;
    else if (stream_right_emergency_waiters > 0 && stream_left_emergency_waiters == 0) direction = dir_right;
    /* Также условимся не переключать сигнал светофора, если данная машина (данный поток, который вошел в этот метод)
       еще успевает по времени с учетом погрешности EPS проехать в ту же сторону за предыдущей машиной. */
    else if (on_bridge_left_count > 0 && stream_left_waiters > 0 && timer_elapsed(signal_change_ticks) <= eps) direction = dir_left;
    else if (on_bridge_right_count > 0 && stream_right_waiters > 0 && timer_elapsed(signal_change_ticks) <= eps) direction = dir_right;
    /* Еще одно условие: не переключать сигнал светофора, если до сих пор существуют потоки только в одно направление. */
    else if (stream_left_waiters == 0) direction = dir_right;
    else if (stream_right_waiters == 0) direction = dir_left;

// printf("exe %s dir = %s\n", thread_name(), direction == dir_left ? "left" : "right"); // DEBUG-LINE

    if (direction == dir_left && on_bridge_right_count == 0)
    {
        while (stream_left_emergency_waiters && on_bridge_count < N)
        {
            sync_increment(&stream_left_emergency.on_bridge_count);     // Повышаем счетчик машин на мосту для данного потока.
            sync_decrement(&stream_left_emergency.waiters_count);       // Понижаем счетчик машин ожидающих переправы для данного потока.
            refreshVarsValues();                                        // Обновляем значения переменных (в частности счетчик общего числа машин на мосту).

            sema_up(&stream_left_emergency.queue);
        }

        while (stream_left_normal_waiters && on_bridge_count < N)
        {
            sync_increment(&stream_left_normal.on_bridge_count);
            sync_decrement(&stream_left_normal.waiters_count);
            refreshVarsValues();

            sema_up(&stream_left_normal.queue);
        }

        /* Переключаем сигнал светофора (при следующем вызове метода есть вероятность, что он снова станет прежним). */
        signal_change_ticks = timer_ticks();
        direction = dir_right;
    } else if (direction == dir_right && on_bridge_left_count == 0)
    {
        while (stream_right_emergency_waiters && on_bridge_count < N)
        {
            sync_increment(&stream_right_emergency.on_bridge_count);
            sync_decrement(&stream_right_emergency.waiters_count);
            refreshVarsValues();

            sema_up(&stream_right_emergency.queue);
        }

        while (stream_right_normal_waiters && on_bridge_count < N)
        {
            sync_increment(&stream_right_normal.on_bridge_count);
            sync_decrement(&stream_right_normal.waiters_count);
            refreshVarsValues();

            sema_up(&stream_right_normal.queue);
        }

        /* Переключаем сигнал светофора (при следующем вызове метода есть вероятность, что он снова станет прежним). */
        signal_change_ticks = timer_ticks();
        direction = dir_left;
    }

end:
    lock_release(&h1);
}

/* Вспомогательный метод. Пересчитывает значения переменных. */
static void refreshVarsValues(void)
{
    on_bridge_left_normal_count = sync_read(&stream_left_normal.on_bridge_count);
    on_bridge_left_emergency_count = sync_read(&stream_left_emergency.on_bridge_count);
    on_bridge_right_normal_count = sync_read(&stream_right_normal.on_bridge_count);
    on_bridge_right_emergency_count = sync_read(&stream_right_emergency.on_bridge_count);

    on_bridge_left_count = on_bridge_left_normal_count + on_bridge_left_emergency_count;
    on_bridge_right_count = on_bridge_right_normal_count + on_bridge_right_emergency_count;
    on_bridge_count = on_bridge_left_count + on_bridge_right_count;

    stream_left_normal_waiters = sync_read(&stream_left_normal.waiters_count);
    stream_left_emergency_waiters = sync_read(&stream_left_emergency.waiters_count);
    stream_right_normal_waiters = sync_read(&stream_right_normal.waiters_count);
    stream_right_emergency_waiters = sync_read(&stream_right_emergency.waiters_count);

    stream_left_waiters = stream_left_normal_waiters + stream_left_emergency_waiters;
    stream_right_waiters = stream_right_normal_waiters + stream_right_emergency_waiters;
}

/* ----------------------------- */
/* --- ===== separator ===== --- */
/* ----------------------------- */

/* Инициализирует поток-очередь. */
static void stream_init(struct stream *stream)
{
    sync_var_init(&stream->on_bridge_count, 0);
    sync_var_init(&stream->waiters_count, 0);
    sema_init(&stream->queue, 0);
}

/* Добавляет новый элемент в очередь потока STREAM и вызывает регулировщика. */
static void stream_enter(struct stream *stream)
{
    lock_acquire(&h2);
    sync_increment(&stream->waiters_count);
    way_schedule();
    lock_release(&h2);

    sema_down(&stream->queue);
}

/* Покидает очередь данного потока STREAM и вызывает регулировщика. */
static void stream_leave(struct stream *stream)
{
    lock_acquire(&h3);
    sync_decrement(&stream->on_bridge_count);
    way_schedule();
    lock_release(&h3);
}

/* ----------------------------- */
/* --- ===== separator ===== --- */
/* ----------------------------- */

/* Инициализация структуры синхронизированного доступа. */
static void sync_var_init(struct sync_var *sv, int var_value)
{
    lock_init(&sv->lock);
    sv->var = var_value;
}

/* Синхронизированное увеличение значения структуры. */
static int sync_increment(struct sync_var *sv)
{
    int value;
    lock_acquire(&sv->lock);
    value = ++sv->var;
    lock_release(&sv->lock);
    return value;
}

/* Синхронизированное уменьшение значения структуры. */
static int sync_decrement(struct sync_var *sv)
{
    int value;
    lock_acquire(&sv->lock);
    value = --sv->var;
    lock_release(&sv->lock);
    return value;
}

/* Синхронизированное чтение значения структуры. */
static int sync_read(struct sync_var *sv)
{
    int value;
    lock_acquire(&sv->lock);
    value = sv->var;
    lock_release(&sv->lock);
    return value;
}
