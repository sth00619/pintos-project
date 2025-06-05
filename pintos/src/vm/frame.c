#include "vm/frame.h"
#include <debug.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"

/* Frame table */
static struct list frame_table;
static struct lock frame_lock;

/* Initialize frame table */
void
frame_init (void)
{
    list_init (&frame_table);
    lock_init (&frame_lock);
}

/* Allocate a frame for a page */
void *
frame_alloc (struct page *page, enum palloc_flags flags)
{
    if ((flags & PAL_USER) == 0)
        return NULL;
    
    lock_acquire (&frame_lock);
    
    void *kpage = palloc_get_page (flags);
    if (kpage == NULL)
    {
        /* Try to evict a frame */
        struct frame *victim = frame_evict ();
        if (victim == NULL)
        {
            lock_release (&frame_lock);
            PANIC ("Out of frames");
        }
        kpage = victim->kpage;
    }
    
    struct frame *frame = malloc (sizeof (struct frame));
    if (frame == NULL)
    {
        lock_release (&frame_lock);
        palloc_free_page (kpage);
        return NULL;
    }
    
    frame->kpage = kpage;
    frame->page = page;
    frame->owner = thread_current ();
    list_push_back (&frame_table, &frame->elem);
    
    lock_release (&frame_lock);
    return kpage;
}

/* Free a frame */
void
frame_free (void *kpage)
{
    lock_acquire (&frame_lock);
    
    struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table);
         e = list_next (e))
    {
        struct frame *frame = list_entry (e, struct frame, elem);
        if (frame->kpage == kpage)
        {
            list_remove (e);
            palloc_free_page (kpage);
            free (frame);
            break;
        }
    }
    
    lock_release (&frame_lock);
}

/* Evict a frame using clock algorithm */
struct frame *
frame_evict (void)
{
    static struct list_elem *clock_hand = NULL;
    
    if (list_empty (&frame_table))
        return NULL;
    
    if (clock_hand == NULL || clock_hand == list_end (&frame_table))
        clock_hand = list_begin (&frame_table);
    
    while (true)
    {
        struct frame *frame = list_entry (clock_hand, struct frame, elem);
        struct page *page = frame->page;
        
        if (page->pinned)
        {
            clock_hand = list_next (clock_hand);
            if (clock_hand == list_end (&frame_table))
                clock_hand = list_begin (&frame_table);
            continue;
        }
        
        /* Check if page was accessed */
        if (pagedir_is_accessed (frame->owner->pagedir, page->vaddr))
        {
            pagedir_set_accessed (frame->owner->pagedir, page->vaddr, false);
            clock_hand = list_next (clock_hand);
            if (clock_hand == list_end (&frame_table))
                clock_hand = list_begin (&frame_table);
            continue;
        }
        
        /* Found victim */
        if (pagedir_is_dirty (frame->owner->pagedir, page->vaddr) || 
            page->status == PAGE_ZERO)
        {
            /* Page is dirty or anonymous, swap out */
            page->swap_slot = swap_out (frame->kpage);
            page->status = PAGE_SWAP;
        }
        else if (page->status == PAGE_FILE)
        {
            /* Clean file page, can just discard */
        }
        
        /* Remove from page table */
        pagedir_clear_page (frame->owner->pagedir, page->vaddr);
        page->status = (page->status == PAGE_FILE) ? PAGE_FILE : PAGE_SWAP;
        page->frame = NULL;
        
        list_remove (&frame->elem);
        struct frame *victim = frame;
        
        clock_hand = list_next (clock_hand);
        if (clock_hand == list_end (&frame_table))
            clock_hand = list_begin (&frame_table);
        
        return victim;
    }
}