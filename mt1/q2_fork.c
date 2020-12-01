#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int value = 5;

int main(int argc, char* argv[]) {
  pid_t pid;
  pid = fork();
  if (pid == 0) {
    value = value + 15;
  } else if (pid > 0) {
    wait(NULL);
    printf("PARENT : value = %d\n", value);  // LINE A
    exit(0);
  }
}