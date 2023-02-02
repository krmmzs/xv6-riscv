// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
// -----------------------------------------------------
// The implementation of allocator is a nice example.
// The allocator sometimes treats addresses as integers in order to perform arithmetic on them
// (e.g., traversing all pages infreerange), and sometimes uses addresses as pointers to read and
// write memory (e.g., manipulating therunstructure stored in each page); this dual use of addresses
// is the main reason that the allocator code is full of C type casts. The other reason is that freeing
// and allocation inherently change the type of the memory.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// Act a node in the data structure
// The data structure is a linked list
// Each node is a page of memory
// (content is alloced by memset, so without content field)
struct run {
    struct run *next;
};

// Act as a head node in the data structure
struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;

// kinitinitializesthe free list to hold every page
// between the end of the kernel(end[]) and PHYSTOP
void
kinit()
{
    initlock(&kmem.lock, "kmem");
    freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char*)PGROUNDUP((uint64)pa_start);
    for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
// ---------------------------------------------
// Head insertion method in the linked list
void
kfree(void *pa)
{
    struct run *r;

    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    // hopefully that will cause such code to break faster
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// ---------------------------------------------
// Head removal method in the linked list
void *
kalloc(void)
{
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if(r)
        kmem.freelist = r->next;
    release(&kmem.lock);

    if(r)
        memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
}
