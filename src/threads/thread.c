#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif
/* coded by Eldar */
#include "devices/timer.h"
#include "threads/alarm.h"
#include "threads/malloc.h"
/* coded by Eldar : end */

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority, unsigned CPU_burst); // coded by Eldar
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT, TIME_SLICE);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= t->CPU_burst) // coded by Eldar
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create_(const char *name, int priority, unsigned CPU_burst, thread_func *function, void *aux) // coded by Eldar
{
    struct thread *t;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;
    enum intr_level old_level;

    ASSERT (function != NULL);

    /* Allocate thread. */
    t = palloc_get_page (PAL_ZERO);
    if (t == NULL)
    return TID_ERROR;

    /* Initialize thread. */
    init_thread (t, name, priority, CPU_burst);
    tid = t->tid = allocate_tid ();

    /* coded by Eldar : begin */
#ifdef USERPROG
    t->parent_tid = thread_current()->tid;
#endif
    /* coded by Eldar : end */

    /* Prepare thread for first run by initializing its stack.
    Do this atomically so intermediate values for the 'stack'
    member cannot be observed. */
    old_level = intr_disable ();

    /* Stack frame for kernel_thread(). */
    kf = alloc_frame (t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;

    /* Stack frame for switch_entry(). */
    ef = alloc_frame (t, sizeof *ef);
    ef->eip = (void (*) (void)) kernel_thread;

    /* Stack frame for switch_threads(). */
    sf = alloc_frame (t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;

    intr_set_level (old_level);

    /* Add to run queue. */
    thread_unblock (t);

    /* coded by Eldar */
    thread_yield();

    return tid;
}

/* The same as thread_create_(), but uses default CPU_burst value equal to TIME_SLICE.
   Coded by Eldar. */
tid_t thread_create(const char *name, int priority, thread_func *function, void *aux)
{
    return thread_create_(name, priority, TIME_SLICE, function, aux);
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* coded by Eldar : begin */

/* Переводит текущий поток в состояние сна на время TICKS.
   Прерывания должны быть включены. Поток должен быть в THREAD_RUNNING. */
void thread_sleep(int64_t ticks)
{
	enum intr_level old_level;

	if (ticks <= 0) return;
	ASSERT (!intr_context());
	ASSERT (intr_get_level() == INTR_ON);

	old_level = intr_disable();
	ASSERT (thread_current()->status == THREAD_RUNNING);

	alarm_set(ticks + timer_ticks());
	thread_current()->status = THREAD_SLEEPING;

    schedule();
    intr_set_level(old_level);
}

/* Переводит спящий поток T в состояние ready-to-run.
   Не станет будить поток, если он заблокирован. */
void thread_wake(struct thread *t)
{
	enum intr_level old_level;

	ASSERT (is_thread(t));
	if (t->status == THREAD_BLOCKED) return;

	old_level = intr_disable();
	ASSERT (t->status == THREAD_SLEEPING);
	alarm_remove(t);
	list_push_back(&ready_list, &t->elem);
	t->status = THREAD_READY;
	intr_set_level(old_level);
}

/* Безусловная приостановка потока (перевод в состояние BLOCKED). */
void thread_pause(tid_t tid)
{
	ASSERT (!intr_context());

	enum intr_level old_level = intr_disable();
	struct thread *t = get_thread_by_tid(tid);
	struct thread *t_running;

	ASSERT (is_thread(t));
	ASSERT (t->status != THREAD_BLOCKED);

	t->status = THREAD_BLOCKED;
	t_running = thread_current();
	if (t_running != t)
	{
		list_push_back(&ready_list, &t_running->elem);
		t_running->status = THREAD_READY;
	}
	schedule();
    intr_set_level(old_level);
}

/* Возобновление выполнения потока, ранее приостановленного
   с помощью thread_pause().
   То же самое, что и thread_unblock(), но с возможностью выбрать
   поток по tid и тихим выполнением проверок. */
void thread_resume(tid_t tid)
{
	struct thread *t = get_thread_by_tid(tid);
	if (is_thread(t) && t->status == THREAD_BLOCKED) thread_unblock(t);
}

/* Получить поток по его TID.
   Вернет NULL, если поток с данным TID не будет найден.
   Прерывания должны быть выключены. */
struct thread * get_thread_by_tid(tid_t tid)
{
	struct list_elem *e;
	struct thread *t = NULL;

	enum intr_level old_level = intr_disable();
	for (e = list_begin(&all_list); e != list_end(&all_list); e = list_next(e))
	{
		t = list_entry(e, struct thread, allelem);
		if (t->tid == tid) break; else t = NULL;
	}
	intr_set_level(old_level);

	return t;
}

/* coded by Eldar : end */

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit (void) { thread_exit_(0); }
void thread_exit_(int exit_code)
{
    ASSERT (!intr_context ());

#ifdef USERPROG
    /* coded by Eldar : begin */
    thread_current()->wait_info[1] = exit_code;
    process_exit();
    /* coded by Eldar : end */
#endif

    /* Remove thread from all threads list, set our status to dying,
    and schedule another process.  That process will destroy us
    when it calls thread_schedule_tail(). */
    intr_disable ();
    list_remove (&thread_current()->allelem);
    thread_current ()->status = THREAD_DYING;
    schedule ();
    NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    list_push_back (&ready_list, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority)
{
    /* coded by Eldar */
    if (list_empty(&thread_current()->debts))
    {
        thread_current()->priority = new_priority;
        thread_yield();
    } else
    {
        thread_current()->planned_priority = new_priority;
    }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  return thread_current()->priority;
}

/* coded by Eldar */

/* Пытается разделить приоритет с потоком, который в данный момент обладает замком LOCK.
   Возможно 2 ситуации: текущему выполняемому потоку C передавал приоритет свыше другой высокоприоритетный поток H, или же не передавал.
   * Если такая передача была, поток C пытается разделить свой долг по приоритету от потока H с потоком-обладателем замка LOCK - L.
   * Если подобной передачи не было (т.е. список долгов у C пуст), то C делится уже своим собственным приоритетом с потоком L. */
void thread_priority_yield(struct lock *lock)
{
    enum intr_level old_level = intr_disable();
    struct thread *thread_to = lock->holder;
    struct thread *thread_from = thread_current();

    ASSERT (lock != NULL);
    ASSERT (is_thread(thread_to)); // Возможно стоит просто тихо завершить функцию (к примеру, если поток сам принудительно завершил программу выполнения).
    ASSERT (thread_from->priority > thread_to->priority); // Если на этом застрянет, возможно стоит отключить прерывания до вызова этой функции.

    int difference = thread_from->priority - thread_to->priority;
    struct debt_elem *debt = NULL;

    if (!list_empty(&thread_from->debts)) debt = list_entry(list_front(&thread_from->debts), struct debt_elem, elem);

    if (debt == NULL) // Приоритет свыше никто не передавал.
    {
        struct debt_elem *debt_elem = malloc(sizeof(struct debt_elem));

        debt_elem->lock = lock;
        debt_elem->thread_first_lock_holder = thread_to;
        debt_elem->debt = difference;
        debt_elem->thread_lender = thread_from;
        debt_elem->magic = DEBT_ELEM_MAGIC;

        thread_to->priority += difference;
        thread_from->priority -= difference;

        list_push_front(&thread_to->debts, &debt_elem->elem);
//printf(" * %s [%d] donate priority to %s [%d]: %d\n", thread_from->name, thread_from->priority, thread_to->name, thread_to->priority, difference);
    } else if (difference >= debt->debt) // Был передан приоритет свыше. Отдаем его полностью обладателю замка.
    {
        list_remove(&debt->elem);
        list_push_front(&thread_to->debts, &debt->elem);

        thread_from->priority -= debt->debt;
        thread_to->priority += debt->debt;
//printf(" * Перевешиваем долг по приоритету ниже: от %s [%d] на %s [%d]. Вернуть: %s [%d].\n", thread_name(), thread_from->priority, thread_to->name, thread_to->priority, debt->thread_lender->name, debt->thread_lender->priority);
    } else // Был передан приоритет свыше. Отдаем его часть обладателю замка.
    {
        struct debt_elem *new_lock_debt = malloc(sizeof(struct debt_elem));

        new_lock_debt->lock = lock;
        new_lock_debt->thread_first_lock_holder = debt->thread_first_lock_holder;
        new_lock_debt->debt = difference;
        new_lock_debt->thread_lender = debt->thread_lender;
        new_lock_debt->magic = DEBT_ELEM_MAGIC;

        debt->debt -= difference;

        thread_from->priority -= difference;
        thread_to->priority += difference;

        list_push_front(&thread_to->debts, &new_lock_debt->elem);
//printf(" * Разделяем долг по приоритету с потоком ниже: от %s [%d] на %s [%d]. Вернуть: %s [%d].\n", thread_name(), thread_from->priority, thread_to->name, thread_to->priority, debt->thread_lender->name, debt->thread_lender->priority);
    }

    intr_set_level(old_level);
    thread_yield();
}

/* Процесс пытается вернуть долги по приоритетам заключенных по сделке с условием: возврат сразу после освобождения замка LOCK или смены владельца этого замка. */
void thread_try_return_priorities(struct lock *lock)
{
    enum intr_level old_level = intr_disable();
    struct thread *thread_cur = thread_current();
    bool need_to_yield = false;
    struct list_elem *e;

    if (list_empty(&thread_cur->debts)) goto end;

    for (e = list_begin(&thread_cur->debts); e != list_end(&thread_cur->debts); e = list_next(e))
    {
        struct debt_elem *debt = list_entry(e, struct debt_elem, elem);

        ASSERT (debt->magic == DEBT_ELEM_MAGIC);

        if (lock == debt->lock || (need_to_yield && debt->thread_first_lock_holder != debt->lock->holder))
        {
            debt->thread_lender->priority += debt->debt;
            thread_cur->priority -= debt->debt;
//printf(" ** %s [%d] return priority to %s [%d] %d\n", thread_cur->name, thread_cur->priority, debt->thread_lender->name, debt->thread_lender->priority, is_thread(debt->thread_lender));

            list_remove(e);
            e = list_prev(e);
            free(debt);

            need_to_yield = true;
        } else break;
    }

    if (thread_cur->planned_priority >= PRI_MIN && list_empty(&thread_cur->debts))
        thread_cur->priority = thread_cur->planned_priority;

    end:
    intr_set_level(old_level);
    if (need_to_yield) thread_yield();
}

/* coded by Eldar : end */

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED)
{
  /* Not yet implemented. */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  /* Not yet implemented. */
  return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  /* Not yet implemented. */
  return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  /* Not yet implemented. */
  return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority, unsigned CPU_burst) // coded by Eldar
{
    ASSERT (t != NULL);
    ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT (name != NULL);

    memset (t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy (t->name, name, sizeof t->name);
    t->stack = (uint8_t *) t + PGSIZE;
    t->priority = priority;
    t->magic = THREAD_MAGIC;

    /* coded by Eldar */
    t->CPU_burst = CPU_burst;
    t->planned_priority = PRI_MIN - 1;
    list_init(&t->debts);

#ifdef USERPROG
    sema_init(&t->life, 0);
    t->parent_tid = 0;
    t->wait_info[0] = 0;
    t->wait_info[1] = 0;

    list_init(&t->open_files);
#endif
    /* coded by Eldar : end */

    list_push_back (&all_list, &t->allelem);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
    /* coded by Eldar */
    if (list_empty(&ready_list))
    {
        return idle_thread;
    } else
    {
        sort_thread_list(&ready_list);
        return list_entry(list_pop_front(&ready_list), struct thread, elem);
    }
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
  {
//printf(" :: SCHEDULE [from %s (%d) to %s (%d)] :: ticks %d\n", cur->name, cur->priority, next->name, next->priority, (int) timer_ticks()); // NEED TO TURN ASSERT IN thread_current() OFF
      prev = switch_threads (cur, next);
  }
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* coded by Eldar */

/* Указывает, как правильно сортировать список потоков, готовых к выполнению.
   Сравнивает значения CPU burst у элементов списка A и B.
   Сортировка происходит так, что потоки расставляются по возрастанию значений CPU burst. */
static bool thread_list_less_CPU_burst(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
    struct thread *A, *B;

    ASSERT (a != NULL && b != NULL);

    A = list_entry(a, struct thread, elem);
    B = list_entry(b, struct thread, elem);

    return (A->CPU_burst < B->CPU_burst);
}

/* Указывает, как правильно сортировать список потоков, готовых к выполнению.
   Сравнивает значения приоритета у элементов списка A и B.
   Сортировка происходит так, что потоки расставляются по убыванию приоритетов. */
static bool thread_list_more_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
    struct thread *A, *B;

    ASSERT (a != NULL && b != NULL);

    A = list_entry(a, struct thread, elem);
    B = list_entry(b, struct thread, elem);

    return (A->priority > B->priority);
}

/* Сортирует массив потоков готовых к выполнению по убыванию значений CPU burst
   и по возрастанию приоритетов.
   Планировщик выбирает поток на выполнение из начала списка, поэтому сортировка позволит
   выполнять в первую очередь потоки с большим приоритетом и меньшим CPU burst. */
void sort_thread_list(struct list *_list)
{
    if (list_empty(_list)) return;

    enum intr_level old_level = intr_disable();

    list_sort(_list, thread_list_less_CPU_burst, NULL);
    list_sort(_list, thread_list_more_priority, NULL);

    intr_set_level(old_level);
}

/* coded by Eldar : end */

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
