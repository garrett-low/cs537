#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"
#define check(exp, msg) if(exp) {} else {\
  printf(1, "%s:%d check (" #exp ") failed: %s\n", __FILE__, __LINE__, msg);\
  exit();}

int
main(int argc, char *argv[])
{
  int pid = fork();
  if (!pid) {
    sleep(10);
    exit();
  }

  int pri = -1;
  check(!setpri(pid, 2), "setpri() returns nonzero code");
  pri = getpri(pid);
  check(pri == 2, "tester's child should be in priority 2");

  check(!setpri(pid, 2), "setpri() returns nonzero code");
  pri = getpri(pid);
  check(pri == 2, "tester's child should be in priority 1");

  check(!setpri(pid, 2), "setpri() returns nonzero code");
  pri = getpri(pid);
  check(pri == 2, "tester's child should be in priority 0");

  kill(pid);

  printf(1, "TEST PASSED");
  exit();
}
