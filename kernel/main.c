#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/mmu.h"
#include "kernel/proc.h"
#include "kernel/x86.h"

static void startothers(void);
static void mpmain(void) __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // first address after kernel loaded from ELF file

int main(void) {
  kinit1(end, P2V(4 * 1024 * 1024)); // phys page allocator
  kvmalloc();                        // kernel page table
  mpinit();                          // collect info about this machine
  lapicinit();
  seginit(); // set up segments
  cprintf("\ncpu%d: starting xv6\n\n", cpu->id);
  picinit();     // interrupt controller
  ioapicinit();  // another interrupt controller
  consoleinit(); // I/O devices & their interrupts
  uartinit();    // serial port
  pinit();       // process table
  tvinit();      // trap vectors
  binit();       // buffer cache
  fileinit();    // file table
  ideinit();     // disk
  if (!ismp)
    timerinit();                              // uniprocessor timer
  startothers();                              // start other processors
  kinit2(P2V(4 * 1024 * 1024), P2V(PHYSTOP)); // must come after startothers()
  userinit();                                 // first user process
  mpmain();
}

static void mpenter(void) {
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

static void mpmain(void) {
  cprintf("cpu%d: starting\n", cpu->id);
  idtinit();              // load idt register
  xchg(&cpu->started, 1); // tell startothers() we're up
  scheduler();            // start running processes
}

pde_t entrypgdir[]; // For entry.S

static void startothers(void) {
  extern uchar _binary_kernel_entryother_start[], _binary_kernel_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  code = p2v(0x7000);
  memmove(code, _binary_kernel_entryother_start, (uint)_binary_kernel_entryother_size);

  for (c = cpus; c < cpus + ncpu; c++) {
    if (c == cpus + cpunum()) // We've started already.
      continue;

    stack = kalloc();
    *(void **)(code - 4) = stack + KSTACKSIZE;
    *(void **)(code - 8) = mpenter;
    *(int **)(code - 12) = (void *)v2p(entrypgdir);

    lapicstartap(c->id, v2p(code));

    while (c->started == 0)
      ;
  }
}

__attribute__((__aligned__(PGSIZE))) pde_t entrypgdir[NPDENTRIES] = {
    [0] = (0) | PTE_P | PTE_W | PTE_PS,
    [KERNBASE >> PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

