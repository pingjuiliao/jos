// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
    void *addr = (void *) utf->utf_fault_va;
    uint32_t err = utf->utf_err;
    int r;

    // Check that the faulting access was (1) a write, and (2) to a
    // copy-on-write page.  If not, panic.
    // Hint:
    //   Use the read-only page table mappings at uvpt
    //   (see <inc/memlayout.h>).

    // LAB 4: Your code here.
    envid_t eid = sys_getenvid();
    eid = 0;
    pte_t pte = uvpt[PGNUM(addr)] ;
#ifdef DEBUG
    cprintf("pgfault (lib/fork.c): err code == 0x%08x\n", err);
    cprintf("                      fault_va == %p\n", addr);
    cprintf("                      pte      == %p\n", pte );
#endif

    if ( !( err & FEC_WR ) ) {
        panic("pgfault (lib/fork.c): the fault is not a write");
    }
    if ( !( pte & PTE_COW ) ) {
        panic("pgfault (lib/fork.c): the fault is not a copy-on-write");
    }
    // Allocate a new page, map it at a temporary location (PFTEMP),
    // copy the data from the old page to the new page, then move the new
    // page to the old page's address.
    // Hint:
    //   You should make three system calls.

    // LAB 4: Your code here.
    sys_page_alloc(eid, PFTEMP, PTE_U | PTE_W | PTE_P );
    // bad alternative : sys_page_alloc(eid, PFTEMP, PGOFF(pte) | PTE_W ); << this will crash ....
    memmove(PFTEMP, (void *) PTE_ADDR(addr), PGSIZE);
    sys_page_map(eid, (void *) PFTEMP, eid, (void *) PTE_ADDR(addr) , PTE_U | PTE_W | PTE_P );
    // bad alternative : sys_page_map(eid, (void *) PFTEMP, eid, (void *) PTE_ADDR(addr) , PGOFF(pte) | PTE_P ); << this will crash
    sys_page_unmap(eid, PFTEMP);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
    int r;

    // LAB 4: Your code here.
    void* va = (void *) (pn * PGSIZE);
    envid_t parent = thisenv->env_id;
    pte_t pte = uvpt[pn];
    if ( pte & PTE_SHARE ) {
        if ( ( r = sys_page_map(parent, va, envid, va, pte & PTE_SYSCALL) ) < 0 ) {
            return r ;
        }
    } else if ( ( pte & PTE_W ) || ( pte & PTE_COW ) ) {
        if ( ( r = sys_page_map(parent, va, envid, va, PTE_COW | PTE_U | PTE_P )) < 0 ) {
            return r;
        }
        if ( ( r = sys_page_map(envid, va, parent, va, PTE_COW | PTE_U | PTE_P)) < 0 ) {
            return r;
        }
    } else {
        if ( ( r = sys_page_map(parent, va, envid, va, PTE_SYSCALL) ) < 0 ) {
            return r;
        }
    }
    return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a envid.
// Copy our address space and page fault handler setup to the envid.
// Then mark the envid as runnable and return.
//
// Returns: envid's envid to the parent, 0 to the envid, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the envid process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the envid's user exception stack.
//
envid_t
fork(void)
{

    // LAB 4: Your code here.

    envid_t envid;
    // pde_t *pt ;
    pte_t pde ;
    pte_t pte ;
    uint32_t perm, i ;
    uintptr_t va, base_va ;
    int r ;
    set_pgfault_handler(&pgfault);
    envid = sys_exofork() ;

    // child
    if ( envid < 0 ) {
        panic("sys_exofork: %e", envid);
    } else if ( envid == 0 ) {
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0 ;
    }

    // parent
    for ( base_va = UTEXT  ; base_va < UTOP ; base_va += PTSIZE ) { // += 4MB
        pde = (pde_t) uvpd[PDX( base_va )];
        if ( pde & PTE_P ) {
            for ( i = 0; i < 1024; ++i ) {
                va = base_va + i * PGSIZE ;
                if ( va >= UXSTACKTOP - PGSIZE )
                    break ;
                pte = (pte_t) uvpt[PGNUM(va)];
                perm = PGOFF( (uintptr_t) pde & (uintptr_t) pte ) ;
                if ( pte & PTE_P ) {
                    if ( ( r = duppage(envid, PGNUM(va))) < 0 ) {
                        panic("duppage failed\n");
                    }
                }
            }
        }
    }
    // map User Exception stack
    sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_U | PTE_W | PTE_P );
    sys_page_map(envid, (void *) (UXSTACKTOP - PGSIZE), thisenv->env_id, PFTEMP, PTE_U | PTE_W | PTE_P);
    memmove(PFTEMP, (void *) (UXSTACKTOP - PGSIZE), PGSIZE) ;
    sys_page_unmap(thisenv->env_id, PFTEMP);

    sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);
    sys_env_set_status(envid, ENV_RUNNABLE);



    return envid ;
}

// Challenge!
    int
sfork(void)
{
    panic("sfork not implemented");
    return -E_INVAL;
}
