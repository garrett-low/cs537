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
  int pri = getpri(getpid());

  check(pri == 3, "tester should be in priority 3");

  printf(1, "TEST PASSED");
  exit();
}
