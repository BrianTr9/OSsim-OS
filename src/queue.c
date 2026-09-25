#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        if (q == NULL || proc == NULL) return;
        
        // Check if queue is full
        if (q->size >= MAX_QUEUE_SIZE) {
                printf("enqueue: queue is full, process %d dropped\n", proc->pid);
                return;
        }
        
        // Add process to the end of the queue
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        if (q == NULL || q->size == 0) {
                return NULL; // Queue is empty
        }
        
        // In this simple implementation, we'll just remove and return
        // the first process in the queue (FIFO behavior)
        struct pcb_t *proc = q->proc[0];
        
        // Shift all remaining processes
        int i;
        for (i = 0; i < q->size - 1; i++) {
                q->proc[i] = q->proc[i + 1];
        }
        
        q->size--;
        return proc;
}

int remove_from_queue(struct queue_t * q, struct pcb_t * proc) {
        if (q == NULL || proc == NULL) return 0;

        int i;
        for (i = 0; i < q->size; i++) {
                if (q->proc[i] == proc) {
                        for (; i < q->size - 1; i++)
                                q->proc[i] = q->proc[i + 1];
                        q->size--;
                        return 1;
                }
        }
        return 0;
}
