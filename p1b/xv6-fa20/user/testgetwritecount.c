#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"

char buf[2048];
int stdout = 1;

void basic(void) {
  printf(1, "test\n");
  int writeCount = getwritecount();
  printf(stdout, "after printing 'test\\n', getwritecount returned %d!\n", writeCount);
}

int main(int argc, char *argv[]) {
  basic();
  
  exit();
}