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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
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
    fprintf(stderr,
            "Error: Failed to allocate space for input file content!\n");
  }

  FILE *outFile;
  outFile = fopen(outFilename, "w");
  if (outFile == NULL) {
    fprintf(stderr, "Error: Could not read the specified file!\n");
    exit(1);
  }

  // char readLine[MAX_LINE_LEN];
  // while (fgets(readLine, MAX_LINE_LEN, inFile) != NULL) {
  //   fwrite(readLine, MAX_LINE_LEN, 1, stdout);
  //   fwrite(readLine, MAX_LINE_LEN, 1, outFile);
  // }

  // getline does not exist in Windows
  size_t nread;
  char *line = NULL;
  size_t len = 0;  // man says need to free the buffer if getline fails?
  while ((nread = getline(&line, &len, inFile)) != -1) {
    fwrite(line, nread, 1, outFile);
  }
  free(line);

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