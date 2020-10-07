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

  check(setpri(-pid, 2) == -1, "setpri() should check invalid argumet");

  check(setpri(pid, 10) == -1, "setpri() should check invalid argumet");

  check(getpri(-pid) == -1, "getpri() should check invalid argumet");

  check(getpinfo(NULL) == -1, "getpinfo() should check invalid argumet");

  kill(pid);
  printf(1, "TEST PASSED");
  exit();
}
