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

int readCmd();

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
    if (readCmd(&cmd) == -1)
    {
      continue;
    }

    char *arg0 = strtok(cmd, " \t");

    // built-in commands
    if (strcmp(arg0, "exit") == 0)
    {
      exit(0);
    }
    else if (strcmp(arg0, "pwd") == 0)
    {
      char workingDir[PATH_MAX];
      if (getcwd(workingDir, PATH_MAX) == NULL)
      {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        continue;
      }
      printf("%s\n", workingDir);
    }
    else if (strcmp(arg0, "cd") == 0)
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
    // not built-in commands
    else
    {
      char *cmdArg[128];
      cmdArg[0] = arg0;
      int i = 1;
      do
      {
        cmdArg[i] = strtok(NULL, " \t");
        i++;
      } while (cmdArg[i] != NULL);

      i = 0;
      while (cmdArg[i] != NULL)
      {
        debugPrintf("arg %i: %s.\n", i, cmdArg[i]);
        i++;
      }
      debugPrintf("arg %i: %s.\n", i, cmdArg[i]);

      int pid = fork();
      if (pid == 0)
      {
        // child
        if (execvp(cmdArg[0], cmdArg) == -1)
        {
          write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
          exit(1);
        }
      }
      else if (pid > 0)
      {
        waitpid(-1, NULL, 0);
      }
    }
  }
}

/**
 * readCmd
 * read from stdin a max of 128 chars
 * trims leading and trailing whitespace (space or tab)
 * 
 * Invalid input:
 *   - >128 characters - print an error message
 *   - all whitespace - don't print an error message
 * 
 * Returns:
 *   0 if valid input (<= 128 chars)
 *   -1 if error or invalid input
 */
int readCmd(char *cmd)
{
  if (fgets(cmd, sizeof(char) * INPUT_LENGTH_MAX, stdin) == NULL)
  {
    write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
    return -1;
  }

  // exceeds 128 bytes - print error
  if (strchr(cmd, '\n') == NULL)
  {
    char readChar;
    while ((readChar = fgetc(stdin)) != '\n' && readChar != EOF)
    {
    }
    write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
    return -1;
  }

  // only newline - not an error
  if (cmd[0] == '\n')
  {
    return -1;
  }

  // trim newline
  size_t cmdLen = strlen(cmd);
  if (cmdLen > 0 && cmd[cmdLen - 1] == '\n')
  {
    cmd[cmdLen - 1] = '\0';
    cmdLen--;
  }

  debugPrintf("\"%s\" - trimmed newline cmd\n", cmd);

  // trim leading whitespace (forward)
  for (int i = 0; i < cmdLen; i++)
  {
    if (cmd[i] == ' ' || cmd[i] == '\t')
    {
      cmd++;
      i--;
    }
    else // hit non-whitespace
    {
      break;
    }
  }

  // all whitespace - not an error
  if (strlen(cmd) == 0)
  {
    return -1;
  }

  // trim trailing whitespace (reverse)
  for (int i = strlen(cmd) - 1; i >= 0; i--)
  {
    if (cmd[i] == ' ' || cmd[i] == '\t')
    {
      cmd[i] = '\0';
    }
    else // hit non-whitespace
    {
      break;
    }
  }

  debugPrintf("\"%s\" - trimmed all whitespace cmd\n", cmd);

  return 0;
}