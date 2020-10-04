/**
 * Course: CS537
 * Project: Project 2A - Shell
 * Name: Garrett Low
 * CSL Login: Low
 * Wisc Email: grlow@wisc.edu
 * Web Sources Used:
 *   too much for fgets: https://stackoverflow.com/a/30823277
 * Personal Help: N/A
 * Description:
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/limits.h>

#define INPUT_LENGTH_MAX 128
#define GENERIC_ERROR "An error has occurred\n"

// debug preprocessor directives
#ifdef DEBUG
#define debugPrintf(...) printf(__VA_ARGS__)
#else
#define debugPrintf(...) ((void)0)
#endif

int main(int argc, char *argv[])
{
  if (argc > 1)
  {
    write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
    exit(1);
  }

  int historyLine = 0;
  while (1)
  {
    historyLine++;
    printf("wish (%i)> ", historyLine);
    fflush(stdout);

    char cmd[INPUT_LENGTH_MAX];
    if (fgets(cmd, sizeof(char) * INPUT_LENGTH_MAX, stdin) == NULL)
    {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      continue;
    }

    // exceeds 128 bytes
    if (strchr(cmd, '\n') == NULL)
    {
      char readChar;
      while ((readChar = fgetc(stdin)) != '\n' && readChar != EOF) {
        
      }
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      continue;
    }

    // only newline
    if (cmd[0] == '\n')
    {
      continue;
    }

    // trim newline
    size_t cmdLen = strlen(cmd);
    if (cmdLen > 0 && cmd[cmdLen - 1] == '\n')
    {
      cmd[cmdLen - 1] = '\0';
    }

    // backup cmd
    char *cmdCpy = strdup(cmd);
    if (cmdCpy == NULL)
    {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      continue;
    }

    char *arg1 = strtok(cmdCpy, " \t");
    debugPrintf("arg1: %s.\n", arg1);

    // only whitespace - not an error
    if (arg1 == NULL)
    {
      continue;
    }

    if (strcmp(arg1, "exit") == 0)
    {
      exit(0);
    }
    else if (strcmp(arg1, "pwd") == 0)
    {
      char workingDir[PATH_MAX];
      if (getcwd(workingDir, PATH_MAX) == NULL)
      {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        continue;
      }
      printf("%s\n", workingDir);
    }
    else if (strcmp(arg1, "cd") == 0)
    {
      char *arg2 = strtok(NULL, " \t");
      debugPrintf("arg2: %s.\n", arg2);
      if (arg2 == NULL)
      {
        chdir(getenv("HOME"));
      }
      else
      {
        if (chdir(arg2) == -1)
        {
          write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
          continue;
        }
      }
    }

    // char *cmdArg[128];
    // int i = 0;
    // cmdArg[i] = strtok(cmdCpy, " \t");
    // while (cmdArg[i] != NULL)
    // {
    //   cmdArg[i] = strtok(NULL, " \t");
    //   debugPrintf("arg %i: %s.\n", i, cmdArg[i]);
    // }
  }
}