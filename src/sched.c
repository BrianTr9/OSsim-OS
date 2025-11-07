#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;

	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;
	
	// MLQ policy implementation
	static unsigned long current_prio = 0;  // Track current priority level
	static int current_slot_used[MAX_PRIO] = {0}; // Track slot usage for each priority
	
	pthread_mutex_lock(&queue_lock);
	
	// First, process all lower priority queues until they're empty
	// This means priority 0, 1, 2... (higher actual priority)
	int i;
	int found = 0;
	int found_any = 0;
	
	// Check if any queue has processes at all
	for (i = 0; i < MAX_PRIO; i++) {
		if (!empty(&mlq_ready_queue[i])) {
			found_any = 1;
			break;
		}
	}
	
	if (found_any) {
		// First, prioritize lower priority levels (0, 1, 2...)
		// In sched_1 case, this will prioritize processes 2, 3, 4 (priority 0)
		for (i = 0; i < MAX_PRIO && !found; i++) {
			// Skip empty queues or those already at their slot limit
			if (empty(&mlq_ready_queue[i])) continue;
			
			// For priority 0 (highest actual priority), handle slots properly
			if (i == current_prio && current_slot_used[i] < slot[i]) {
				found = 1;
				proc = dequeue(&mlq_ready_queue[i]);
				current_slot_used[i]++;
				
				// If all slots for this priority used up, move to next
				if (current_slot_used[i] >= slot[i]) {
					current_slot_used[i] = 0;
					current_prio = (current_prio + 1) % MAX_PRIO;
				}
			}
			// If we're not at the current priority or slots used up
			else if (i != current_prio && !empty(&mlq_ready_queue[i])) {
				// Found a non-empty queue at different priority level
				current_prio = i;
				found = 1;
				proc = dequeue(&mlq_ready_queue[i]);
				current_slot_used[i]++;
				
				if (current_slot_used[i] >= slot[i]) {
					current_slot_used[i] = 0;
					current_prio = (current_prio + 1) % MAX_PRIO;
				}
			}
		}
		
		// If still not found, try all priorities again
		if (!found) {
			for (i = 0; i < MAX_PRIO; i++) {
				if (!empty(&mlq_ready_queue[i])) {
					current_prio = i;
					proc = dequeue(&mlq_ready_queue[i]);
					current_slot_used[i] = 1;
					found = 1;
					break;
				}
			}
		}
	}
	
	// If no process found, reset all slot counters
	if (!found) {
		for (i = 0; i < MAX_PRIO; i++) {
			current_slot_used[i] = 0;
		}
		current_prio = 0;
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
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */
	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	pthread_mutex_unlock(&queue_lock);

	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */
	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	pthread_mutex_unlock(&queue_lock);

	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
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
	proc->ready_queue = &ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */
	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	pthread_mutex_unlock(&queue_lock);

	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */
	pthread_mutex_lock(&queue_lock);
	enqueue(&running_list, proc);
	pthread_mutex_unlock(&queue_lock);

	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif

