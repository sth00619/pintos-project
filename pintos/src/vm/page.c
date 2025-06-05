#include "vm/page.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"

/* File system lock functions */
extern void acquire_filesys_lock (void);
extern void release_filesys_lock (void);

/* Hash function for supplemental page table */
unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
    const struct page *p = hash_entry (e, struct page, hash_elem);
    return hash_bytes (&p->vaddr, sizeof p->vaddr);
}

/* Comparison function for supplemental page table */
bool
page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    const struct page *pa = hash_entry (a, struct page, hash_elem);
    const struct page *pb = hash_entry (b, struct page, hash_elem);
    return pa->vaddr < pb->vaddr;
}

/* Initialize page system */
void
page_init (void)
{
    /* Page initialization is done per-thread */
}

/* Create a new page entry */
struct page *
page_create (void *vaddr, bool writable)
{
    struct page *page = malloc (sizeof (struct page));
    if (page == NULL)
        return NULL;
    
    page->vaddr = pg_round_down (vaddr);
    page->owner = thread_current ();
    page->status = PAGE_ZERO;
    page->writable = writable;
    page->pinned = false;
    page->file = NULL;
    page->file_offset = 0;
    page->file_bytes = 0;
    page->zero_bytes = PGSIZE;
    page->swap_slot = BITMAP_ERROR;
    page->frame = NULL;
    
    struct hash_elem *e = hash_insert (&thread_current ()->pages, &page->hash_elem);
    if (e != NULL)
    {
        free (page);
        return NULL;
    }
    
    return page;
}

/* Look up a page for the given virtual address */
struct page *
page_lookup (void *vaddr)
{
    struct page p;
    struct hash_elem *e;
    
    p.vaddr = pg_round_down (vaddr);
    e = hash_find (&thread_current ()->pages, &p.hash_elem);
    
    return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Destroy a page entry */
void
page_destroy (struct page *page)
{
    hash_delete (&thread_current ()->pages, &page->hash_elem);
    
    if (page->status == PAGE_FRAME && page->frame != NULL)
        frame_free (page->frame);
    else if (page->status == PAGE_SWAP && page->swap_slot != BITMAP_ERROR)
        swap_free (page->swap_slot);
    
    free (page);
}

/* Load a page into memory */
bool
page_load (struct page *page)
{
    if (page->status == PAGE_FRAME)
        return true;
    
    void *frame = frame_alloc (page, PAL_USER);
    if (frame == NULL)
        return false;
    
    bool success = false;
    switch (page->status)
    {
        case PAGE_ZERO:
            memset (frame, 0, PGSIZE);
            success = true;
            break;
            
        case PAGE_FILE:
            if (page->file_bytes > 0)
            {
                acquire_filesys_lock ();
                file_seek (page->file, page->file_offset);
                if (file_read (page->file, frame, page->file_bytes) != (int) page->file_bytes)
                {
                    release_filesys_lock ();
                    frame_free (frame);
                    return false;
                }
                release_filesys_lock ();
            }
            memset (frame + page->file_bytes, 0, page->zero_bytes);
            success = true;
            break;
            
        case PAGE_SWAP:
            swap_in (page->swap_slot, frame);
            page->swap_slot = BITMAP_ERROR;
            success = true;
            break;
            
        default:
            PANIC ("Invalid page status");
    }
    
    if (!success)
    {
        frame_free (frame);
        return false;
    }
    
    if (!pagedir_set_page (thread_current ()->pagedir, page->vaddr, frame, page->writable))
    {
        frame_free (frame);
        return false;
    }
    
    page->status = PAGE_FRAME;
    page->frame = frame;
    
    return true;
}

/* Map a file into memory */
bool
page_map_file (void *vaddr, struct file *file, off_t offset,
                size_t read_bytes, size_t zero_bytes, bool writable)
{
    printf("page_map_file: vaddr=%p, read_bytes=%zu, zero_bytes=%zu\n", 
           vaddr, read_bytes, zero_bytes);

    ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT (pg_ofs (vaddr) == 0);
    ASSERT (offset % PGSIZE == 0);
    
    while (read_bytes > 0 || zero_bytes > 0)
    {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        
        struct page *page = page_create (vaddr, writable);
        if (page == NULL)
        {
            printf("page_map_file: page_create failed for %p\n", vaddr);
            return false;
        }
        
        page->status = PAGE_FILE;
        page->file = file;
        page->file_offset = offset;
        page->file_bytes = page_read_bytes;
        page->zero_bytes = page_zero_bytes;
        
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        vaddr += PGSIZE;
        offset += page_read_bytes;
    }

    printf("page_map_file: completed successfully\n");
    return true;
}

/* Map a zero page */
bool
page_map_zero (void *vaddr, bool writable)
{
    printf("page_map_zero: vaddr=%p, writable=%d\n", vaddr, writable);
    
    struct page *page = page_create (vaddr, writable);
    if (page == NULL)
    {
        printf("page_map_zero: page_create failed\n");
        return false;
    }
    
    page->status = PAGE_ZERO;
    printf("page_map_zero: success\n");
    return true;
}

/* Handle page fault */
bool
page_fault_handler (void *vaddr, bool write, void *esp)
{
    printf("page_fault_handler: vaddr=%p, write=%d, esp=%p\n", vaddr, write, esp);
  
    if (vaddr == NULL || !is_user_vaddr (vaddr))
    {
        printf("Invalid address: %p\n", vaddr);
        return false;
    }
    
    void *stack_ptr = esp;
    if (stack_ptr == NULL)
        stack_ptr = thread_current ()->esp;
    
    if (vaddr >= stack_ptr - 32 && vaddr < PHYS_BASE)
    {
        void *page_addr = pg_round_down (vaddr);
        struct page *page = page_lookup (page_addr);
        
        if (page == NULL)
        {
            if (!page_map_zero (page_addr, true))
                return false;
            page = page_lookup (page_addr);
        }
    }
    
    struct page *page = page_lookup (vaddr);
    if (page == NULL)
        return false;
    
    if (write && !page->writable)
        return false;
    
    return page_load (page);
}
