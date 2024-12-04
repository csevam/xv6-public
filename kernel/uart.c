
#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/param.h"
#include "kernel/traps.h"
#include "kernel/spinlock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/mmu.h"
#include "kernel/proc.h"
#include "kernel/x86.h"

#define COM1 0x3f8

static int uart; // is there a uart?

void uartinit(void) {
  char *p;

  outb(COM1 + 2, 0);

  outb(COM1 + 3, 0x80); // Unlock divisor
  outb(COM1 + 0, 115200 / 9600);
  outb(COM1 + 1, 0);
  outb(COM1 + 3, 0x03); // Lock divisor, 8 data bits.
  outb(COM1 + 4, 0);
  outb(COM1 + 1, 0x01); // Enable receive interrupts.

  if (inb(COM1 + 5) == 0xFF)
    return;
  uart = 1;

  inb(COM1 + 2);
  inb(COM1 + 0);
  picenable(IRQ_COM1);
  ioapicenable(IRQ_COM1, 0);

  for (p = "xv6...\n"; *p; p++)
    uartputc(*p);
}

void uartputc(int c) {
  int i;

  if (!uart)
    return;
  for (i = 0; i < 128 && !(inb(COM1 + 5) & 0x20); i++)
    microdelay(10);
  outb(COM1 + 0, c);
}

static int uartgetc(void) {
  if (!uart)
    return -1;
  if (!(inb(COM1 + 5) & 0x01))
    return -1;
  return inb(COM1 + 0);
}

void uartintr(void) { consoleintr(uartgetc); }
