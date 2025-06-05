#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "threads/synch.h"

/* Frame table entry */
struct frame 
{
    void *kpage;                 /* Kernel virtual address */
    struct page *page;           /* Page occupying frame */
    struct thread *owner;        /* Thread that owns frame */
    struct list_elem elem;       /* List element */
};

/* Initialize frame table */
void frame_init (void);

/* Frame allocation and deallocation */
void *frame_alloc (struct page *page, enum palloc_flags flags);
void frame_free (void *frame);

/* Frame eviction */
struct frame *frame_evict (void);

#endif /* vm/frame.h */