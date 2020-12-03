#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// xv6 - FS constants
// Block 0 is unused.
// Block 1 is super block. (see mkfs -> wsect(1, sb))
// Inodes start at block 2. Inodes per block = 8 -> 25 blocks for inodes
// Empty block at 27.
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

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

// P5 - My globals/constants
void *fsPtr;
struct superblock *sBlock;
// Debug preprocessor directives
#ifdef DEBUG
#define debugPrintf(...) printf(__VA_ARGS__)
#else
#define debugPrintf(...) ((void)0)
#endif

#define MAXDIR_PER_BLOCK (BSIZE / sizeof(struct dirent))

// Errors
#define USAGE_ERROR "Usage: xv6_fsck <file_system_image>.\n"
#define FSIMAGE_ERROR "image not found.\n"
#define ERROR1_BAD_INODE "ERROR: bad inode.\n"
#define ERROR2_BAD_DIRECT_DATA "ERROR: bad direct address in inode.\n"
#define ERROR2_BAD_INDIRECT_DATA "ERROR: bad indirect address in inode.\n"
#define ERROR3_BAD_ROOT "ERROR: root directory does not exist.\n"
#define ERROR4_BAD_DIR_FORMAT "ERROR: directory not properly formatted.\n"
#define ERROR5_INODE_NOT_MARKED \
  "ERROR: address used by inode but marked free in bitmap.\n"
#define ERROR6_BITMAP_NOT_MARKED \
  "ERROR: bitmap marks block in use but it is not in use.\n"

void test1(uint i, short type);
void test2(uint i, struct dinode *inodePtr);
void test3();
void test4(uint i, struct dinode *inodePtr);
void collectInodeUsed(struct dinode *inodePtr, uint *inodeUsed,
                      int *inodeUsedCount);
void collectBitmapUsed(uint *bitmapUsed, int *bitmapUsedCount);
void test5(uint *inodeUsed, int inodeUsedCount, uint *bitmapUsed,
           int bitmapUsedCount);
void test6(uint *inodeUsed, int inodeUsedCount, uint *bitmapUsed,
           int bitmapUsedCount);

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

  // Read in the fs.img into memory
  fsPtr = mmap(NULL, fsStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (fsPtr == MAP_FAILED) {
    fprintf(stderr, "Failed to map memory to store FS file.\n");
    exit(1);
  }

  // Superblock info
  sBlock = (struct superblock *)(fsPtr + BSIZE);
  debugPrintf("sBlock: size %d, # data blocks %d, # inodes %d \n", sBlock->size,
              sBlock->nblocks, sBlock->ninodes);

  // Traverse inodes - start at second block
  // Initialize array
  uint inodeUsed[sBlock->nblocks];
  int inodeUsedCount = 0;
  for (int i = 0; i < sBlock->nblocks; i++) {
    inodeUsed[i] = 0;
  }
  struct dinode *inodePtr = (struct dinode *)(fsPtr + (2 * BSIZE));
  for (uint i = 0; i < sBlock->ninodes; i++, inodePtr++) {
    // Test 1 - each inode is either unallocated or one of the valid types
    test1(i, inodePtr->type);

    // Test 2 - for inuse inodes; points to valid datablock address
    test2(i, inodePtr);

    // Test 4 - each dir contains . and ..; . points to dir itself
    test4(i, inodePtr);

    // Setup for test 5 and 6
    collectInodeUsed(inodePtr, inodeUsed, &inodeUsedCount);
  }

  // Test 3 - root dir exists; inode num 1; parent is itself
  test3();

  // Initialize array
  int bitmapUsed[sBlock->size];
  for (int i = 0; i < sBlock->size; i++) {
    bitmapUsed[i] = 0;
  }
  int bitmapUsedCount = 0;
  collectBitmapUsed(bitmapUsed, &bitmapUsedCount);

  // Test 5 - check inode data blocks against bitmap
  test5(inodeUsed, inodeUsedCount, bitmapUsed, bitmapUsedCount);

  // Test 6 - check bitmap against inode data blocks
  test6(inodeUsed, inodeUsedCount, bitmapUsed, bitmapUsedCount);

  return 0;
}

// Test 1 - each inode is either unallocated or one of the valid types
void test1(uint i, short type) {
  if (!(type == 0 || type == T_FILE || type == T_DIR || type == T_DEV)) {
    debugPrintf("ERROR: inode %d, type %d\n", i, type);
    fprintf(stderr, ERROR1_BAD_INODE);
    exit(1);
  }
}

// Test 2 - for inuse inodes; points to valid datablock address
void test2(uint i, struct dinode *inodePtr) {
  if (inodePtr->type != T_FILE) {
    return;
  }

  // Direct block number invalid
  // debugPrintf("file inode %d: ", i);
  for (int j = 0; j < NDIRECT; j++) {
    uint dblockNum = inodePtr->addrs[j];
    // debugPrintf("dblockNum %d, ", dblockNum);
    if (!(dblockNum == 0 || ((dblockNum >= 29) && (dblockNum < 995 + 29)))) {
      debugPrintf("\nERROR: inode %d: dblockNum: %d\n", i, dblockNum);
      fprintf(stderr, ERROR2_BAD_DIRECT_DATA);
      exit(1);
    }
  }
  // debugPrintf("\n");

  // Indirect block number invalid
  uint indirectBlockNum = inodePtr->addrs[NDIRECT];
  if (!(indirectBlockNum == 0 ||
        ((indirectBlockNum >= 29) && (indirectBlockNum < 995 + 29)))) {
    debugPrintf("ERROR: inode %d: indirectBlockNum %d \n", i, indirectBlockNum);
    fprintf(stderr, ERROR2_BAD_INDIRECT_DATA);
    exit(1);
  }
  if (indirectBlockNum == 0) {
    return;
  }

  // Traverse indirect blocks for invalid datablock address
  uint *indirectAddr = (uint *)(fsPtr + (indirectBlockNum * BSIZE));
  for (int j = 0; j < NINDIRECT; j++) {
    uint dblockNum = indirectAddr[j];
    if (!(dblockNum == 0 || ((dblockNum >= 29) && (dblockNum < 995 + 28)))) {
      debugPrintf("inode %d: indirectBlockNum %d, dblockNum: %d \n", i,
                  indirectBlockNum, dblockNum);
      fprintf(stderr, ERROR2_BAD_INDIRECT_DATA);
      exit(1);
    }
  }
}

// Test 3 - root dir exists; inode num 1; parent is itself
void test3() {
  struct dinode *rootInodePtr =
      (struct dinode *)(fsPtr + (2 * BSIZE) + (1 * sizeof(struct dinode)));
  if (rootInodePtr->type != T_DIR) {
    debugPrintf("ERROR: type of inum 1 (root) not T_DIR\n");
    fprintf(stderr, ERROR3_BAD_ROOT);
    exit(1);
  }
  struct dirent *rootDirent =
      (struct dirent *)(fsPtr + (rootInodePtr->addrs[0] * BSIZE));
  if (!(rootDirent->inum == 1 && strcmp(rootDirent->name, ".") == 0)) {
    debugPrintf("ERROR: root not inum 1\n");
    fprintf(stderr, ERROR3_BAD_ROOT);
    exit(1);
  }
  rootDirent++;
  if (!(rootDirent->inum == 1 && strcmp(rootDirent->name, "..") == 0)) {
    debugPrintf("ERROR: root not parent of itself\n");
    fprintf(stderr, ERROR3_BAD_ROOT);
    exit(1);
  }
}

// Test 4 - each dir contains . and ..; . points to dir itself
void test4(uint i, struct dinode *inodePtr) {
  if (inodePtr->type == T_FILE || inodePtr->type == T_DEV) {
    return;
  }
  if (i == 1 || i == 0) {
    return;
  }

  bool foundCurrDot = false;
  bool foundParentDot = false;
  bool isDirUsed = false;
  for (int j = 0; j < NDIRECT; j++) {
    struct dirent *dirEntPtr =
        (struct dirent *)(fsPtr + (inodePtr->addrs[j] * BSIZE));
    for (int k = 0; k < MAXDIR_PER_BLOCK; k++, dirEntPtr++) {
      if (foundCurrDot && foundParentDot) {
        break;
      }
      if (dirEntPtr->inum == 0) {
        continue;
      }
      isDirUsed = true;

      if (strcmp(dirEntPtr->name, ".") == 0) {
        foundCurrDot = true;
        if (dirEntPtr->inum != i) {
          fprintf(stderr, ERROR4_BAD_DIR_FORMAT);
          exit(1);
        }
        continue;
      } else if (strcmp(dirEntPtr->name, "..") == 0) {
        foundParentDot = true;
        continue;
      }
    }
    if (isDirUsed && !(foundCurrDot && foundParentDot)) {
      debugPrintf("ERROR: . check %i, .. check %i, used check %i\n",
                  foundCurrDot, foundParentDot, isDirUsed);
      fprintf(stderr, ERROR4_BAD_DIR_FORMAT);
      exit(1);
    }
  }
}

void collectInodeUsed(struct dinode *inodePtr, uint *inodeUsed,
                      int *inodeUsedCount) {
  if (inodePtr->type == T_DIR || inodePtr->type == T_FILE) {
    for (int j = 0; j < NDIRECT; j++) {
      if (inodePtr->addrs[j] != 0) {
        inodeUsed[(*inodeUsedCount)++] = inodePtr->addrs[j];
      }
    }

    if (inodePtr->addrs[NDIRECT]) {
      inodeUsed[(*inodeUsedCount)++] = inodePtr->addrs[NDIRECT];
      uint *indirectAddr = (uint *)(fsPtr + (inodePtr->addrs[NDIRECT] * BSIZE));
      for (int j = 0; j < NINDIRECT; j++) {
        if (indirectAddr[j] != 0) {
          inodeUsed[(*inodeUsedCount)++] = indirectAddr[j];
        }
      }
    }
  }
}

void collectBitmapUsed(uint *bitmapUsed, int *bitmapUsedCount) {
  unsigned char *bitmap =
      (unsigned char *)(fsPtr + (BBLOCK(0, sBlock->ninodes) * BSIZE));
  int blockNum = 0;
  for (int i = 0; i < BSIZE; i++) {
    for (int j = 0; j < 8; j++, blockNum++) {
      uint bit = (bitmap[i] >> j) & 1;
      if (bit == 1) {
        bitmapUsed[(*bitmapUsedCount)++] = blockNum;
      }
    }
  }
}

void test5(uint *inodeUsed, int inodeUsedCount, uint *bitmapUsed,
           int bitmapUsedCount) {
  for (int i = 0; i < inodeUsedCount; i++) {
    bool isInodeUsedBlockInBitmap = false;
    uint inodeUsedBlockNum = inodeUsed[i];
    // debugPrintf("used inode data block %d\n", inodeUsedBlockNum);
    for (int j = 29; j < bitmapUsedCount; j++) {
      if (bitmapUsed[j] == inodeUsedBlockNum) {
        isInodeUsedBlockInBitmap = true;
        break;
      }
    }
    if (!isInodeUsedBlockInBitmap) {
      debugPrintf("ERROR: block %d not marked in bitmap\n", inodeUsedBlockNum);
      fprintf(stderr, ERROR5_INODE_NOT_MARKED);
      exit(1);
    }
  }
}

void test6(uint *inodeUsed, int inodeUsedCount, uint *bitmapUsed,
           int bitmapUsedCount) {
  for (int i = 29; i < bitmapUsedCount; i++) {
    bool isBitmapUsedByInode = false;
    uint bitmapUsedBlockNum = bitmapUsed[i];
    // debugPrintf("used bitmap data block %d\n", bitmapUsedBlockNum);
    for (int j = 0; j < inodeUsedCount; j++) {
      if (inodeUsed[j] == bitmapUsedBlockNum) {
        isBitmapUsedByInode = true;
        break;
      }
    }
    if (!isBitmapUsedByInode) {
      debugPrintf("ERROR: bitmap %d not ref'd by inode\n", bitmapUsedBlockNum);
      fprintf(stderr, ERROR6_BITMAP_NOT_MARKED);
      exit(1);
    }
  }
}