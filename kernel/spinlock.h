struct spinlock {
  uint locked;       // Is the lock held?
  
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
};

