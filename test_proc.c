#include "defs.h"
#include "proc.h"

// Function to test direct access to proc
void test_proc_access(void) {
    cprintf("DEBUG: Testing proc access...\n");

    if (proc == 0) {
        cprintf("DEBUG: proc is NULL. Cannot access directly.\n");
        return;
    }

    cprintf("DEBUG: proc address = %p\n", proc);
    cprintf("DEBUG: proc->pid = %d\n", proc->pid);
    cprintf("DEBUG: proc->name = %s\n", proc->name);
}
