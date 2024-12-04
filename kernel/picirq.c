
#include "kernel/types.h"
#include "kernel/x86.h"
#include "kernel/traps.h"

#define IO_PIC1 0x20 // Master (IRQs 0-7)
#define IO_PIC2 0xA0 // Slave (IRQs 8-15)

#define IRQ_SLAVE 2 // IRQ at which slave connects to master

static ushort irqmask = 0xFFFF & ~(1 << IRQ_SLAVE);

static void picsetmask(ushort mask) {
  irqmask = mask;
  outb(IO_PIC1 + 1, mask);
  outb(IO_PIC2 + 1, mask >> 8);
}

void picenable(int irq) { picsetmask(irqmask & ~(1 << irq)); }

void picinit(void) {
  outb(IO_PIC1 + 1, 0xFF);
  outb(IO_PIC2 + 1, 0xFF);


  outb(IO_PIC1, 0x11);

  outb(IO_PIC1 + 1, T_IRQ0);

  outb(IO_PIC1 + 1, 1 << IRQ_SLAVE);

  outb(IO_PIC1 + 1, 0x3);

  outb(IO_PIC2, 0x11);           // ICW1
  outb(IO_PIC2 + 1, T_IRQ0 + 8); // ICW2
  outb(IO_PIC2 + 1, IRQ_SLAVE);  // ICW3
  outb(IO_PIC2 + 1, 0x3); // ICW4

  outb(IO_PIC1, 0x68); // clear specific mask
  outb(IO_PIC1, 0x0a); // read IRR by default

  outb(IO_PIC2, 0x68); // OCW3
  outb(IO_PIC2, 0x0a); // OCW3

  if (irqmask != 0xFFFF)
    picsetmask(irqmask);
}
