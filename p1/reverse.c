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

int main(int argc, char *argv[])
{
  if (argc != 5)
  {
    fprintf(stderr, USAGE_ERROR);
    exit(1);
  }

  char *inFilename;
  char *outFilename;
  if (argv[1][1] == 'i' && argv[3][1] == 'o')
  {
    inFilename = argv[2];
    outFilename = argv[4];
  }
  else
  {
    fprintf(stderr, USAGE_ERROR);
    exit(1);
  }

  debugPrintf("input file: %s\noutput file: %s\n", inFilename, outFilename);

  FILE *outFile;
  outFile = fopen(outFilename, "wb");
  if (outFile == NULL)
  {
    fprintf(stderr, "reverse: Cannot open file: %s\n", outFilename);
    exit(1);
  }

  // read file
  FILE *inFile;
  inFile = fopen(inFilename, "r");
  if (inFile == NULL)
  {
    fprintf(stderr, "reverse: Cannot open file: %s\n", inFilename);
    exit(1);
  }

  // input file size is 0
  unsigned long long int maxLines; // # of lines < input file size
  struct stat inStat;
  if (stat(inFilename, &inStat) == -1)
  {
    fprintf(stderr, "Error: stat() failed to read %s.\n", inFilename);
    exit(1);
  }
  maxLines = inStat.st_size;
  if (maxLines == 0)
  {          // input file is empty
    exit(0); // Hints say to exit(0) when runs cleanly? why not return 0?
  }

  char **inputStrAry;
  inputStrAry = (char **)malloc(sizeof(char *) * maxLines);
  if (inputStrAry == NULL)
  {
    fprintf(stderr, "Error: Failed to allocate space for line array!\n");
    exit(1);
  }

  // getline does not exist in Windows
  ssize_t nread;
  char *line = NULL;
  size_t len = 0; // man says need to free the buffer if getline fails?
  unsigned long long int lineCnt = 0;
  while ((nread = getline(&line, &len, inFile)) != -1)
  {
    int lineEnd = nread - 2;

    // swap
    int lineStart = 0;
    while (lineStart < lineEnd)
    {
      char temp = line[lineStart];
      line[lineStart] = line[lineEnd];
      line[lineEnd] = temp;
      lineStart++;
      lineEnd--;
    }

    inputStrAry[lineCnt] = (char *)malloc(nread); // nread: account for terminating '\0'
    if (inputStrAry[lineCnt] == NULL)
    {
      fprintf(stderr, "Error: Failed to allocate space for line!\n");
      exit(1);
    }

    memcpy(inputStrAry[lineCnt], line, sizeof(char) * (nread - 1)); // nread-1: don't include newline
    // debugPrintf("%s", inputStrAry[lineCnt]);
    lineCnt++;
  }
  free(line);

  // debugPrintf("line count: %lli\n", lineCnt);

  for (int i = lineCnt - 1; i >= 0; i--)
  {
    char *writeLine = NULL;
    writeLine = (char *)malloc(strlen(inputStrAry[i]) + 2);
    if (writeLine == NULL)
    {
      fprintf(stderr, "Error: Failed to allocate space for writeLine!\n");
      exit(1);
    }
    strcpy(writeLine,inputStrAry[i]);
    writeLine = strcat(writeLine, "\n");

    if (fwrite(writeLine, strlen(writeLine), 1, outFile) < 1)
    {
      fprintf(stderr, "Error: Could not write to %s!\n", outFilename);
      exit(1);
    }
    debugPrintf("%s", writeLine);
    free(writeLine);
  }

  if (fclose(inFile) != 0)
  {
    fprintf(stderr, "Error: Could not close input file!\n");
    exit(1);
  }

  if (fclose(outFile) != 0)
  {
    fprintf(stderr, "Error: Could not close output file!\n");
    exit(1);
  }

  for (int i = 0; i < maxLines; i++)
  {
    free(inputStrAry[i]);
  }

  exit(0);

  return 0;
}