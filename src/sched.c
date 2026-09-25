
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
static int slot_used[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return 0;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;

	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
		slot_used[i] = 0;
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	running_list.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

/* Program name of a process, i.e. path without the "input/proc/" prefix */
static const char * proc_name(struct pcb_t * proc) {
	const char * name = strrchr(proc->path, '/');
	return name ? name + 1 : proc->path;
}

void finish_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	remove_from_queue(&running_list, proc);
	pthread_mutex_unlock(&queue_lock);
}

int kill_proc_by_name(const char * name, struct pcb_t * caller,
		struct pcb_t ** victims, int * nvictims) {
	int killed = 0;
	int i;

	*nvictims = 0;
	pthread_mutex_lock(&queue_lock);
	i = 0;
	while (i < running_list.size) {
		struct pcb_t * proc = running_list.proc[i];
		if (proc == caller || strcmp(proc_name(proc), name) != 0) {
			i++;
			continue;
		}
		killed++;
		__atomic_store_n(&proc->killed, 1, __ATOMIC_SEQ_CST);
		printf("Found process to terminate: %s (pid=%d)\n",
			proc->path, proc->pid);

		/* A process sitting in a ready queue is not owned by any CPU,
		 * take it out now; a running one is reaped by its CPU */
		int in_ready = remove_from_queue(&ready_queue, proc) ||
			remove_from_queue(&run_queue, proc);
#ifdef MLQ_SCHED
		if (!in_ready && proc->prio < MAX_PRIO)
			in_ready = remove_from_queue(&mlq_ready_queue[proc->prio], proc);
#endif
		if (in_ready) {
			remove_from_queue(&running_list, proc);
			victims[(*nvictims)++] = proc;
		} else {
			i++;
		}
	}
	pthread_mutex_unlock(&queue_lock);
	return killed;
}

#ifdef MLQ_SCHED
/* 
 *  MLQ policy: traverse the queues from the highest priority (0) down,
 *  each queue may dispatch at most slot[prio] = MAX_PRIO - prio times.
 *  When every non-empty queue has used up its slots, a new round starts
 *  and all slot counters are reset, so lower priorities never starve.
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;
	int i, round;

	pthread_mutex_lock(&queue_lock);
	for (round = 0; round < 2 && proc == NULL; round++) {
		for (i = 0; i < MAX_PRIO; i++) {
			if (!empty(&mlq_ready_queue[i]) && slot_used[i] < slot[i]) {
				proc = dequeue(&mlq_ready_queue[i]);
				slot_used[i]++;
				break;
			}
		}
		if (proc == NULL) {
			/* All ready processes exhausted their slots: new round */
			for (i = 0; i < MAX_PRIO; i++)
				slot_used[i] = 0;
		}
	}
	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = & running_list;
	if (proc->prio >= MAX_PRIO)
		proc->prio = MAX_PRIO - 1;

	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	pthread_mutex_lock(&queue_lock);
	
	// First, try to get a process from the ready queue
	if (!empty(&ready_queue)) {
		proc = dequeue(&ready_queue);
	} 
	// If ready queue is empty, try to get from run queue
	else if (!empty(&run_queue)) {
		proc = dequeue(&run_queue);
	}
	
	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->running_list = & running_list;

	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif
