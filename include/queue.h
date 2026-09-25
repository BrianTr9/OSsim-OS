
#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#define MAX_QUEUE_SIZE 100

struct queue_t {
	struct pcb_t * proc[MAX_QUEUE_SIZE];
	int size;
};

void enqueue(struct queue_t * q, struct pcb_t * proc);

struct pcb_t * dequeue(struct queue_t * q);

int empty(struct queue_t * q);

/* Remove [proc] from [q], keep FIFO order. Return 1 if found */
int remove_from_queue(struct queue_t * q, struct pcb_t * proc);

#endif

