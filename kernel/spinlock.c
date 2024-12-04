
#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/param.h"
#include "kernel/x86.h"
#include "kernel/memlayout.h"
#include "kernel/mmu.h"
#include "kernel/proc.h"
#include "kernel/spinlock.h"

void initlock(struct spinlock *lk, char *name) {
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

void acquire(struct spinlock *lk) {
  pushcli(); // disable interrupts to avoid deadlock.
  if (holding(lk))
    panic("acquire");
  while (xchg(&lk->locked, 1) != 0)
    ;
  lk->cpu = cpu;
  getcallerpcs(&lk, lk->pcs);
}

void release(struct spinlock *lk) {
  if (!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;

  xchg(&lk->locked, 0);

  popcli();
}

void getcallerpcs(void *v, uint pcs[]) {
  uint *ebp;
  int i;

  ebp = (uint *)v - 2;
  for (i = 0; i < 10; i++) {
    if (ebp == 0 || ebp < (uint *)KERNBASE || ebp == (uint *)0xffffffff)
      break;
    pcs[i] = ebp[1];      // saved %eip
    ebp = (uint *)ebp[0]; // saved %ebp
  }
  for (; i < 10; i++)
    pcs[i] = 0;
}

int holding(struct spinlock *lock) { return lock->locked && lock->cpu == cpu; }


void pushcli(void) {
  int eflags;

  eflags = readeflags();
  cli();
  if (cpu->ncli++ == 0)
    cpu->intena = eflags & FL_IF;
}

void popcli(void) {
  if (readeflags() & FL_IF)
    panic("popcli - interruptible");
  if (--cpu->ncli < 0)
    panic("popcli");
  if (cpu->ncli == 0 && cpu->intena)
    sti();
}
