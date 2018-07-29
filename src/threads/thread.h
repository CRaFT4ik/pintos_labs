#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h" // coded by Eldar

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING,       /* About to be destroyed. */

    /* coded by Eldar */
    THREAD_SLEEPING     /* Поток в состоянии сна. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
{
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* coded by Eldar : begin */
    int64_t wake_up_ticks;              /* Значение тиков, когда поток должен проснуться. Устанавливается при усыплении. */
    unsigned CPU_burst;                 /* Значение CPU burst. */
    struct list debts;                  /* Список долгов процесса по приоритетам. */
    int planned_priority;               /* Запланированный приоритет. Будет установлен сразу, когда список долгов будет пуст. */

#ifdef USERPROG
    struct semaphore life;              /* Пользовательская программа не завершится, пока семафор установлен на 0. */
    tid_t parent_tid;                   /* ID порождающего процесса. */
    int wait_info[2];                   /* var[0] - установлено ли ожидание завершения для данного процесса (1 - да); var[1] - код завершения (по умолчанию 0). */
#endif

	struct list open_files;             /* Список открытых файлов. */
    /* coded by Eldar : end */

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);
tid_t thread_create_(const char *name, int priority, unsigned CPU_burst, thread_func *function, void *aux); // coded by Eldar

void thread_block (void);
void thread_unblock (struct thread *);

/* coded by Eldar */

#define DEBT_ELEM_MAGIC 0x52e9b759

struct debt_elem
{
    struct list_elem elem;

    struct lock *lock;
    struct thread *thread_first_lock_holder;
    int debt;
    struct thread *thread_lender;

    unsigned magic;
};

void thread_sleep(int64_t);
void thread_wake(struct thread *);
void thread_pause(tid_t);
void thread_resume(tid_t);
void sort_thread_list(struct list *);
void thread_priority_yield(struct lock *);
void thread_try_return_priorities(struct lock *);
struct thread * get_thread_by_tid(tid_t tid);
/* coded by Eldar : end */

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit_ (int) NO_RETURN; // coded by Eldar
void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

#endif /* threads/thread.h */
