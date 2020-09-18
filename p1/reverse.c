/**
 * Course: CS537
 * Project: Project 1A - reverse
 * Name: Garrett Low
 * CSL Login: Low
 * Wisc Email: grlow@wisc.edu
 * Web Sources Used:
 *   debug statements without commenting out using preprocessor directives:
 *     https://docs.microsoft.com/en-us/cpp/preprocessor/preprocessor-directives
 *     https://docs.microsoft.com/en-us/cpp/preprocessor/variadic-macro
 *     https://www.cs.colostate.edu/~fsieker/misc/debug/DEBUG.html
 * Personal Help: N/A
 * Description:
 *   Read in an input file
 *   Reverse the line order
 *   Reverse the contents of each line
 *   Write to output file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// debug preprocessor directives
#ifdef DEBUG
#define debugPrintf(...) printf(__VA_ARGS__)
#else
#define debugPrintf(...) ((void)0)
#endif

// define knowns
#define USAGE_ERROR "Usage: reverse -i inputfile -o outputfile\n"
#define MAX_LINE_LEN 512

int main(int argc, char *argv[]) {
  if (argc != 5) {
    fprintf(stderr, USAGE_ERROR);
    exit(1);
  }

  char *inFilename;
  char *outFilename;
  if (argv[1][1] == 'i') {
    if (argv[3][1] == 'o') {
      inFilename = argv[2];
      outFilename = argv[4];
    } else {
      fprintf(stderr, USAGE_ERROR);
      exit(1);
    }
  } else if (argv[1][1] == 'o') {
    if (argv[3][1] == 'i') {
      outFilename = argv[2];
      inFilename = argv[4];
    } else {
      fprintf(stderr, USAGE_ERROR);
      exit(1);
    }
  }

  debugPrintf("input file: %s\noutput file: %s\n", inFilename, outFilename);

  // input file size
  unsigned long long int maxLines;  // # of lines < input file size
  struct stat inStat;
  if (stat(inFilename, &inStat) == -1) {
    fprintf(stderr, "Error: stat() failed to read %s.\n", inFilename);
    exit(1);
  }
  maxLines = inStat.st_size;

  FILE *outFile;
  outFile = fopen(outFilename, "wb");
  if (outFile == NULL) {
    fprintf(stderr, "Error: Could not read the specified file!\n");
    exit(1);
  }
  
  if (maxLines == 0) { // input file is empty
    exit(0); // Hints say to exit(0) when runs cleanly? why not return 0?
  }

  // read file
  FILE *inFile;
  inFile = fopen(inFilename, "r");
  if (inFile == NULL) {
    fprintf(stderr, "Error: Could not read the specified file!\n");
    exit(1);
  }

  char **inputStrAry;
  inputStrAry = malloc(sizeof(char *) * maxLines);
  if (inputStrAry == NULL) {
    fprintf(stderr, "Error: Failed to allocate space for line array!\n");
  }

  // getline does not exist in Windows
  ssize_t nread;
  char *line = NULL;
  size_t len = 0;  // man says need to free the buffer if getline fails?
  unsigned long long int lineCnt = 0;
  while ((nread = getline(&line, &len, inFile)) != -1) {
    int lineEnd = nread - 2;  // content starts one before pos of newline char
    int lineLen = nread - 1;
    if (nread == 1 || line[lineEnd + 1] != '\n') {
      lineEnd = lineEnd + 1;
      lineLen = lineLen + 1;
    }

    int lineStart = 0;
    while (lineStart < lineEnd) {
      char temp = line[lineStart];
      line[lineStart] = line[lineEnd];
      line[lineEnd] = temp;
      lineStart++;
      lineEnd--;
    }

    inputStrAry[lineCnt] = (char *)malloc(lineLen + 1);
    if (inputStrAry[lineCnt] == NULL) {
      fprintf(stderr, "Error: Failed to allocate space for line!\n");
    }

    memcpy(inputStrAry[lineCnt], line, sizeof(char) * lineLen);
    debugPrintf("%s", inputStrAry[lineCnt]);
    lineCnt++;
  }
  free(line);

  debugPrintf("line count: %lli\n", lineCnt);

  for (int i = lineCnt - 1; i >= 0; i--) {
    char *writeLine = inputStrAry[i];
    // if this is not the last line and the line doesn't contain only a newline
    // then we need to add a new line
    if (i != 0 && strlen(inputStrAry[i]) != 1) {
      writeLine = strcat(inputStrAry[i], "\n");
    } else {
      writeLine = inputStrAry[i];
    }

    debugPrintf("%s", writeLine);
    fwrite(writeLine, strlen(writeLine), 1, outFile);
    free(inputStrAry[i]);
  }

  if (fclose(inFile) != 0) {
    printf("Error: Could not close input file!\n");
    exit(1);
  }

  if (fclose(outFile) != 0) {
    printf("Error: Could not close output file!\n");
    exit(1);
  }

  free(inputStrAry);
  exit(0);
}