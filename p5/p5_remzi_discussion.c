#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

// Block 0 is unused.
// Block 1 is super block. (see mkfs -> wsect(1, sb))
// Inodes start at block 2.

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// File system super block
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
};

// xv6 fs img
// similar to vsfs
// superblock | inode table | bitmap (data) | data blocks
// (some gaps in here)

int main(int argc, char *argv[]) {
  char *fsName = "fs.img";
  
  int fd = open(fsName, O_RDONLY);
  if (fd == -1) {
    exit(1);
  }

  struct stat fsStat;
  if (fstat(fd, &fsStat) == -1) {
    fprintf(stderr, "Error: stat() failed to read %s.\n", fsName);
    exit(1);
  }
  
  void *fsPtr = mmap(NULL, fsStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (fsPtr == MAP_FAILED) {
    exit(1);
  }
  
  struct superblock *sBlock;
  sBlock = (struct superblock *) (fsPtr + BSIZE);
  printf("%d %d %d \n", sBlock->size, sBlock->nblocks, sBlock->ninodes);
  
  return 0;
}