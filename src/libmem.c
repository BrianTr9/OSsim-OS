/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

 #include "string.h"
 #include "mm.h"
 #include "syscall.h"
 #include "libmem.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 
 static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;
 
 /*enlist_vm_freerg_list - add new rg to freerg_list of a vm area,
  *                        merging it with its adjacent free regions
  *@vma: vm area
  *@rg_elmt: new region
  *
  */
 int enlist_vm_freerg_list(struct vm_area_struct *vma, struct vm_rg_struct *rg_elmt)
 {
   struct vm_rg_struct **p_rgit = &vma->vm_freerg_list;
 
   if (rg_elmt->rg_start >= rg_elmt->rg_end)
     return -1;
 
   while (*p_rgit)
   {
     struct vm_rg_struct *rgit = *p_rgit;
     if (rgit->rg_end == rg_elmt->rg_start || rg_elmt->rg_end == rgit->rg_start)
     {
       if (rgit->rg_start < rg_elmt->rg_start)
         rg_elmt->rg_start = rgit->rg_start;
       if (rgit->rg_end > rg_elmt->rg_end)
         rg_elmt->rg_end = rgit->rg_end;
       *p_rgit = rgit->rg_next;
       free(rgit);
       continue;
     }
     p_rgit = &rgit->rg_next;
   }
 
   /* Enlist the new region */
   rg_elmt->rg_next = vma->vm_freerg_list;
   vma->vm_freerg_list = rg_elmt;
 
   return 0;
 }
 
 /*get_symrg_byid - get mem region by region ID
  *@mm: memory region
  *@rgid: region ID act as symbol index of variable
  *
  */
 struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
 {
   if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
     return NULL;
 
   return &mm->symrgtbl[rgid];
 }
 
 /*__alloc - allocate a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *@alloc_addr: address of allocated memory region
  *
  */
 int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
 {
   if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ || size <= 0)
     return -1;
   /*Allocate at the toproof */
   pthread_mutex_lock(&mmvm_lock);
   struct vm_rg_struct rgnode;
 
   if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
   {
     caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
     caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
  
     *alloc_addr = rgnode.rg_start;
 
     pthread_mutex_unlock(&mmvm_lock);
     return 0;
   }
 
   /* get_free_vmrg_area FAILED, increase the limit of the vm area */
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
   if(cur_vma == NULL){
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   /* INCREASE THE LIMIT as invoking systemcall 
    * sys_memap with SYSMEM_INC_OP 
    */
   struct sc_regs regs;
   regs.a1 = SYSMEM_INC_OP;
   regs.a2 = vmaid;
   regs.a3 = size;
   /* SYSCALL 17 sys_memmap */
   syscall(caller, 17, &regs);
   
   /* Commit the allocation on the enlarged area */
   if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0){
     caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
     caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
  
     *alloc_addr = rgnode.rg_start;
 
     pthread_mutex_unlock(&mmvm_lock);
     return 0;
   }
   pthread_mutex_unlock(&mmvm_lock);
   return -1;
 
 }
 
 /*__free - remove a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __free(struct pcb_t *caller, int vmaid, int rgid)
 {
   struct vm_rg_struct *rgnode;
   struct vm_area_struct *cur_vma;
 
   if(rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
     return -1;
   pthread_mutex_lock(&mmvm_lock);
 
   cur_vma = get_vma_by_num(caller->mm, vmaid);
   if (cur_vma == NULL ||
       caller->mm->symrgtbl[rgid].rg_start >= caller->mm->symrgtbl[rgid].rg_end){
     /* Unknown area or region is not allocated (or already freed) */
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }  
 
   rgnode = init_vm_rg(caller->mm->symrgtbl[rgid].rg_start,
                       caller->mm->symrgtbl[rgid].rg_end);
 
   /* The symbol does not own the region anymore */
   caller->mm->symrgtbl[rgid].rg_start = 0;
   caller->mm->symrgtbl[rgid].rg_end = 0;
 
   /*enlist the obsoleted memory region */
   enlist_vm_freerg_list(cur_vma, rgnode);
   pthread_mutex_unlock(&mmvm_lock);
 
   return 0;
 }
 
 /*liballoc - PAGING-based allocate a region memory
  *@proc:  Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
 {
   /* TODO Implement allocation on vm area 0 */
   int addr;
 
   /* By default using vmaid = 0 */
   int ret = __alloc(proc, 0, reg_index, size, &addr);
   if (ret == -1)  return -1;
 
   printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
   printf("PID=%d - Region=%d - Address=%08x - Size=%d byte\n", proc->pid, reg_index,addr, size);
   // printf("Allocated region: [%d - %d]\n", proc->mm->symrgtbl[reg_index].rg_start, proc->mm->symrgtbl[reg_index].rg_end);
   print_pgtbl(proc, 0, -1);
  
   // return __alloc(proc, 0, reg_index, size, &addr);
   return ret;
 }
 
 /*libfree - PAGING-based free a region memory
  *@proc: Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 
 int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  /* Display deallocation notification */
  printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
  printf("PID=%d - Region=%d\n", proc->pid, reg_index);
  
  /* By default using vmaid = 0 */
  int ret = __free(proc, 0, reg_index);
  
  /* Print page table if deallocation was successful */
  if (ret == 0) {
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); //print max TBL
#endif
  }
  
  return ret;
}
 
 /*__swap_out_page - move the victim page of caller from MEMRAM to MEMSWP
  *@caller: caller
  *@retfpn: return the released frame in MEMRAM
  *
  */
 int __swap_out_page(struct pcb_t *caller, int *retfpn)
 {
   int vicpgn, vicfpn, swpfpn;
 
   /* Find victim page */
   if (find_victim_page(caller->mm, &vicpgn) != 0)
     return -1;
 
   /* Get free frame in MEMSWP */
   if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0)
   {
     /* Swap is full, the victim stays in RAM */
     enlist_pgn_node(&caller->mm->fifo_pgn, vicpgn);
     return -1;
   }
 
   vicfpn = PAGING_FPN(caller->mm->pgd[vicpgn]);
 
   /* Copy victim frame to swap
    * SWP(vicfpn <--> swpfpn)
    * SYSCALL 17 sys_memmap with operation SYSMEM_SWP_OP
    */
   struct sc_regs regs;
   regs.a1 = SYSMEM_SWP_OP;
   regs.a2 = vicfpn;
   regs.a3 = swpfpn;
   syscall(caller, 17, &regs);
 
   /* The victim page is now offline */
   pte_set_swap(&caller->mm->pgd[vicpgn], caller->active_mswp_id, swpfpn);
 
   *retfpn = vicfpn;
   return 0;
 }
 
 /*pg_getpage - get the page in ram
  *@mm: memory region
  *@pagenum: PGN
  *@framenum: return FPN
  *@caller: caller
  *
  */
 int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
 {
   if (pgn < 0 || pgn >= PAGING_MAX_PGN)
     return -1;
 
   uint32_t pte = mm->pgd[pgn];
 
   if (!PAGING_PAGE_PRESENT(pte))
     return -1; /* Page has never been mapped */
 
   if (pte & PAGING_PTE_SWAPPED_MASK)
   { /* Page is not online, make it actively living */
     int swptyp = GETVAL(pte, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
     int tgtswpfpn = PAGING_PTE_SWP(pte); /* the swap frame storing our page */
     struct memphy_struct *tgtswp = caller->mswp[swptyp];
     int tgtfpn;
 
     /* Get a frame in MEMRAM, evict a victim page if RAM is full */
     if (MEMPHY_get_freefp(caller->mram, &tgtfpn) != 0 &&
         __swap_out_page(caller, &tgtfpn) != 0)
       return -1;
 
     /* Copy target frame from swap to mem */
     __swap_cp_page(tgtswp, tgtswpfpn, caller->mram, tgtfpn);
     MEMPHY_put_freefp(tgtswp, tgtswpfpn);
 
     /* Update its online status of the target page */
     pte_set_fpn(&mm->pgd[pgn], tgtfpn);
     enlist_pgn_node(&mm->fifo_pgn, pgn);
   }
 
   *fpn = PAGING_FPN(mm->pgd[pgn]);
 
   return 0;
 }
 
 /*pg_getval - read value at given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
 int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
 {
   int pgn = PAGING_PGN(addr);
   int off = PAGING_OFFST(addr);
   int fpn;
 
   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
     return -1; /* invalid page access */
 
   int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
   /* TODO 
    *  MEMPHY_read(caller->mram, phyaddr, data);
    *  MEMPHY READ 
    *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
    */
   struct sc_regs regs;
   regs.a1 = SYSMEM_IO_READ;
   regs.a2 = phyaddr;
   // regs.a3 = NULL;
 
   /* SYSCALL 17 sys_memmap */
   // __sys_memmap(caller, &regs);
   syscall(caller, 17, &regs);
 
   // Update data
   *data = (BYTE)regs.a3;
 
   return 0;
 }
 
 /*pg_setval - write value to given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
 int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
 {
   int pgn = PAGING_PGN(addr);
   int off = PAGING_OFFST(addr);
   int fpn;
 
   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
     return -1; /* invalid page access */
 
   int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
   /* TODO
    *  MEMPHY_write(caller->mram, phyaddr, value);
    *  MEMPHY WRITE
    *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
    */
   struct sc_regs regs;
   regs.a1 = SYSMEM_IO_WRITE;
   regs.a2 = phyaddr;
   regs.a3 = value;
 
   /* SYSCALL 17 sys_memmap */
   // __sys_memmap(caller, &regs);
   syscall(caller, 17, &regs);
   // Update data
   // data = (BYTE) ;
 
   return 0;
 }
 
 /*__read - read value in region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
 {
   int ret;
 
   pthread_mutex_lock(&mmvm_lock);
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL ||   /* Invalid memory identify */
       offset < 0 || currg->rg_start + offset >= currg->rg_end) /* Out of region */
   {
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   ret = pg_getval(caller->mm, currg->rg_start + offset, data, caller);
   pthread_mutex_unlock(&mmvm_lock);
 
   return ret;
 }
 
 /*libread - PAGING-based read a region memory */
 int libread(
     struct pcb_t *proc, // Process executing the instruction
     uint32_t source,    // Index of source register
     uint32_t offset,    // Source address = [source] + [offset]
     uint32_t* destination)
 {
   BYTE data;
   int val = __read(proc, 0, source, offset, &data);
 
   if (val == -1)
     return -1;
   *destination = data;
   printf("===== PHYSICAL MEMORY AFTER READING =====\n");
 #ifdef IODUMP
   printf("read region=%d offset=%d value=%d\n", source, offset, data);
 #ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1); //print max TBL
 #endif

   MEMPHY_dump(proc->mram);
 
 #endif
 
   return val;
 }
 
 /*__write - write a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
 {
   int ret;
 
   pthread_mutex_lock(&mmvm_lock);
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL ||   /* Invalid memory identify */
       offset < 0 || currg->rg_start + offset >= currg->rg_end) /* Out of region */
   {
     pthread_mutex_unlock(&mmvm_lock);
     return -1;
   }
 
   ret = pg_setval(caller->mm, currg->rg_start + offset, value, caller);
   pthread_mutex_unlock(&mmvm_lock);
 
   return ret;
 }
 
 /*libwrite - PAGING-based write a region memory */
 int libwrite(
     struct pcb_t *proc,   // Process executing the instruction
     BYTE data,            // Data to be wrttien into memory
     uint32_t destination, // Index of destination register
     uint32_t offset)
 {
   int val = __write(proc, 0, destination, offset, data);
   if (val == -1)
     return -1;
  printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
 #ifdef IODUMP
   printf("write region=%d offset=%d value=%d\n", destination, offset, data);
 #ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1); //print max TBL
 #endif
   MEMPHY_dump(proc->mram);
  
 #endif
 
   return val;
 }
 
 /*free_pcb_memphy - collect all memphy of pcb
  *@caller: caller
  *
  */
 int free_pcb_memph(struct pcb_t *caller)
 {
   int pagenum;
   uint32_t pte;
 
   for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
   {
     pte = caller->mm->pgd[pagenum];
 
     if (!PAGING_PAGE_PRESENT(pte))
       continue;
 
     if (pte & PAGING_PTE_SWAPPED_MASK)
     {
       int swptyp = GETVAL(pte, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
       MEMPHY_put_freefp(caller->mswp[swptyp], PAGING_PTE_SWP(pte));
     } else {
       MEMPHY_put_freefp(caller->mram, PAGING_FPN(pte));
     }
     caller->mm->pgd[pagenum] = 0;
   }
 
   return 0;
 }
 
 /*free_pcb - release a terminated process and all its resources
  *@proc: process
  *
  */
 void free_pcb(struct pcb_t *proc)
 {
   if (proc == NULL)
     return;
 
   if (proc->mm)
   {
     struct mm_struct *mm = proc->mm;
 
     pthread_mutex_lock(&mmvm_lock);
     free_pcb_memph(proc);
     pthread_mutex_unlock(&mmvm_lock);
 
     while (mm->fifo_pgn)
     {
       struct pgn_t *pg = mm->fifo_pgn;
       mm->fifo_pgn = pg->pg_next;
       free(pg);
     }
     while (mm->mmap)
     {
       struct vm_area_struct *vma = mm->mmap;
       mm->mmap = vma->vm_next;
       while (vma->vm_freerg_list)
       {
         struct vm_rg_struct *rg = vma->vm_freerg_list;
         vma->vm_freerg_list = rg->rg_next;
         free(rg);
       }
       free(vma);
     }
     free(mm->pgd);
     free(mm);
   }
 
   if (proc->code)
   {
     free(proc->code->text);
     free(proc->code);
   }
   free(proc->page_table);
   free(proc);
 }
 
 /*find_victim_page - find victim page (FIFO: the oldest enlisted page)
  *@caller: caller
  *@pgn: return page number
  *
  */
 int find_victim_page(struct mm_struct *mm, int *retpgn)
 {
   struct pgn_t **p_pg = &mm->fifo_pgn;
 
   if (*p_pg == NULL)
     return -1;
 
   /* New pages are enlisted at the head, the victim is the tail */
   while ((*p_pg)->pg_next)
     p_pg = &(*p_pg)->pg_next;
 
   *retpgn = (*p_pg)->pgn;
   free(*p_pg);
   *p_pg = NULL;
 
   return 0;
 }
 
 /*get_free_vmrg_area - get a free vm region
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@size: allocated size
  *
  */
 int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
 {
   struct vm_area_struct * vma = get_vma_by_num(caller->mm, vmaid);
   if (vma == NULL) return -1;
   for (struct vm_rg_struct ** p_rgit = &vma->vm_freerg_list; *p_rgit; p_rgit = &((*p_rgit)->rg_next)) {
     struct vm_rg_struct * rgit = *p_rgit;
     if (rgit->rg_start + size == rgit->rg_end) {
       newrg->rg_start = rgit->rg_start;
       newrg->rg_end = rgit->rg_end;
       *p_rgit = rgit->rg_next;
       free(rgit);
       return 0;
     }
     if (rgit->rg_start + size < rgit->rg_end) {
       newrg->rg_start = rgit->rg_start;
       newrg->rg_end = rgit->rg_start += size;
       return 0;
     }
   }
   return -1;
 }
 
 //#endif