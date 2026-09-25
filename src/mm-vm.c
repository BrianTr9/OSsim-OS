// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

 #include "string.h"
 #include "mm.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 
 /*get_vma_by_num - get vm area by numID
  *@mm: memory region
  *@vmaid: ID vm area to alloc memory region
  *
  */
 struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
 {
   struct vm_area_struct *pvma = mm->mmap;
 
   while (pvma != NULL && pvma->vm_id != vmaid)
     pvma = pvma->vm_next;
 
   return pvma;
 }
 
 int __mm_swap_page(struct pcb_t *caller, int vicfpn , int swpfpn)
 {
     __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
     return 0;
 }
 
 /*get_vm_area_node - get vm area for a number of pages
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@incpgnum: number of page
  *@vmastart: vma end
  *@vmaend: vma end
  *
  */
 struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
 {
   struct vm_rg_struct * newrg;
   /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (cur_vma == NULL)
     return NULL;
 
   newrg = malloc(sizeof(struct vm_rg_struct));
 
   /* TODO: update the newrg boundary*/
   newrg->rg_start = cur_vma->sbrk;
   newrg->rg_end = newrg->rg_start + alignedsz;
   newrg->rg_next = NULL;
   return newrg;
 }
 
 /*validate_overlap_vm_area
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@vmastart: vma end
  *@vmaend: vma end
  *
  */
 int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
 {
   if(vmastart >= vmaend) return -1;
   struct vm_area_struct *vma = caller->mm->mmap;
   /* The planned area [vmastart, vmaend) must not overlap other areas */
   while(vma){
     if(vma->vm_id != vmaid && vma->vm_start < vma->vm_end &&
        vmastart < vma->vm_end && vma->vm_start < vmaend){
       return -1;
     }
     vma = vma->vm_next;
   }
 
   return 0;
 }
 
 /*inc_vma_limit - increase vm area limits to reserve space for new variable
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@inc_sz: increment size
  *
  */
 int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
 {
   int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
   int incnumpage =  inc_amt / PAGING_PAGESZ;
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
   struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
 
   if (cur_vma == NULL || area == NULL || inc_amt <= 0) {
     free(area);
     return -1;
   }
 
   /*Validate overlap of obtained region */
   if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0){
     free(area);
     return -1; /*Overlap and failed allocation */
   }
 
   /* Map the memory to MEMRAM, [area] receives the mapped region */
   if (vm_map_ram(caller, area->rg_start, area->rg_end, 
                     cur_vma->vm_end, incnumpage, area) < 0){
     free(area);
     return -1;
   }
   
   cur_vma->vm_end += inc_amt;
   cur_vma->sbrk += inc_amt;
 
   /* The new space is free, merge it with the free tail if any */
   enlist_vm_freerg_list(cur_vma, area);
   return 0;
 }
 
 // #endif