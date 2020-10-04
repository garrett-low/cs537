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

#define INPUT_LENGTH_MAX 128
#define GENERIC_ERROR "An error has occurred\n"

// debug preprocessor directives
#ifdef DEBUG
#define debugPrintf(...) printf(__VA_ARGS__)
#else
#define debugPrintf(...) ((void)0)
#endif

static int readCmd();
static void parseCmd();
static void handleExit();
static void handlePwd());
static void handleCd());

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

    char *cmdArg[128];
    bool hasOutputRedir = false;
    bool hasInputRedir = false;
    bool hasPipe = false;
    bool isBackground = false;
    char **cmdArgPostAnglePipe = NULL;
    parseCmd(cmd, cmdArg, cmdArgPostAnglePipe, &hasOutputRedir, &hasInputRedir, &hasPipe, &isBackground);

    // built-in commands
    if (strcmp(cmdArg[0], "exit") == 0)
    {
      handleExit();
    }
    else if (strcmp(cmdArg[0], "pwd") == 0)
    {
      char workingDir[PATH_MAX];
      if (getcwd(workingDir, PATH_MAX) == NULL)
      {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        continue;
      }
      printf("%s\n", workingDir);
    }
    else if (strcmp(cmdArg[0], "cd") == 0)
    {
      if (cmdArg[1] == NULL)
      {
        chdir(getenv("HOME"));
      }
      else
      {
        if (chdir(cmdArg[1]) == -1)
        {
          write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
          continue;
        }
      }
    }
    // non-built-in commands
    else
    {
      int pid = fork();

      // child
      if (pid == 0)
      {
        if (execvp(cmdArg[0], cmdArg) == -1)
        {
          write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
          exit(1);
        }
      }
      // parent
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

static void parseCmd(char *cmd, char **cmdArg, char **cmdArgPostAnglePipe,
                     bool *hasOutputRedir, bool *hasInputRedir, bool *hasPipe,
                     bool *isBackground)
{
  cmdArg[0] = strtok(cmd, " \t");
  *hasOutputRedir = false;
  *hasInputRedir = false;
  *hasPipe = false;
  *isBackground = false;

  int i = 1;
  int anglePipeArgIndex = -1;
  int ampersandArgIndex = -1;
  do
  {
    cmdArg[i] = strtok(NULL, " \t");
    if (cmdArg[i] == NULL)
    {
      break;
    }

    // assume redirection and pipeline never used together for this prj
    if (cmdArg[i][0] == '<' || cmdArg[i][0] == '>')
    {
      *hasOutputRedir = true;
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
  } while (cmdArg[i] != NULL);

  cmdArgPostAnglePipe = NULL;
  if (anglePipeArgIndex != -1)
  {
    cmdArg[anglePipeArgIndex] = NULL;
    cmdArgPostAnglePipe = &cmdArg[anglePipeArgIndex + 1];
  }

  debugPrintf("%s - has \">\"\n", *hasOutputRedir ? "true" : "false");
  debugPrintf("%s - has \"<\"\n", *hasInputRedir ? "true" : "false");
  debugPrintf("%s - has \"|\"\n", *hasPipe ? "true" : "false");
  debugPrintf("%s - has \"&\"\n", *isBackground ? "true" : "false");
  
  i = 0;
  while (cmdArg[i] != NULL)
  {
    debugPrintf("\"%s\" - arg%i\n", cmdArg[i], i);
    i++;
  }
  debugPrintf("\"%s\" - arg%i\n", cmdArg[i], i);


  if (cmdArgPostAnglePipe != NULL)
  {
    i = 0;
    while (cmdArgPostAnglePipe[i] != NULL)
    {
      debugPrintf("\"%s\" - arg%i\n", cmdArgPostAnglePipe[i], i);
      i++;
    }
    debugPrintf("\"%s\" - arg%i\n", cmdArgPostAnglePipe[i], i);
  }

  return;
}

static void handleExit() {
  exit(0);
}