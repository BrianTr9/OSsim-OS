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
#include "sched.h"
#include "mm.h"

#define PROC_NAME_MAX 100

/**
 * System call implementation to terminate all processes with a given name
 * @param caller The process making the system call
 * @param regs Register state containing system call parameters
 *             a1: memory region ID holding the process name
 * @return Number of processes terminated
 */
int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    char proc_name[PROC_NAME_MAX];
    uint32_t memrg = regs->a1;
    int i;

    /* Retrieve process name from memory region, it ends with
     * a 0 or -1 byte, or at the end of the region */
    for (i = 0; i < PROC_NAME_MAX - 1; i++)
    {
        BYTE data;
        if (__read(caller, 0, memrg, i, &data) != 0)
            break;
        if (data == 0 || data == (BYTE)-1)
            break;
        proc_name[i] = data;
    }
    proc_name[i] = '\0';

    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    struct pcb_t *victims[MAX_QUEUE_SIZE];
    int nvictims;
    int terminated_count = kill_proc_by_name(proc_name, caller, victims, &nvictims);

    /* Processes taken out of the ready queues are no longer referenced
     * by the scheduler, release them here */
    for (i = 0; i < nvictims; i++)
    {
        printf("Terminating process: %s (pid=%d)\n", victims[i]->path, victims[i]->pid);
        free_pcb(victims[i]);
    }

    printf("Total processes terminated by killall: %d\n", terminated_count);
    return terminated_count; 
}
