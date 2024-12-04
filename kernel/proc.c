#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/mmu.h"
#include "kernel/x86.h"
#include "kernel/proc.h"
#include "kernel/spinlock.h"
#include "kernel/trace.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void) { initlock(&ptable.lock, "ptable"); }

static struct proc *allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->traced = T_UNTRACE;
  release(&ptable.lock);

  if ((p->kstack = kalloc()) == 0) {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

void userinit(void) {
  struct proc *p;
  extern char _binary_user_initcode_start[], _binary_user_initcode_size[];

  p = allocproc();
  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_user_initcode_start, (int)_binary_user_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
}

int growproc(int n) {
  uint sz;

  sz = proc->sz;
  if (n > 0) {
    if ((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if (n < 0) {
    if ((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

int fork(void) {
  int i, pid;
  struct proc *np;

  if ((np = allocproc()) == 0)
    return -1;

  if ((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0) {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->traced = (proc->traced & T_ONFORK) ? proc->traced: T_UNTRACE;
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

void exit(void) {
  struct proc *p;
  int fd;

  if (proc == initproc)
    panic("init exiting");

  for (fd = 0; fd < NOFILE; fd++) {
    if (proc->ofile[fd]) {
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  wakeup1(proc->parent);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == proc) {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

int wait(void) {
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for (;;) {
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->parent != proc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE) {
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    if (!havekids || proc->killed) {
      release(&ptable.lock);
      return -1;
    }

    sleep(proc, &ptable.lock); // DOC: wait-sleep
  }
}

void scheduler(void) {
  struct proc *p;
  int foundproc = 1;

  for (;;) {
    sti();

    if (!foundproc)
      hlt();

    foundproc = 0;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->state != RUNNABLE)
        continue;

      foundproc = 1;
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      proc = 0;
    }
    release(&ptable.lock);
  }
}

void sched(void) {
  int intena;

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (cpu->ncli != 1)
    panic("sched locks");
  if (proc->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

void yield(void) {
  acquire(&ptable.lock); // DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

void forkret(void) {
  static int first = 1;
  release(&ptable.lock);

  if (first) {
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

}

void sleep(void *chan, struct spinlock *lk) {
  if (proc == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  if (lk != &ptable.lock) { // DOC: sleeplock0
    acquire(&ptable.lock);  // DOC: sleeplock1
    release(lk);
  }

  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  proc->chan = 0;

  if (lk != &ptable.lock) { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

static void wakeup1(void *chan) {
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

void wakeup(void *chan) {
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

int kill(int pid) {
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) {
      p->killed = 1;
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

void procdump(void) {
  static char *states[] = {
      [UNUSED] "unused",   [EMBRYO] "embryo",  [SLEEPING] "sleep ",
      [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING) {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
