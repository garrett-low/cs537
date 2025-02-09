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
static void outToFile(char *outPath);
static void inFromFile(char *inPath);

// debug
static void debugParse(char *cmd, char **cmdArg, char **cmdArgIn,
                       char **cmdArgOut, char **cmdArgPipe, bool *hasOut,
                       bool *hasIn, bool *hasPipe, bool *isBackground);

int main(int argc, char *argv[]) {
  if (argc > 1) {
    errorAndEndProcess();
  }

  int historyLine = 0;
  int bgPids[MAX_BACKGROUND_PROC];
  for (int i = 0; i < MAX_BACKGROUND_PROC; i++) {
    bgPids[i] = 0;
    // debugPrintf("bg pid [%d]\n", bgPids[i]);
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
    char *cmdArgIn[INPUT_LENGTH_MAX];
    char *cmdArgOut[INPUT_LENGTH_MAX];
    char *cmdArgPipe[INPUT_LENGTH_MAX];
    bool hasOut = false;
    bool hasIn = false;
    bool hasPipe = false;
    bool isBackground = false;
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
    } else if (hasPipe) {
      // pipe commands
      int pipes[2];
      int stdinCpy = dup(STDIN_FILENO);
      int stdoutCpy = dup(STDOUT_FILENO);
      if (pipe(pipes) == -1) {
        errorAndEndProcess();
      }

      int pidPipe = fork();
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

      int pidPipe2 = fork();
      if (pidPipe2 == 0) {
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
        close(pipes[0]);
        close(pipes[1]);
        waitpid(pidPipe2, NULL, 0);
      }
    } else {
      // other non-pipe commands
      int pid = fork();
      // child
      if (pid == 0) {
        if (hasOut && !hasIn) {
          outToFile(cmdArgOut[0]);
        } else if (hasIn && !hasOut) {
          inFromFile(cmdArgIn[0]);
        } else if (hasIn && hasOut) {
          inFromFile(cmdArgIn[0]);
          outToFile(cmdArgOut[0]);
        }

        if (execvp(cmdArg[0], cmdArg) == -1) {
          errorAndEndProcess();
        }
      }

      // parent
      else if (pid > 0) {
        if (!isBackground) {
          waitpid(pid, NULL, 0);
        } else {
          for (int i = 0; i < MAX_BACKGROUND_PROC; i++) {
            if (bgPids[i] == 0) {
              bgPids[i] = pid;
              // debugPrintf("bg pid [%d]\n", bgPids[i]);
              break;
            }
          }
        }
        for (int i = 0; i < MAX_BACKGROUND_PROC; i++) {
          if (bgPids[i] != 0) {
            debugPrintf("check bg pid [%d]\n", bgPids[i]);
            // if (kill(bgPids[i], 0) != 0) {
            // if (bgPids[i] != 0) {
            // int status;
            //   waitpid(bgPids[i], &status, WNOHANG);
            if (waitpid(bgPids[i], NULL, WNOHANG) != 0) {
              // if (status == WIFEXITED) {
              debugPrintf("zombie bg pid [%d]\n", bgPids[i]);
              waitpid(bgPids[i], NULL, 0);
              bgPids[i] = 0;
              // }
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
    if (cmdArg[outArgIndex + 2] != NULL && cmdArg[outArgIndex + 2][0] != '&') {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return -1;
    }
    if (*isBackground) {
      if (cmdArg[outArgIndex + 2][1] != '\0') {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        return -1;
      }
    }
  } else if (!*hasOut && *hasIn) {
    cmdArg[inArgIndex] = NULL;

    cmdArgIn[0] = cmdArg[inArgIndex + 1];
    cmdArgIn[1] = NULL;
    if (cmdArg[inArgIndex + 2] != NULL && cmdArg[inArgIndex + 2][0] != '&') {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return -1;
    }
    if (*isBackground) {
      if (cmdArg[inArgIndex + 2][1] != '\0') {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        return -1;
      }
    }
  } else if (*hasOut && *hasIn) {
    cmdArg[inArgIndex] = NULL;
    cmdArg[outArgIndex] = NULL;

    cmdArgOut[0] = cmdArg[outArgIndex + 1];
    cmdArgOut[1] = NULL;

    cmdArgIn[0] = cmdArg[inArgIndex + 1];
    cmdArgIn[1] = NULL;

    if (inArgIndex > outArgIndex) {
      if (cmdArg[inArgIndex + 2] != NULL && cmdArg[inArgIndex + 2][0] != '&') {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        return -1;
      }
      if (*isBackground) {
        if (cmdArg[inArgIndex + 2][1] != '\0') {
          write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
          return -1;
        }
      }
    } else {
      if (cmdArg[outArgIndex + 2] != NULL &&
          cmdArg[outArgIndex + 2][0] != '&') {
        write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
        return -1;
      }
      if (*isBackground) {
        if (cmdArg[outArgIndex + 2][1] != '\0') {
          write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
          return -1;
        }
      }
    }
  } else if (*hasPipe) {
    cmdArg[pipeArgIndex] = NULL;
    if (cmdArg[pipeArgIndex + 1] == NULL) {
      write(STDERR_FILENO, GENERIC_ERROR, strlen(GENERIC_ERROR));
      return -1;
    }

    for (i = 0; cmdArg[pipeArgIndex + i + 1] != NULL; i++) {
      cmdArgPipe[i] = cmdArg[pipeArgIndex + i + 1];
    }
    cmdArgPipe[i] = NULL;
  }

  if (*isBackground) {
    cmdArg[ampArgIndex] = NULL;
  }

  // debugParse(cmd, cmdArg, cmdArgIn, cmdArgOut, cmdArgPipe, hasOut, hasIn,
  //  hasPipe, isBackground);

  return 0;
}

static void handleExit(int *bgPids) {
  for (int i = 0; i < MAX_BACKGROUND_PROC; i++) {
    if (bgPids[i] != 0) {
      debugPrintf("kill bg pid [%d]\n", bgPids[i]);
      kill(bgPids[i], SIGKILL);
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

  if (*hasOut) {
    i = 0;
    while (cmdArgOut[i] != NULL) {
      debugPrintf("\"%s\" [%p] - arg%i\n", cmdArgOut[i], &cmdArgOut[i], i);
      i++;
    }
    debugPrintf("\"%s\" [%p] - arg%i\n", cmdArgOut[i], &cmdArgOut[i], i);
  }
  if (*hasIn) {
    i = 0;
    while (cmdArgIn[i] != NULL) {
      debugPrintf("\"%s\" [%p] - arg%i\n", cmdArgIn[i], &cmdArgIn[i], i);
      i++;
    }
    debugPrintf("\"%s\" [%p] - arg%i\n", cmdArgIn[i], &cmdArgIn[i], i);
  }
  if (*hasPipe) {
    i = 0;
    while (cmdArgPipe[i] != NULL) {
      debugPrintf("\"%s\" [%p] - arg%i\n", cmdArgPipe[i], &cmdArgPipe[i], i);
      i++;
    }
    debugPrintf("\"%s\" [%p] - arg%i\n", cmdArgPipe[i], &cmdArgPipe[i], i);
  }
}

static void outToFile(char *outPath) {
  int retVal;
  if ((retVal = open(outPath, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU)) == -1) {
    errorAndEndProcess();
  }

  if (dup2(retVal, STDOUT_FILENO) == -1) {
    errorAndEndProcess();
  }
}

static void inFromFile(char *inPath) {
  int retVal;
  if ((retVal = open(inPath, O_RDONLY)) == -1) {
    errorAndEndProcess();
  }

  if (dup2(retVal, STDIN_FILENO) == -1) {
    errorAndEndProcess();
  }
}