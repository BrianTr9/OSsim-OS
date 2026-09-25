#ifndef SCHED_H
#define SCHED_H

#include "common.h"

#ifndef MLQ_SCHED
#define MLQ_SCHED
#endif

#define MAX_PRIO 140

int queue_empty(void);

void init_scheduler(void);
void finish_scheduler(void);

/* Get the next process from ready queue */
struct pcb_t * get_proc(void);

/* Put a process back to run queue */
void put_proc(struct pcb_t * proc);

/* Add a new process to ready queue */
void add_proc(struct pcb_t * proc);

/* Remove a finished/killed process from the running list */
void finish_proc(struct pcb_t * proc);

/* Kill every process whose program name is [name], except [caller].
 * Processes waiting in a ready queue are dequeued and returned in [victims]
 * (the caller must free them); processes currently on a CPU are only marked
 * killed and cleaned up by that CPU. Return number of processes killed. */
int kill_proc_by_name(const char * name, struct pcb_t * caller,
		struct pcb_t ** victims, int * nvictims);

#endif


