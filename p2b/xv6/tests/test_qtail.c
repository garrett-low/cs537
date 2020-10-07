#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"
#define check(exp, msg) if(exp) {} else {\
  printf(1, "%s:%d check (" #exp ") failed: %s\n", __FILE__, __LINE__, msg);\
  exit();}

void busywork() {
  int i = 0;
  for (;;)
    ++i;
}

int
main(int argc, char *argv[])
{
  int pid = fork();
  if (!pid) {
    busywork(); // child is in pri 3, so it should never be preempted
    exit();
  }

  struct pstat st;
  int i, j, k;
  for (i = 0; i < 10; ++i) { // repeat 10 times
    sleep(1);
    check(getpinfo(&st) == 0, "getpinfo");
    for (j = 0; j < NPROC; ++j) {
      if (st.inuse[j] && st.pid[j] == pid && st.priority[j] == 3)
        break;
    }
    check(j < NPROC, "cannot find child");
    // everytime the child moved to the tail, it must have run a fullslide
    // except the first time it was created (in fork())
    check(st.qtail[j][3] != 0 && st.ticks[j][3] == (st.qtail[j][3] - 1) * 8, "child qtail doesn't match ticks");
    printf(1, "child pid: %d priority: %d\n ", pid, st.priority[j]);
    for (k = 3; k >= 0; k--) {
      printf(1, "\t level %d ticks used %d\n", j, st.ticks[j][k]); 
      printf(1, "\t level %d times moved to tail %d\n", j, st.qtail[j][k]); 
    }
  }

  kill(pid);

  printf(1, "TEST PASSED");
  exit();
}
