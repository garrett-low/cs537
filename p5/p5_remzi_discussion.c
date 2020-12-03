#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// On-disk file system format.
// Both the kernel and user programs use this header file.

// Block 0 is unused.
// Block 1 is super block.
// Inodes start at block 2.

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// File system super block
struct superblock {
  uint size;     // Size of file system image (blocks)
  uint nblocks;  // Number of data blocks
  uint ninodes;  // Number of inodes.
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;               // File type
  short major;              // Major device number (T_DEV only)
  short minor;              // Minor device number (T_DEV only)
  short nlink;              // Number of links to inode in file system
  uint size;                // Size of file (bytes)
  uint addrs[NDIRECT + 1];  // Data block addresses
};

// Inodes per block.
#define IPB (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i) ((i) / IPB + 2)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) (b / BPB + (ninodes) / IPB + 3)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

void printBits(size_t const size, void const *const ptr) {
  unsigned char *b = (unsigned char *)ptr;
  unsigned char byte;
  int i, j;

  for (i = size - 1; i >= 0; i--) {
    for (j = 7; j >= 0; j--) {
      byte = (b[i] >> j) & 1;
      printf("%u", byte);
    }
  }
  // puts("");
}

// xv6 fs img
// similar to vsfs
// superblock | inode table | bitmap (data) | data blocks
// (some gaps in here)

int main(int argc, char *argv[]) {
  // Invalid # of arguments
  if (argc != 2) {
    fprintf(stderr, "blahblah\n");
    exit(1);
  }

  char *fsName = argv[1];

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
  sBlock = (struct superblock *)(fsPtr + BSIZE);
  printf("%d %d %d \n", sBlock->size, sBlock->nblocks, sBlock->ninodes);

  struct dinode *dinodePtr = (struct dinode *)(fsPtr + (2 * BSIZE));
  for (uint i = 0; i < sBlock->ninodes; i++, dinodePtr++) {
    printf("inode %d: type: %d\n", i, dinodePtr->type);

    // Test 3 testing
    if (i == 1) {
      struct dirent *rootDir =
          (struct dirent *)(fsPtr + (dinodePtr->addrs[0] * BSIZE));
      for (int j = 0; j < (BSIZE / sizeof(struct dirent)); j++, rootDir++) {
        printf("rootdir: inum %d, name %s\n", rootDir->inum, rootDir->name);
      }

    } else {
      continue;
    }
  }

  printf("BBLOCK(1,200): %ld\n", BBLOCK(1, sBlock->ninodes));
  printf("BBLOCK(5,200): %ld\n", BBLOCK(5, sBlock->ninodes));
  unsigned char *bitmap =
      (unsigned char *)(fsPtr + (BBLOCK(0, sBlock->ninodes) * BSIZE));
  printf("bitmap: 0x");
  for (int i = 0; i < 512; i++) {
    printf("%x", bitmap[i]);
    // printf("%p, ", &bitmap[i]);
    // printBits(sizeof(bitmap[i]), &bitmap[i]);
  }
  printf("\n");
  printf("bitmap again: 0x");
  for (int i = 0; i < 512; i++) {
    // if (bitmap[i] == 0xff) {
    //   printf("%x", bitmap[i]);
    // }
    // printf("%p, ", &bitmap[i]);
    printBits(sizeof(bitmap[i]), &bitmap[i]);
  }
  printf("\n");

  uint bitblocks = sBlock->size / (512 * 8) + 1;
  uint usedblocks = sBlock->ninodes / IPB + 3 + bitblocks;
  printf("initial used blocks: %d\n", usedblocks);
  printf("IPB: %ld\n", IPB);
  printf("0 & 1 = %i\n", 0 & 1);
  printf("1 & 1 = %i\n", 1 & 1);
  printf("(0b11001010 >> 7) & 1 = %i\n", (0b11001010 >> 7) & 1);
  printf("(0b11001010 >> 6) & 1 = %i\n", (0b11001010 >> 6) & 1);
  printf("(0b11001010 >> 5) & 1 = %i\n", (0b11001010 >> 5) & 1);
  printf("(0b11001010 >> 4) & 1 = %i\n", (0b11001010 >> 4) & 1);
  printf("(0b11001010 >> 3) & 1 = %i\n", (0b11001010 >> 3) & 1);
  printf("(0b11001010 >> 2) & 1 = %i\n", (0b11001010 >> 2) & 1);
  printf("(0b11001010 >> 1) & 1 = %i\n", (0b11001010 >> 1) & 1);
  printf("(0b11001010 >> 0) & 1 = %i\n", (0b11001010 >> 0) & 1);

  return 0;
}