#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"

int sys_getwritecount(void) {
  return proc->writecount;
}