/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"
#include "stdio.h"  
#include "libmem.h"
#include "string.h"
#include "queue.h"

/* Global variable tracking the currently running process */
struct pcb_t *current_running = NULL;

/* Simple spinlock mechanism for synchronization */
volatile int queue_lock = 0;
volatile int current_running_lock = 0;

/**
 * Simple spinlock acquire function
 * @param lock Pointer to the lock variable
 */
void acquire_lock(volatile int *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        // Wait until lock is released
    }
}

/**
 * Simple spinlock release function
 * @param lock Pointer to the lock variable
 */
void release_lock(volatile int *lock) {
    __sync_lock_release(lock);
}

/**
 * Remove processes from a queue whose name matches the specified name
 * @param q The queue to search through
 * @param name The process name to match
 * @param caller The calling process (which will not be terminated)
 * @return Number of processes terminated
 */
int remove_process_by_name(struct queue_t *q, const char *name, struct pcb_t *caller);

/**
 * Terminate a specific process and clean up its resources
 * @param proc The process to terminate
 */
void terminate_process(struct pcb_t *proc);

/**
 * Select the next process to run after current one is terminated
 * @param caller The calling process
 */
void scheduler_pick_next_process(struct pcb_t *caller)
{
    acquire_lock(&queue_lock);
    acquire_lock(&current_running_lock);
    
    if (caller->ready_queue && caller->ready_queue->size > 0)
    {
        struct pcb_t *next_proc = caller->ready_queue->proc[0];
        current_running = next_proc;
        printf("Next running process: %s (pid=%d)\n", current_running->path, current_running->pid);
    }
    else
    {
        current_running = NULL;
    }
    
    release_lock(&current_running_lock);
    release_lock(&queue_lock);
}

/**
 * System call implementation to terminate all processes with a given name
 * @param caller The process making the system call
 * @param regs Register state containing system call parameters
 * @return Number of processes terminated
 */
int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    char proc_name[100];
    uint32_t data;
    int terminated_count = 0;

    // Get region ID from syscall parameter
    uint32_t memrg = regs->a1;

    /* Retrieve process name from memory region with bounds checking */
    int i = 0;
    data = 0;
    while(data != -1 && i < 99) // Add boundary check for safety
    {
        libread(caller, memrg, i, &data);
        proc_name[i] = data;
        if(data == -1) proc_name[i] = '\0';
        i++;
    }
    // Ensure null termination
    proc_name[i < 100 ? i : 99] = '\0';
    
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    // Process all queues with proper synchronization
    acquire_lock(&queue_lock);
    
    // Process regular ready queue
    if (caller->ready_queue)
    {
        terminated_count += remove_process_by_name(caller->ready_queue, proc_name, caller);
    }

    // Process running list
    if (caller->running_list)
    {
        terminated_count += remove_process_by_name(caller->running_list, proc_name, caller);
    }

    // Process multi-level queues if enabled
    #ifdef MLQ_SCHED
    if (caller->mlq_ready_queue)
    {
        for (int i = 0; i < MAX_PRIO; ++i)
        {
            terminated_count += remove_process_by_name(&caller->mlq_ready_queue[i], proc_name, caller);
        }
    }
    #endif
    
    release_lock(&queue_lock);

    // Special handling for current running process - needs separate lock
    acquire_lock(&current_running_lock);
    struct pcb_t *curr_proc = current_running; // Local copy for safe access
    if (curr_proc && strcmp(curr_proc->path, proc_name) == 0 && curr_proc != caller)
    {
        printf("Terminating current running process: %s (pid=%d)\n", curr_proc->path, curr_proc->pid);
        
        // Terminate the process properly
        terminate_process(curr_proc);
        
        current_running = NULL;
        terminated_count++;

        release_lock(&current_running_lock);
        
        // Pick next process to run
        scheduler_pick_next_process(caller);
    }
    else
    {
        release_lock(&current_running_lock);
    }

    printf("Total processes terminated by killall: %d\n", terminated_count);
    return terminated_count; 
}

/**
 * Terminate a process and clean up its resources
 */
void terminate_process(struct pcb_t *proc)
{
    if (!proc) return;
    
    printf("Terminating process: %s (pid=%d)\n", proc->path, proc->pid);
    
    // Free any allocated memory
    #ifdef MM_PAGING
    if (proc->mm) {
        // Free memory regions in page table
        for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++) {
            if (proc->mm->symrgtbl[i].rg_start != 0 || proc->mm->symrgtbl[i].rg_end != 0) {
                // Free this memory region
                printf("Freeing memory region %d [%ld-%ld] for terminated process\n", 
                       i, proc->mm->symrgtbl[i].rg_start, proc->mm->symrgtbl[i].rg_end);
                
                // Reset region parameters
                proc->mm->symrgtbl[i].rg_start = 0;
                proc->mm->symrgtbl[i].rg_end = 0;
            }
        }
    }
    #endif
    
    // Mark as terminated
    proc->pc = 0xFFFFFFFF;  // Set PC to invalid position to mark termination
}

/**
 * Remove processes from queue that match the given name
 * @return Number of processes terminated
 */
int remove_process_by_name(struct queue_t *q, const char *name, struct pcb_t *caller)
{
    int terminated = 0;
    
    int i = 0;
    while (i < q->size)
    {
        struct pcb_t *proc = q->proc[i];
        if (proc && strcmp(proc->path, name) == 0 && proc != caller)
        {
            printf("Found process to terminate: %s (pid=%d)\n", proc->path, proc->pid);
            
            // Terminate the process first
            terminate_process(proc);
            terminated++;
            
            // Remove from queue
            memmove(&q->proc[i], &q->proc[i + 1], (q->size - i - 1) * sizeof(struct pcb_t *));
            q->size--;
        } 
        else 
        {
            i++;
        }
    }
    
    return terminated;
}

