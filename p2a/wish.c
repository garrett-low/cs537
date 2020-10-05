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
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INPUT_LENGTH_MAX 128
#define GENERIC_ERROR "An error has occurred\n"
#define MAX_BACKGROUND_PROC 20

// debug preprocessor directives
#ifdef DEBUG
#define debugPrintf(...) printf(__VA_ARGS__)
#else
#define debugPrintf(...) ((void)0)
#endif

// typedef struct pidTableStruct
// {
//   int pid[MAX_BACKGROUND_PROC];
//   int head;
//   int tail;
//   int size;
// } pidTable;

static int readCmd();
static int parseCmd(char *cmd, char **cmdArg, char **cmdArgPostAnglePipe,
                    bool *hasOutputRedir, bool *hasInputRedir, bool *hasPipe,
                    bool *isBackground);
static void handleExit(int* bgPids);
static void handlePwd();
static void handleCd(char *path);
static void errorAndEndProcess();
// static char isEmpty(pidTable bgPids);
// static void enqueue();
// static void dequeue();

// debug
static void debugParse(char **cmdArg, char **cmdArgPostAnglePipe,
                       bool hasOutputRedir, bool hasInputRedir, bool hasPipe,
                       bool isBackground);

int main(int argc, char *argv[])
{
  if (argc > 1)
  {
    errorAndEndProcess();
  }

  int historyLine = 0;
  int bgPids[MAX_BACKGROUND_PROC] = {-1};
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

    char *cmdArg[INPUT_LENGTH_MAX];
    bool hasOutputRedir = false;
    bool hasInputRedir = false;
    bool hasPipe = false;
    bool isBackground = false;
    char *cmdArgPostAnglePipe[INPUT_LENGTH_MAX];
    if (parseCmd(cmd, cmdArg, cmdArgPostAnglePipe, &hasOutputRedir,
                 &hasInputRedir, &hasPipe, &isBackground) == -1)
    {
      continue;
    }

    // built-in commands
    if (strcmp(cmdArg[0], "exit") == 0)
    {
      handleExit(bgPids);
    }
    else if (strcmp(cmdArg[0], "pwd") == 0)
    {
      handlePwd();
    }
    else if (strcmp(cmdArg[0], "cd") == 0)
    {
      handleCd(cmdArg[1]);
    }
    // non-built-in commands
    else
    {
      int pid = fork();

      // child
      if (pid == 0)
      {
        if (hasPipe)
        {
          int pipes[2], pidPipe;
          if (pipe(pipes) == -1)
          {
            debugPrintf("1\n");
            errorAndEndProcess();
          }

          pidPipe = fork();
          if (pidPipe == 0)
          {
            if (close(pipes[0]) == -1)
            {
              debugPrintf("2\n");
              errorAndEndProcess();
            }

            if (dup2(pipes[1], STDOUT_FILENO) == -1)
            {
              debugPrintf("3\n");
              errorAndEndProcess();
            }

            if (execvp(cmdArg[0], cmdArg) == -1)
            {
              debugPrintf("4\n");
              errorAndEndProcess();
            }
          }
          pidPipe = fork();
          if (pidPipe == 0)
          {
            if (close(pipes[1]) == -1)
            {
              debugPrintf("5\n");
              errorAndEndProcess();
            }

            if (dup2(pipes[0], STDIN_FILENO) == -1)
            {
              debugPrintf("6\n");
              errorAndEndProcess();
            }

            if (execvp(cmdArgPostAnglePipe[0], cmdArgPostAnglePipe) == -1)
            {
              debugPrintf("7\n");
              errorAndEndProcess();
            }
          }
          else
          {
            waitpid(-1, NULL, 0);
            exit(0);
          }
        }
        else
        {
          if (hasOutputRedir)
          {
            char *path = cmdArgPostAnglePipe[0];

            int retVal;
            if ((retVal =
                     open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU)) == -1)
              errorAndEndProcess();

            if (dup2(retVal, 1) == -1)
              errorAndEndProcess();
          }
          else if (hasInputRedir)
          {
            char *path = cmdArgPostAnglePipe[0];

            int retVal;
            if ((retVal = open(path, O_RDONLY)) == -1)
              errorAndEndProcess();

            if (dup2(retVal, STDOUT_FILENO) == -1)
              errorAndEndProcess();
          }

          if (execvp(cmdArg[0], cmdArg) == -1)
          {
            errorAndEndProcess();
          }
        }
      }
      // parent
      else if (pid > 0)
      {
        if (!isBackground)
        {
          waitpid(pid, NULL, 0);
        }
        else
        {
          for (int i = 0; i < MAX_BACKGROUND_PROC; i++)
          {
            if (bgPids[i] == -1)
            {
              bgPids[i] = pid;
            }
          }

          for (int i = 0; i < MAX_BACKGROUND_PROC; i++)
          {
            if (bgPids[i] != -1)
            {
              if (waitpid(bgPids[i], NULL, WNOHANG) == bgPids[i])
              {
                bgPids[i] = -1;
              }
            }
          }
        }
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
 * Parameters:
 *   cmd - output - trimmed user input
 * 
 * Returns:
 *   0 if valid input (<= 128 chars)
 *   -1 if error or invalid input
 */
static int readCmd(char *cmd)
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

  return 0;
}

static int parseCmd(char *cmd, char **cmdArg, char **cmdArgPostAnglePipe,
                    bool *hasOutputRedir, bool *hasInputRedir, bool *hasPipe,
                    bool *isBackground)
{
  char *savePtr;

  cmdArg[0] = strtok_r(cmd, " \t", &savePtr);
  *hasOutputRedir = false;
  *hasInputRedir = false;
  *hasPipe = false;
  *isBackground = false;

  int i = 1;
  int anglePipeArgIndex = -1;
  int ampersandArgIndex = -1;
  while ((cmdArg[i] = strtok_r(NULL, " \t", &savePtr)) != NULL)
  {
    if (cmdArg[i] == NULL)
      continue;

    // assume redirection and pipeline never used together for this prj
    if (cmdArg[i][0] == '>')
    {
      *hasOutputRedir = true;
      anglePipeArgIndex = i;
    }
    else if (cmdArg[i][0] == '<')
    {
      *hasInputRedir = true;
      anglePipeArgIndex = i;
    }
    else if (cmdArg[i][0] == '|')
    {
      *hasPipe = true;
      anglePipeArgIndex = i;
    }

    // assume redirection can be used with bg execution for this prj
    // assume pipeline cannot be used with bg execution for this prj
    if (cmdArg[i][0] == '&')
    {
      *isBackground = true; // redundant because we have ampersandArgIndex
      ampersandArgIndex = i;
    }

    i++;
  }

  if (anglePipeArgIndex != -1)
  {
    cmdArg[anglePipeArgIndex] = NULL;

    for (i = 0; cmdArg[anglePipeArgIndex + i + 1] != NULL; i++)
    {
      cmdArgPostAnglePipe[i] = cmdArg[anglePipeArgIndex + i + 1];
    }
    cmdArgPostAnglePipe[i] = NULL; // set last argv to NULL

    // we expect exactly one word after redirector
    if ((*hasOutputRedir || *hasInputRedir) &&
        (cmdArgPostAnglePipe[0] == NULL || cmdArgPostAnglePipe[1] != NULL))
    {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return -1;
    }
  }

  return 0;
}

static void handleExit(int* bgPids)
{
  for (int i = 0; i < MAX_BACKGROUND_PROC; i++)
  {
    if (bgPids[i] != -1)
    {
      kill(bgPids[i]);
    }
  }
  exit(0);
}

static void handlePwd()
{
  char workingDir[PATH_MAX];
  if (getcwd(workingDir, PATH_MAX) == NULL)
  {
    write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
    return;
  }
  printf("%s\n", workingDir);
  return;
}

static void handleCd(char *path)
{
  if (path == NULL)
  {
    chdir(getenv("HOME"));
  }
  else
  {
    if (chdir(path) == -1)
    {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return;
    }
  }

  return;
}

static void errorAndEndProcess()
{
  write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
  exit(1);
}

static void debugParse(char **cmdArg, char **cmdArgPostAnglePipe,
                       bool hasOutputRedir, bool hasInputRedir, bool hasPipe,
                       bool isBackground)
{
  debugPrintf("%s - \">\" output redir\n", hasOutputRedir ? "true" : "false");
  debugPrintf("%s - \"<\" input redir\n", hasInputRedir ? "true" : "false");
  debugPrintf("%s - \"|\" pipeline\n", hasPipe ? "true" : "false");
  debugPrintf("%s - \"&\" bg exec\n", isBackground ? "true" : "false");

  int i = 0;
  while (cmdArg[i] != NULL)
  {
    debugPrintf("\"%s\" [%p] - arg%i\n", cmdArg[i], &cmdArg[i], i);
    i++;
  }
  debugPrintf("\"%s\" [%p] - arg%i\n", cmdArg[i], &cmdArg[i], i);

  if (hasOutputRedir || hasInputRedir || hasPipe)
  {
    i = 0;
    while (cmdArgPostAnglePipe[i] != NULL)
    {
      debugPrintf("\"%s\" [%p] - arg%i\n", cmdArgPostAnglePipe[i], &cmdArgPostAnglePipe[i], i);
      i++;
    }
    debugPrintf("\"%s\" [%p] - arg%i\n", cmdArgPostAnglePipe[i], &cmdArgPostAnglePipe[i], i);
  }
}

// /**
//  * Returns 1 if the set is currently empty, 0 otherwise
//  */
// char isEmpty(pidTable bgPids)
// {
//   return (bgPids.head == -1) || (bgPids.tail == -1);
// }

// /**
//  * Add to the back of the queue by inserting after tail node
//  */
// void enqueue(pidTable bgPids, int pid)
// {
//   if (bgPids.size >= MAX_BACKGROUND_PROC)
//   {
//     write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
//     return;
//   }

//   if (isEmpty(bgPids))
//   { // set is currently empty; head and tail same
//     bgPids.tail++;
//     bgPids.head++;
//     // debugPrintSet(set, 0);
//   }
//   else if (bgPids.tail + 1 < MAX_BACKGROUND_PROC)
//   { // do not wrap around
//     bgPids.tail++;
//   }
//   else
//   { // wrap around
//     bgPids.tail = 0;
//   }

//   bgPids.pid[bgPids.tail] = pid;
//   bgPids.size++;
// }

// /**
//  * Remove from the front of the queue by removing the head node
//  */
// void dequeue(pidTable bgPids)
// {
//   if (isEmpty(bgPids))
//   {
//     write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
//     return;
//   }

//   bgPids.pid[bgPids.head] = 0;

//   if (bgPids.head == bgPids.tail)
//   { // make queue empty
//     bgPids.head = -1;
//     bgPids.tail = -1;
//   }
//   else if (bgPids.head + 1 < MAX_BACKGROUND_PROC)
//   { // do not wrap around
//     bgPids.head++;
//   }
//   else
//   { // wrap around
//     bgPids.head = 0;
//   }

//   bgPids.size--;
// }