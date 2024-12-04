
#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/mmu.h"
#include "kernel/proc.h"
#include "kernel/x86.h"
#include "kernel/traps.h"
#include "kernel/spinlock.h"
#include "kernel/fs.h"
#include "buf.h"

#define SECTOR_SIZE 512
#define IDE_BSY 0x80
#define IDE_DRDY 0x40
#define IDE_DF 0x20
#define IDE_ERR 0x01

#define IDE_CMD_READ 0x20
#define IDE_CMD_WRITE 0x30


static struct spinlock idelock;
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf *);

static int idewait(int checkerr) {
  int r;

  while (((r = inb(0x1f7)) & (IDE_BSY | IDE_DRDY)) != IDE_DRDY)
    ;
  if (checkerr && (r & (IDE_DF | IDE_ERR)) != 0)
    return -1;
  return 0;
}

void ideinit(void) {
  int i;

  initlock(&idelock, "ide");
  picenable(IRQ_IDE);
  ioapicenable(IRQ_IDE, ncpu - 1);
  idewait(0);

  outb(0x1f6, 0xe0 | (1 << 4));
  for (i = 0; i < 1000; i++) {
    if (inb(0x1f7) != 0) {
      havedisk1 = 1;
      break;
    }
  }

  outb(0x1f6, 0xe0 | (0 << 4));
}

static void idestart(struct buf *b) {
  if (b == 0)
    panic("idestart");
  if (b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block = BSIZE / SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;

  if (sector_per_block > 7)
    panic("idestart");

  idewait(0);
  outb(0x3f6, 0);                // generate interrupt
  outb(0x1f2, sector_per_block); // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((b->dev & 1) << 4) | ((sector >> 24) & 0x0f));
  if (b->flags & B_DIRTY) {
    outb(0x1f7, IDE_CMD_WRITE);
    outsl(0x1f0, b->data, BSIZE / 4);
  } else {
    outb(0x1f7, IDE_CMD_READ);
  }
}

void ideintr(void) {
  struct buf *b;

  acquire(&idelock);
  if ((b = idequeue) == 0) {
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  if (!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE / 4);

  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  if (idequeue != 0)
    idestart(idequeue);

  release(&idelock);
}

void iderw(struct buf *b) {
  struct buf **pp;

  if (!(b->flags & B_BUSY))
    panic("iderw: buf not busy");
  if ((b->flags & (B_VALID | B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");
  if (b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  acquire(&idelock); // DOC:acquire-lock

  b->qnext = 0;
  for (pp = &idequeue; *pp; pp = &(*pp)->qnext) // DOC:insert-queue
    ;
  *pp = b;

  if (idequeue == b)
    idestart(b);

  while ((b->flags & (B_VALID | B_DIRTY)) != B_VALID) {
    sleep(b, &idelock);
  }

  release(&idelock);
}
