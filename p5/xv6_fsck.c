#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// xv6 - FS constants
// Block 0 is unused.
// Block 1 is super block. (see mkfs -> wsect(1, sb))
// Inodes start at block 2. Inodes per block = 8 -> 25 blocks for inodes
// Bitmap starts at block 28.
// Data blocks start at block 29.
// | boot | superblock | inode table | bitmap (data) | data |

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// File system super block
struct superblock {
  uint size;     // Size of file system image (blocks)
  uint nblocks;  // Number of data blocks
  uint ninodes;  // Number of inodes.
};

// Inode types
#define T_DIR 1   // Directory
#define T_FILE 2  // File
#define T_DEV 3   // Special device

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

// P5 - My constants
// Debug preprocessor directives
#ifdef DEBUG
#define debugPrintf(...) printf(__VA_ARGS__)
#else
#define debugPrintf(...) ((void)0)
#endif

// Errors
#define USAGE_ERROR "Usage: xv6_fsck <file_system_image>.\n"
#define FSIMAGE_ERROR "image not found.\n"
#define ERROR1_BAD_INODE "ERROR: bad inode.\n"
#define ERROR2_BAD_DIRECT_DATA "ERROR: bad direct address in inode.\n"
#define ERROR2_BAD_INDIRECT_DATA "ERROR: bad indirect address in inode.\n"

int main(int argc, char *argv[]) {
  debugPrintf("constants: IPB %ld, BPB %d\n", IPB, BPB);

  // Invalid # of arguments
  if (argc != 2) {
    fprintf(stderr, USAGE_ERROR);
    exit(1);
  }

  // FS image does not exist
  char *fsName = argv[1];
  int fd = open(fsName, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, FSIMAGE_ERROR);
    exit(1);
  }

  // Get size of FS file
  struct stat fsStat;
  if (fstat(fd, &fsStat) == -1) {
    fprintf(stderr, FSIMAGE_ERROR);
    exit(1);
  }

  void *fsPtr = mmap(NULL, fsStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (fsPtr == MAP_FAILED) {
    fprintf(stderr, "Failed to map memory to store FS file.\n");
    exit(1);
  }

  struct superblock *sBlock;
  sBlock = (struct superblock *)(fsPtr + BSIZE);
  debugPrintf("sBlock: size %d, # data blocks %d, # inodes %d \n", sBlock->size,
              sBlock->nblocks, sBlock->ninodes);

  // Traverse inodes - start at second block
  struct dinode *dinodePtr = (struct dinode *)(fsPtr + (2 * BSIZE));
  for (int i = 0; i < sBlock->ninodes; i++, dinodePtr++) {
    short type = dinodePtr->type;

    // Test 1 - each inode is either unallocated or one of the valid types
    if (type != 0 && type != T_FILE && type != T_DIR && type != T_DEV) {
      debugPrintf("inode %d: type %d\n", i, type);
      fprintf(stderr, ERROR1_BAD_INODE);
      exit(1);
    }

    // Test 2 - for inuse inodes, points to valid datablock address
    // TODO: Badindir2
    if (type == T_FILE) {
      debugPrintf("file inode %d: ", i);
      for (int j = 0; j < NDIRECT + 1; j++) {
        uint dblockNum = dinodePtr->addrs[j];
        debugPrintf("dblockNum %d, ", dblockNum);
        if (!(dblockNum == 0 ||
              ((dblockNum >= 29) && (dblockNum < 995 + 28)))) {
          // debugPrintf("inode %d: dblockNum: %d \n", i, dblockNum);
          if (j < NDIRECT) {  // Direct blocks
            fprintf(stderr, ERROR2_BAD_DIRECT_DATA);
          } else {  // Indirect blocks
            fprintf(stderr, ERROR2_BAD_INDIRECT_DATA);
          }

          exit(1);
        }
      }
      debugPrintf("\n");
    }

    // Traverse indirect blocks for invalid datablock address
    uint indirectBlockNum = dinodePtr->addrs[NDIRECT + 1];
    uint *indirectAddr = (uint *indirectAddr) (indirectBlockNum * BSIZE);
  }

  // figure out bitmap

  // do rest of P5

  return 0;
}