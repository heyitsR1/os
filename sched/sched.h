#pragma once
#include "../include/types.h"

// Task states for the round-robin scheduler.
enum { TASK_READY, TASK_RUNNING, TASK_DEAD };

// Task control block. One per kernel thread.
//   esp is the saved stack pointer; every other register lives on that stack.
typedef struct task {
    uint32_t      esp;         // saved stack pointer (top of saved context)
    uint32_t     *stack;       // base of the allocated stack (0 for task 0)
    uint32_t      stack_size;
    void        (*entry)(void);
    int           state;       // TASK_READY / TASK_RUNNING / TASK_DEAD
    struct task  *next;        // circular scheduler list
} task_t;

void    sched_init(void);                  // wrap the running context as task 0
task_t *task_create(void (*entry)(void));  // spawn a new kernel thread
void    schedule(void);                    // switch to the next ready task
void    sched_yield(void);                 // voluntarily give up the CPU
task_t *sched_current(void);               // currently running task
