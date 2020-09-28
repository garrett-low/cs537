#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include  <string.h>

int main(int argc, char* argv[]) {
    int x = 3;
    pid_t pid = fork();
    if (!pid) {
        ++x;
        printf("%d\n", x);
        exit(0);
    }
    --x;
    waitpid(pid, NULL, 0);
    ++x;
    printf("%d\n", x);
    exit(0);
}
