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
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUT_LENGTH_MAX 128
#define GENERIC_ERROR "An error has occurred\n"
#define MAX_BACKGROUND_PROC 20

// debug preprocessor directives
#ifdef DEBUG
#define debugPrintf(...) printf(__VA_ARGS__)
#else
#define debugPrintf(...) ((void)0)
#endif

static int readCmd(char *cmd, int *historyLine);
static int parseCmd(char *cmd, char **cmdArg, char **cmdArgIn, char **cmdArgOut,
                    char **cmdArgPipe, bool *hasOut, bool *hasIn, bool *hasPipe,
                    bool *isBackground);
static void handleExit(int *bgPids);
static void handlePwd();
static void handleCd(char *path);
static void errorAndEndProcess();

// debug
static void debugParse(char **cmdArg, char **cmdArg2, bool hasOut, bool hasIn,
                       bool hasPipe, bool isBackground);

int main(int argc, char *argv[]) {
  if (argc > 1) {
    errorAndEndProcess();
  }

  int historyLine = 0;
  int bgPids[MAX_BACKGROUND_PROC];
  for (int i = 0; i < MAX_BACKGROUND_PROC; i++) {
    bgPids[i] = -1;
  }

  while (1) {
    historyLine++;
    printf("wish (%i)> ", historyLine);
    fflush(stdout);

    char cmd[INPUT_LENGTH_MAX];
    if (readCmd(cmd, &historyLine) == -1) {
      continue;
    }

    char *cmdArg[INPUT_LENGTH_MAX];
    bool hasOut = false;
    bool hasIn = false;
    bool hasPipe = false;
    bool isBackground = false;
    char *cmdArgIn[INPUT_LENGTH_MAX];
    char *cmdArgOut[INPUT_LENGTH_MAX];
    char *cmdArgPipe[INPUT_LENGTH_MAX];
    if (parseCmd(cmd, cmdArg, cmdArgIn, cmdArgOut, cmdArgPipe, &hasOut, &hasIn,
                 &hasPipe, &isBackground) == -1) {
      continue;
    }

    // built-in commands
    if (strcmp(cmdArg[0], "exit") == 0) {
      handleExit(bgPids);
    } else if (strcmp(cmdArg[0], "pwd") == 0) {
      if (cmdArg[1] != NULL) {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        continue;
      }
      handlePwd();
    } else if (strcmp(cmdArg[0], "cd") == 0) {
      if (cmdArg[2] != NULL) {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        continue;
      }
      handleCd(cmdArg[1]);
    }
    // non-built-in commands
    else {
      int pid = fork();

      // child
      if (pid == 0) {
        if (hasPipe) {
          int pipes[2], pidPipe;
          if (pipe(pipes) == -1) {
            errorAndEndProcess();
          }

          pidPipe = fork();
          if (pidPipe == 0) {
            if (close(pipes[0]) == -1) {
              errorAndEndProcess();
            }

            if (dup2(pipes[1], STDOUT_FILENO) == -1) {
              errorAndEndProcess();
            }

            if (execvp(cmdArg[0], cmdArg) == -1) {
              errorAndEndProcess();
            }
          }
          pidPipe = fork();
          if (pidPipe == 0) {
            if (close(pipes[1]) == -1) {
              errorAndEndProcess();
            }

            if (dup2(pipes[0], STDIN_FILENO) == -1) {
              errorAndEndProcess();
            }

            if (execvp(cmdArgPipe[0], cmdArgPipe) == -1) {
              errorAndEndProcess();
            }
          } else {
            waitpid(-1, NULL, 0);
            exit(0);
          }
        } else {
          if (hasOut && !hasIn) {
            char *path = cmdArgOut[0];

            int retVal;
            if ((retVal = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU)) ==
                -1) {
              errorAndEndProcess();
            }

            if (dup2(retVal, STDOUT_FILENO) == -1) {
              errorAndEndProcess();
            }
          } else if (hasIn && !hasOut) {
            char *path = cmdArgIn[0];

            int retVal;
            if ((retVal = open(path, O_RDONLY)) == -1) {
              errorAndEndProcess();
            }

            if (dup2(retVal, STDIN_FILENO) == -1) {
              errorAndEndProcess();
            }
          } else if (hasIn && hasOut) {
            char *inPath = cmdArgIn[0];
            char *outPath = cmdArgOut[0];

            int retVal;
            if ((retVal = open(inPath, O_RDONLY)) == -1) {
              errorAndEndProcess();
            }

            if (dup2(retVal, STDIN_FILENO) == -1) {
              errorAndEndProcess();
            }

            if ((retVal = open(outPath, O_RDONLY)) == -1) {
              errorAndEndProcess();
            }

            if (dup2(retVal, STDIN_FILENO) == -1) {
              errorAndEndProcess();
            }
          }

          if (execvp(cmdArg[0], cmdArg) == -1) {
            errorAndEndProcess();
          }
        }
      }
      // parent
      else if (pid > 0) {
        if (!isBackground) {
          waitpid(pid, NULL, 0);
        } else {
          for (int i = 0; i < MAX_BACKGROUND_PROC; i++) {
            if (bgPids[i] == -1) {
              bgPids[i] = pid;
            }
          }

          for (int i = 0; i < MAX_BACKGROUND_PROC; i++) {
            if (bgPids[i] != -1) {
              if (waitpid(bgPids[i], NULL, WNOHANG) == bgPids[i]) {
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
static int readCmd(char *cmd, int *historyLine) {
  if (fgets(cmd, sizeof(char) * INPUT_LENGTH_MAX, stdin) == NULL) {
    write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
    return -1;
  }

  // exceeds 128 bytes - print error
  if (strchr(cmd, '\n') == NULL) {
    char readChar;
    while ((readChar = fgetc(stdin)) != '\n' && readChar != EOF) {
    }
    write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
    return -1;
  }

  // only newline - not an error
  if (cmd[0] == '\n') {
    *historyLine -= 1;
    return -1;
  }

  // trim newline
  size_t cmdLen = strlen(cmd);
  if (cmdLen > 0 && cmd[cmdLen - 1] == '\n') {
    cmd[cmdLen - 1] = '\0';
    cmdLen--;
  }

  // trim leading whitespace (forward)
  for (int i = 0; i < cmdLen; i++) {
    if (cmd[i] == ' ' || cmd[i] == '\t') {
      cmd++;
      i--;
    } else  // hit non-whitespace
    {
      break;
    }
  }

  // all whitespace - not an error
  if (strlen(cmd) == 0) {
    *historyLine -= 1;
    return -1;
  }

  // trim trailing whitespace (reverse)
  for (int i = strlen(cmd) - 1; i >= 0; i--) {
    if (cmd[i] == ' ' || cmd[i] == '\t') {
      cmd[i] = '\0';
    } else  // hit non-whitespace
    {
      break;
    }
  }

  return 0;
}

static int parseCmd(char *cmd, char **cmdArg, char **cmdArgIn, char **cmdArgOut,
                    char **cmdArgPipe, bool *hasOut, bool *hasIn, bool *hasPipe,
                    bool *isBackground) {
  char *savePtr;

  cmdArg[0] = strtok_r(cmd, " \t", &savePtr);
  *hasOut = false;
  *hasIn = false;
  *hasPipe = false;
  *isBackground = false;

  int i = 1;
  int outArgIndex = -1;
  int inArgIndex = -1;
  int pipeArgIndex = -1;
  int ampArgIndex = -1;
  while ((cmdArg[i] = strtok_r(NULL, " \t", &savePtr)) != NULL) {
    if (cmdArg[i] == NULL) continue;

    // assume redirection and pipeline never used together for this prj
    if (cmdArg[i][0] == '>') {
      *hasOut = true;
      outArgIndex = i;
    } else if (cmdArg[i][0] == '<') {
      *hasIn = true;
      inArgIndex = i;
    } else if (cmdArg[i][0] == '|') {
      *hasPipe = true;
      pipeArgIndex = i;
    }

    // assume redirection can be used with bg execution for this prj
    // assume pipeline cannot be used with bg execution for this prj
    if (cmdArg[i][0] == '&') {
      *isBackground = true;  // redundant because we have ampArgIndex
      ampArgIndex = i;
    }

    i++;
  }

  if (*hasOut && !*hasIn) {
    cmdArg[outArgIndex] = NULL;

    cmdArgOut[0] = cmdArg[outArgIndex + 1];
    cmdArgOut[1] = NULL;
    if (cmdArg[outArgIndex + 2] != NULL) {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return -1;
    }
  } else if (!*hasOut && *hasIn) {
    cmdArg[inArgIndex] = NULL;

    cmdArgIn[0] = cmdArg[inArgIndex + 1];
    cmdArgIn[1] = NULL;
    if (cmdArg[inArgIndex + 2] != NULL) {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return -1;
    }
  } else if (*hasOut && *hasIn) {
    cmdArg[inArgIndex] = NULL;
    cmdArg[outArgIndex] = NULL;

    cmdArgOut[0] = cmdArg[outArgIndex + 1];
    cmdArgOut[1] = NULL;

    cmdArgIn[0] = cmdArg[inArgIndex + 1];
    cmdArgIn[1] = NULL;

    if (inArgIndex > outArgIndex) {
      if (cmdArg[outArgIndex + 2] != NULL) {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        return -1;
      }

    } else {
      if (cmdArg[outArgIndex + 2] != NULL) {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        return -1;
      }
    }

    if (cmdArg[inArgIndex + 2] != NULL) {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return -1;
    }
  } else if (*hasPipe) {
    cmdArg[pipeArgIndex] = NULL;
    cmdArgPipe[0] = cmdArg[pipeArgIndex + 1];
    cmdArgPipe[1] = NULL;

    if (cmdArg[pipeArgIndex + 2] != NULL) {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return -1;
    }
  }

  // if (anglePipeArgIndex != -1) {
  //   cmdArg[anglePipeArgIndex] = NULL;

  //   for (i = 0; cmdArg[anglePipeArgIndex + i + 1] != NULL; i++) {
  //     cmdArg2[i] = cmdArg[anglePipeArgIndex + i + 1];
  //   }
  //   cmdArg2[i] = NULL;  // set last argv to NULL

  //   // we expect exactly one word after redirector or ampersand for bg exec
  //   if ((*hasOut || *hasIn) &&
  //       !(*hasOut && *hasIn) &&
  //       (cmdArg2[0] == NULL || cmdArg2[1] != NULL) &&
  //       (cmdArg2[1][0] != '&')) {
  //     write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
  //     return -1;
  //   }
  // }

  return 0;
}

static void handleExit(int *bgPids) {
  for (int i = 0; i < MAX_BACKGROUND_PROC; i++) {
    if (bgPids[i] != -1) {
      kill(bgPids[i], SIGTERM);
    }
  }
  exit(0);
}

static void handlePwd() {
  char workingDir[PATH_MAX];
  if (getcwd(workingDir, PATH_MAX) == NULL) {
    write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
    return;
  }
  printf("%s\n", workingDir);
  return;
}

static void handleCd(char *path) {
  if (path == NULL) {
    chdir(getenv("HOME"));
  } else {
    if (chdir(path) == -1) {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return;
    }
  }

  return;
}

static void errorAndEndProcess() {
  write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
  exit(1);
}

static void debugParse(char *cmd, char **cmdArg, char **cmdArgIn,
                       char **cmdArgOut, char **cmdArgPipe, bool *hasOut,
                       bool *hasIn, bool *hasPipe, bool *isBackground) {
  debugPrintf("%s - \">\" output redir\n", *hasOut ? "true" : "false");
  debugPrintf("%s - \"<\" input redir\n", *hasIn ? "true" : "false");
  debugPrintf("%s - \"|\" pipeline\n", *hasPipe ? "true" : "false");
  debugPrintf("%s - \"&\" bg exec\n", *isBackground ? "true" : "false");

  int i = 0;
  while (cmdArg[i] != NULL) {
    debugPrintf("\"%s\" [%p] - arg%i\n", cmdArg[i], &cmdArg[i], i);
    i++;
  }
  debugPrintf("\"%s\" [%p] - arg%i\n", cmdArg[i], &cmdArg[i], i);

  if (hasOut || hasIn || hasPipe) {
    i = 0;
    while (cmdArg2[i] != NULL) {
      debugPrintf("\"%s\" [%p] - arg%i\n", cmdArg2[i], &cmdArg2[i], i);
      i++;
    }
    debugPrintf("\"%s\" [%p] - arg%i\n", cmdArg2[i], &cmdArg2[i], i);
  }
}