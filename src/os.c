
#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mm.h"
#include "libmem.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int time_slot;
static int num_cpus;
static int done = 0;

#ifdef MM_PAGING
static int memramsz;
static int memswpsz[PAGING_MAX_MMSWP];

struct mmpaging_ld_args {
	/* A dispatched argument struct to compact many-fields passing to loader */
	int vmemsz;
	struct memphy_struct *mram;
	struct memphy_struct **mswp;
	struct memphy_struct *active_mswp;
	int active_mswp_id;
	struct timer_id_t  *timer_id;
};
#endif

static struct ld_args{
	char ** path;
	unsigned long * start_time;
#ifdef MLQ_SCHED
	unsigned long * prio;
#endif
} ld_processes;
int num_processes;

struct cpu_args {
	struct timer_id_t * timer_id;
	int id;
};


static void * cpu_routine(void * args) {
	struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
	int id = ((struct cpu_args*)args)->id;
	/* Check for new process in ready queue */
	int time_left = 0;
	struct pcb_t * proc = NULL;
	while (1) {
		/* Check the status of current process */
		if (proc == NULL) {
			/* No process is running, the we load new process from
		 	* ready queue */
			proc = get_proc();
			if (proc == NULL) {
                           next_slot(timer_id);
                           continue; /* First load failed. skip dummy load */
                        }
		}else if (__atomic_load_n(&proc->killed, __ATOMIC_SEQ_CST)) {
			/* The process has been terminated by killall */
			printf("\tCPU %d: Processed %2d has been killed\n",
				id ,proc->pid);
			finish_proc(proc);
			free_pcb(proc);
			proc = get_proc();
			time_left = 0;
		}else if (proc->pc == proc->code->size) {
			/* The porcess has finish it job */
			printf("\tCPU %d: Processed %2d has finished\n",
				id ,proc->pid);
			finish_proc(proc);
			free_pcb(proc);
			proc = get_proc();
			time_left = 0;
		}else if (time_left == 0) {
			/* The process has done its job in current time slot */
			printf("\tCPU %d: Put process %2d to run queue\n",
				id, proc->pid);
			put_proc(proc);
			proc = get_proc();
		}
		
		/* Recheck process status after loading new process */
		if (proc == NULL && done) {
			/* No process to run, exit */
			printf("\tCPU %d stopped\n", id);
			break;
		}else if (proc == NULL) {
			/* There may be new processes to run in
			 * next time slots, just skip current slot */
			next_slot(timer_id);
			continue;
		}else if (time_left == 0) {
			printf("\tCPU %d: Dispatched process %2d\n",
				id, proc->pid);
			time_left = time_slot;
		}
		
		/* Run current process */
		run(proc);
		time_left--;
		next_slot(timer_id);
	}
	detach_event(timer_id);
	pthread_exit(NULL);
}

static void * ld_routine(void * args) {
#ifdef MM_PAGING
	struct memphy_struct* mram = ((struct mmpaging_ld_args *)args)->mram;
	struct memphy_struct** mswp = ((struct mmpaging_ld_args *)args)->mswp;
	struct memphy_struct* active_mswp = ((struct mmpaging_ld_args *)args)->active_mswp;
	int active_mswp_id = ((struct mmpaging_ld_args *)args)->active_mswp_id;
	struct timer_id_t * timer_id = ((struct mmpaging_ld_args *)args)->timer_id;
#else
	struct timer_id_t * timer_id = (struct timer_id_t*)args;
#endif
	int i = 0;
	printf("ld_routine\n");
	while (i < num_processes) {
		struct pcb_t * proc = load(ld_processes.path[i]);
#ifdef MLQ_SCHED
		proc->prio = ld_processes.prio[i];
#endif
		while (current_time() < ld_processes.start_time[i]) {
			next_slot(timer_id);
		}
#ifdef MM_PAGING
		proc->mm = malloc(sizeof(struct mm_struct));
		init_mm(proc->mm, proc);
		proc->mram = mram;
		proc->mswp = mswp;
		proc->active_mswp = active_mswp;
		proc->active_mswp_id = active_mswp_id;
#endif
		printf("\tLoaded a process at %s, PID: %d PRIO: %ld\n",
			ld_processes.path[i], proc->pid, ld_processes.prio[i]);
		add_proc(proc);
		free(ld_processes.path[i]);
		i++;
		next_slot(timer_id);
	}
	free(ld_processes.path);
	free(ld_processes.start_time);
#ifdef MLQ_SCHED
	free(ld_processes.prio);
#endif
	done = 1;
	detach_event(timer_id);
	pthread_exit(NULL);
}

static void read_config(const char * path) {
	FILE * file;
	if ((file = fopen(path, "r")) == NULL) {
		printf("Cannot find configure file at %s\n", path);
		exit(1);
	}
	if (fscanf(file, "%d %d %d\n", &time_slot, &num_cpus, &num_processes) != 3 ||
	    time_slot <= 0 || num_cpus <= 0 || num_processes <= 0) {
		printf("Invalid config header in %s\n", path);
		exit(1);
	}
	ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);
	ld_processes.start_time = (unsigned long*)
		malloc(sizeof(unsigned long) * num_processes);
#ifdef MM_PAGING
	/* The memory config line is optional:
	 *        MEM_RAM_SZ MEM_SWP0_SZ MEM_SWP1_SZ MEM_SWP2_SZ MEM_SWP3_SZ
	 * (size=0 result non-used memswap, must have RAM and at least 1 SWAP)
	 * Legacy config files (scheduler only) omit it, then the default
	 * sizes are used: 1MB of RAM and one 16MB SWAP.
	 */
	int sit;
	char line[256];
	long line_pos = ftell(file);
	int memsz[1 + PAGING_MAX_MMSWP];

	if (fgets(line, sizeof(line), file) != NULL &&
	    sscanf(line, "%d %d %d %d %d", &memsz[0], &memsz[1], &memsz[2],
	           &memsz[3], &memsz[4]) == 1 + PAGING_MAX_MMSWP) {
		memramsz = memsz[0];
		for(sit = 0; sit < PAGING_MAX_MMSWP; sit++)
			memswpsz[sit] = memsz[1 + sit];
	} else {
		/* Not a memory config line, it is the first process */
		fseek(file, line_pos, SEEK_SET);
		memramsz    =  0x100000;
		memswpsz[0] = 0x1000000;
		for(sit = 1; sit < PAGING_MAX_MMSWP; sit++)
			memswpsz[sit] = 0;
	}
	if (memramsz < PAGING_PAGESZ || memswpsz[0] < PAGING_PAGESZ) {
		printf("Invalid memory config: need RAM and SWAP0 >= %d bytes\n",
			PAGING_PAGESZ);
		exit(1);
	}
#endif

#ifdef MLQ_SCHED
	ld_processes.prio = (unsigned long*)
		malloc(sizeof(unsigned long) * num_processes);
#endif
	int i;
	for (i = 0; i < num_processes; i++) {
		ld_processes.path[i] = (char*)malloc(sizeof(char) * 100);
		char proc[64];
#ifdef MLQ_SCHED
		if (fscanf(file, "%lu %63s %lu\n", &ld_processes.start_time[i], proc, &ld_processes.prio[i]) != 3) {
#else
		if (fscanf(file, "%lu %63s\n", &ld_processes.start_time[i], proc) != 2) {
#endif
			printf("Invalid process line %d in %s\n", i + 1, path);
			exit(1);
		}
		snprintf(ld_processes.path[i], 100, "input/proc/%s", proc);
	}
	fclose(file);
}

int main(int argc, char * argv[]) {
	/* Read config */
	if (argc != 2) {
		printf("Usage: os [path to configure file]\n");
		return 1;
	}
	char path[256];
	snprintf(path, sizeof(path), "input/%s", argv[1]);
	read_config(path);

	pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	struct cpu_args * args =
		(struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
	pthread_t ld;
	
	/* Init timer */
	int i;
	for (i = 0; i < num_cpus; i++) {
		args[i].timer_id = attach_event();
		args[i].id = i;
	}
	struct timer_id_t * ld_event = attach_event();
	start_timer();

#ifdef MM_PAGING
	/* Init all MEMPHY include 1 MEMRAM and n of MEMSWP */
	int rdmflag = 1; /* By default memphy is RANDOM ACCESS MEMORY */

	struct memphy_struct mram;
	struct memphy_struct mswp[PAGING_MAX_MMSWP];
	struct memphy_struct *mswp_list[PAGING_MAX_MMSWP];

	/* Create MEM RAM */
	init_memphy(&mram, memramsz, rdmflag);

        /* Create all MEM SWAP */ 
	int sit;
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++) {
	       init_memphy(&mswp[sit], memswpsz[sit], rdmflag);
	       mswp_list[sit] = &mswp[sit];
	}

	/* In Paging mode, it needs passing the system mem to each PCB through loader*/
	struct mmpaging_ld_args *mm_ld_args = malloc(sizeof(struct mmpaging_ld_args));

	mm_ld_args->timer_id = ld_event;
	mm_ld_args->mram = (struct memphy_struct *) &mram;
	mm_ld_args->mswp = mswp_list;
	mm_ld_args->active_mswp = (struct memphy_struct *) &mswp[0];
        mm_ld_args->active_mswp_id = 0;
#endif

	/* Init scheduler */
	init_scheduler();

	/* Run CPU and loader */
#ifdef MM_PAGING
	pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);
#else
	pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif
	for (i = 0; i < num_cpus; i++) {
		pthread_create(&cpu[i], NULL,
			cpu_routine, (void*)&args[i]);
	}

	/* Wait for CPU and loader finishing */
	for (i = 0; i < num_cpus; i++) {
		pthread_join(cpu[i], NULL);
	}
	pthread_join(ld, NULL);

	/* Stop timer */
	stop_timer();

	return 0;

}


