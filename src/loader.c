
#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t avail_pid = 1;

#define OPT_CALC	"calc"
#define OPT_ALLOC	"alloc"
#define OPT_FREE	"free"
#define OPT_READ	"read"
#define OPT_WRITE	"write"
#define OPT_SYSCALL	"syscall"

static enum ins_opcode_t get_opcode(char * opt) {
	if (!strcmp(opt, OPT_CALC)) {
		return CALC;
	}else if (!strcmp(opt, OPT_ALLOC)) {
		return ALLOC;
	}else if (!strcmp(opt, OPT_FREE)) {
		return FREE;
	}else if (!strcmp(opt, OPT_READ)) {
		return READ;
	}else if (!strcmp(opt, OPT_WRITE)) {
		return WRITE;
	}else if (!strcmp(opt, OPT_SYSCALL)) {
		return SYSCALL;
	}else{
		printf("get_opcode return Opcode: %s\n", opt);
		exit(1);
	}
}

struct pcb_t * load(const char * path) {
	/* Create new PCB for the new process */
	struct pcb_t * proc = (struct pcb_t * )calloc(1, sizeof(struct pcb_t));
	proc->pid = avail_pid;
	avail_pid++;
	proc->page_table =
		(struct page_table_t*)malloc(sizeof(struct page_table_t));
	proc->bp = PAGE_SIZE;
	proc->pc = 0;
	proc->killed = 0;

	/* Read process code from file */
	FILE * file;
	if ((file = fopen(path, "r")) == NULL) {
		printf("Cannot find process description at '%s'\n", path);
		exit(1);		
	}
	snprintf(proc->path, sizeof(proc->path), "%s", path);
	char opcode[10];
	proc->code = (struct code_seg_t*)malloc(sizeof(struct code_seg_t));
	if (fscanf(file, "%u %u", &proc->priority, &proc->code->size) != 2) {
		printf("Invalid process header in '%s'\n", path);
		exit(1);
	}
	proc->code->text = (struct inst_t*)malloc(
		sizeof(struct inst_t) * proc->code->size
	);
	uint32_t i = 0;
	char buf[200];
	for (i = 0; i < proc->code->size; i++) {
		if (fscanf(file, "%9s", opcode) != 1) {
			printf("'%s': expected %u instructions, found %u\n",
				path, proc->code->size, i);
			exit(1);
		}
		proc->code->text[i].opcode = get_opcode(opcode);
		switch(proc->code->text[i].opcode) {
		case CALC:
			break;
		case ALLOC:
			fscanf(
				file,
				"%u %u\n",
				&proc->code->text[i].arg_0,
				&proc->code->text[i].arg_1
			);
			break;
		case FREE:
			fscanf(file, "%u\n", &proc->code->text[i].arg_0);
			break;
		case READ:
		case WRITE:
			fscanf(
				file,
				"%u %u %u\n",
				&proc->code->text[i].arg_0,
				&proc->code->text[i].arg_1,
				&proc->code->text[i].arg_2
			);
			break;	
		case SYSCALL:
			proc->code->text[i].arg_1 = 0;
			proc->code->text[i].arg_2 = 0;
			proc->code->text[i].arg_3 = 0;
			if (fgets(buf, sizeof(buf), file) == NULL)
				buf[0] = '\0';
			sscanf(buf, "%u%u%u%u",
			           &proc->code->text[i].arg_0,
			           &proc->code->text[i].arg_1,
			           &proc->code->text[i].arg_2,
			           &proc->code->text[i].arg_3
			);
			break;
		default:
			printf("Opcode: %s\n", opcode);
			exit(1);
		}
	}
	fclose(file);
	return proc;
}



