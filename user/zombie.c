#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main(void) {
  if (fork() > 0)
    sleep(5);
  exit();
}
