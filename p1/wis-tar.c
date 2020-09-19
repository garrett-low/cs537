/**
 * Course: CS537
 * Project: Project 1A - reverse
 * Name: Garrett Low
 * CSL Login: Low
 * Wisc Email: grlow@wisc.edu
 * Web Sources Used:
 *   reading a file in chunks: https://stackoverflow.com/a/28197753
 * Personal Help: N/A
 * Description:
 *   Archive file
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

int main(int argc, char *argv[]) {
  // handle too few args
  if (argc < 3) {
    fprintf(stderr, "Usage: wis-tar ARCHIVE [FILE ...]\n");
    exit(EXIT_FAILURE);
  }

  // overwrite any existing tar file
  char *tarName = argv[1];
  FILE *tarStream = NULL;
  tarStream = fopen(tarName, "wb");
  if (tarStream == NULL) {
    fprintf(stderr, "Error: Could not create the tar file!\n");
    exit(EXIT_FAILURE);
  }

  // headers
  for (int i = 2; i < argc; i++) {
    char *inName = argv[i];

    // add the filename to the header, padded with '\0' to 256 bytes
    char *hdrName = malloc(sizeof(char) * 256);
    if (hdrName == NULL) {
      fprintf(stderr, "Error: Failed to allocate space for header filename!\n");
      exit(1);
    }
    memset(hdrName, '\0', 256);
    int inNameLen = strlen(inName);
    if (memcpy(hdrName, inName, inNameLen) == NULL) {
      fprintf(stderr, "Error: Could not set or copy filename %s\n", inName);
      exit(EXIT_FAILURE);
    }
    int hdrNameLen = strlen(hdrName);
    if (fwrite(hdrName, hdrNameLen, 1, tarStream) < 0) {
      fprintf(stderr, "Error: Could not write filename to archive %s\n", tarName);
      exit(EXIT_FAILURE);
    }

    // add the filesize to the header (8 bytes)
    struct stat inStat;
    if (stat(inName, &inStat) == -1) {
      fprintf(stderr, "Error: stat() failed to read %s.\n", inName);
      exit(EXIT_FAILURE);
    }
    uint64_t inSize = inStat.st_size;
    if (fwrite(&inSize, sizeof(uint64_t), 1, tarStream) < 0) {
      fprintf(stderr, "Error: Could not write filesize to archive %s\n", tarName);
      exit(EXIT_FAILURE);
    }
    
    // add the file contents to the tar, in chunks
    FILE *inStream = NULL;
    unsigned char buffer[1024];
    size_t bytesRead = 0;
    inStream = fopen(inName, "rb");
    if (inStream = NULL)

    // add padding (8 nul characters) at end of file content
    char padding[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    if (fwrite(padding, sizeof(char) * 8, 1, tarStream) < 0) {
      fprintf(stderr, "Error: Could not write padding to archive %s\n", tarName);
      exit(EXIT_FAILURE);
    }

    free(hdrName);
      if (fclose(inStream) != 0) {
    fprintf(stderr, "Error: Could not close input file!\n");
    exit(1);
  }
  }

  if (fclose(tarStream) != 0) {
    fprintf(stderr, "Error: Could not close tar file!\n");
    exit(1);
  }
}