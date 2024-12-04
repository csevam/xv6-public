#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/x86.h"
#include "kernel/memlayout.h"
#include "kernel/mmu.h"
#include "kernel/proc.h"
#include "kernel/elf.h"

extern char data[]; // defined by kernel.ld
pde_t *kpgdir;      // for use in scheduler()
struct segdesc gdt[NSEGS];

void seginit(void) {
  struct cpu *c;

  c = &cpus[cpunum()];
  c->gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);

  c->gdt[SEG_KCPU] = SEG(STA_W, &c->cpu, 8, 0);

  lgdt(c->gdt, sizeof(c->gdt));
  loadgs(SEG_KCPU << 3);

  cpu = c;
  proc = 0;
}

static pte_t *walkpgdir(pde_t *pgdir, const void *va, int alloc) {
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if (*pde & PTE_P) {
    pgtab = (pte_t *)p2v(PTE_ADDR(*pde));
  } else {
    if (!alloc || (pgtab = (pte_t *)kalloc()) == 0)
      return 0;
    memset(pgtab, 0, PGSIZE);
    *pde = v2p(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

static int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm) {
  char *a, *last;
  pte_t *pte;

  a = (char *)PGROUNDDOWN((uint)va);
  last = (char *)PGROUNDDOWN(((uint)va) + size - 1);
  for (;;) {
    if ((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if (*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}


static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
    {(void *)KERNBASE, 0, EXTMEM, PTE_W},            // I/O space
    {(void *)KERNLINK, V2P(KERNLINK), V2P(data), 0}, // kern text+rodata
    {(void *)data, V2P(data), PHYSTOP, PTE_W},       // kern data+memory
    {(void *)DEVSPACE, DEVSPACE, 0, PTE_W},          // more devices
};

pde_t *setupkvm(void) {
  pde_t *pgdir;
  struct kmap *k;

  if ((pgdir = (pde_t *)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (p2v(PHYSTOP) > (void *)DEVSPACE)
    panic("PHYSTOP too high");
  for (k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if (mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                 (uint)k->phys_start, k->perm) < 0)
      return 0;
  return pgdir;
}

void kvmalloc(void) {
  kpgdir = setupkvm();
  switchkvm();
}

void switchkvm(void) {
  lcr3(v2p(kpgdir)); // switch to the kernel page table
}

void switchuvm(struct proc *p) {
  pushcli();
  cpu->gdt[SEG_TSS] = SEG16(STS_T32A, &cpu->ts, sizeof(cpu->ts) - 1, 0);
  cpu->gdt[SEG_TSS].s = 0;
  cpu->ts.ss0 = SEG_KDATA << 3;
  cpu->ts.esp0 = (uint)proc->kstack + KSTACKSIZE;
  ltr(SEG_TSS << 3);
  if (p->pgdir == 0)
    panic("switchuvm: no pgdir");
  lcr3(v2p(p->pgdir)); // switch to new address space
  popcli();
}

void inituvm(pde_t *pgdir, char *init, uint sz) {
  char *mem;

  if (sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, v2p(mem), PTE_W | PTE_U);
  memmove(mem, init, sz);
}

int loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz) {
  uint i, pa, n;
  pte_t *pte;

  if ((uint)addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for (i = 0; i < sz; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir, addr + i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if (sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if (readi(ip, p2v(pa), offset + i, n) != n)
      return -1;
  }
  return 0;
}

int allocuvm(pde_t *pgdir, uint oldsz, uint newsz) {
  char *mem;
  uint a;

  if (newsz >= KERNBASE)
    return 0;
  if (newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for (; a < newsz; a += PGSIZE) {
    mem = kalloc();
    if (mem == 0) {
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    mappages(pgdir, (char *)a, PGSIZE, v2p(mem), PTE_W | PTE_U);
  }
  return newsz;
}

int deallocuvm(pde_t *pgdir, uint oldsz, uint newsz) {
  pte_t *pte;
  uint a, pa;

  if (newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for (; a < oldsz; a += PGSIZE) {
    pte = walkpgdir(pgdir, (char *)a, 0);
    if (!pte)
      a += (NPTENTRIES - 1) * PGSIZE;
    else if ((*pte & PTE_P) != 0) {
      pa = PTE_ADDR(*pte);
      if (pa == 0)
        panic("kfree");
      char *v = p2v(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

void freevm(pde_t *pgdir) {
  uint i;

  if (pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for (i = 0; i < NPDENTRIES; i++) {
    if (pgdir[i] & PTE_P) {
      char *v = p2v(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char *)pgdir);
}

void clearpteu(pde_t *pgdir, char *uva) {
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if (pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

pde_t *copyuvm(pde_t *pgdir, uint sz) {
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if ((d = setupkvm()) == 0)
    return 0;
  for (i = 0; i < sz; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir, (void *)i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if (!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char *)p2v(pa), PGSIZE);
    if (mappages(d, (void *)i, PGSIZE, v2p(mem), flags) < 0)
      goto bad;
  }
  return d;

bad:
  freevm(d);
  return 0;
}

char *uva2ka(pde_t *pgdir, char *uva) {
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if ((*pte & PTE_P) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  return (char *)p2v(PTE_ADDR(*pte));
}

int copyout(pde_t *pgdir, uint va, void *p, uint len) {
  char *buf, *pa0;
  uint n, va0;

  buf = (char *)p;
  while (len > 0) {
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char *)va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if (n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

