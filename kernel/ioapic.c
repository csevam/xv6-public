
#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/traps.h"

#define IOAPIC 0xFEC00000 // Default physical address of IO APIC

#define REG_ID 0x00    // Register index: ID
#define REG_VER 0x01   // Register index: version
#define REG_TABLE 0x10 // Redirection table base

#define INT_DISABLED 0x00010000  // Interrupt disabled
#define INT_LEVEL 0x00008000     // Level-triggered (vs edge-)
#define INT_ACTIVELOW 0x00002000 // Active low (vs high)
#define INT_LOGICAL 0x00000800   // Destination is CPU id (vs APIC ID)

volatile struct ioapic *ioapic;

struct ioapic {
  uint reg;
  uint pad[3];
  uint data;
};

static uint ioapicread(int reg) {
  ioapic->reg = reg;
  return ioapic->data;
}

static void ioapicwrite(int reg, uint data) {
  ioapic->reg = reg;
  ioapic->data = data;
}

void ioapicinit(void) {
  int i, id, maxintr;

  if (!ismp)
    return;

  ioapic = (volatile struct ioapic *)IOAPIC;
  maxintr = (ioapicread(REG_VER) >> 16) & 0xFF;
  id = ioapicread(REG_ID) >> 24;
  if (id != ioapicid)
    cprintf("ioapicinit: id isn't equal to ioapicid; not a MP\n");

  for (i = 0; i <= maxintr; i++) {
    ioapicwrite(REG_TABLE + 2 * i, INT_DISABLED | (T_IRQ0 + i));
    ioapicwrite(REG_TABLE + 2 * i + 1, 0);
  }
}

void ioapicenable(int irq, int cpunum) {
  if (!ismp)
    return;

  ioapicwrite(REG_TABLE + 2 * irq, T_IRQ0 + irq);
  ioapicwrite(REG_TABLE + 2 * irq + 1, cpunum << 24);
}
