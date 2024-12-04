
#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/param.h"
#include "kernel/spinlock.h"
#include "kernel/fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf head;
} bcache;

void binit(void) {
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    b->dev = -1;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;

  acquire(&bcache.lock);

loop:
  for (b = bcache.head.next; b != &bcache.head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      if (!(b->flags & B_BUSY)) {
        b->flags |= B_BUSY;
        release(&bcache.lock);
        return b;
      }
      sleep(b, &bcache.lock);
      goto loop;
    }
  }

  for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
    if ((b->flags & B_BUSY) == 0 && (b->flags & B_DIRTY) == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->flags = B_BUSY;
      release(&bcache.lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!(b->flags & B_VALID)) {
    iderw(b);
  }
  return b;
}

void bwrite(struct buf *b) {
  if ((b->flags & B_BUSY) == 0)
    panic("bwrite");
  b->flags |= B_DIRTY;
  iderw(b);
}

void brelse(struct buf *b) {
  if ((b->flags & B_BUSY) == 0)
    panic("brelse");

  acquire(&bcache.lock);

  b->next->prev = b->prev;
  b->prev->next = b->next;
  b->next = bcache.head.next;
  b->prev = &bcache.head;
  bcache.head.next->prev = b;
  bcache.head.next = b;

  b->flags &= ~B_BUSY;
  wakeup(b);

  release(&bcache.lock);
}
