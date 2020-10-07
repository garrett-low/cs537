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
  int pid1 = fork();
  if (!pid1) {
    busywork(); // child is in pri 3, so it should never be preempted
    exit();
  }

  int pid2 = fork();
  if (!pid2) {
    busywork();
    exit();
  }

  check(!setpri(pid1, 2), "setpri() returns nonzero code");
  check(getpri(pid1) == 2, "getpri() mismatch setpri()");
  // child 1 should never run on priority 2
  // because child 2 is always running

  struct pstat st;
  int i, j, k;
  for (i = 0; i < 10; ++i) { // repeat 10 times
    sleep(1);
    check(getpinfo(&st) == 0, "getpinfo");
    for (j = 0; j < NPROC; ++j) {
      if (st.inuse[j] && st.pid[j] == pid1 && st.priority[j] == 2)
        break;
    }
    check(j < NPROC, "cannot find child 1");
    for (k = 0; k < NPROC; ++k) {
      if (st.inuse[k] && st.pid[k] == pid2 && st.priority[k] == 3)
        break;
    }
    check(k < NPROC, "cannot find child 2");

    check(st.qtail[k][3] != 0 && st.ticks[k][3] == (st.qtail[k][3] - 1) * 8, "child 2 qtail doesn't match ticks");
    check(st.ticks[j][2] == 0, "child 1 should never run in level 2")
  }

  kill(pid1);
  kill(pid2);

  printf(1, "TEST PASSED");
  exit();
}
