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
#define ERROR7_DIRECT_UNIQUE "ERROR: direct address used more than once.\n"
#define ERROR8_DIRECT_UNIQUE "ERROR: indirect address used more than once.\n"

#define DBLOCK_UNUSED 0
#define DBLOCK_DIRECT 1
#define DBLOCK_INDIRECT 2

void *fsPtr;
struct superblock *sBlock;
uint bitBlocks; // initial bit blocks
uint hdrBlocks; // initial header blocks

void test1(uint i, short type);
void test2(uint i, struct dinode *iPtr);
void test3();
void test4(uint i, struct dinode *iPtr);
void collectInodeUsed(struct dinode *iPtr, uint *iUsed,
                      int *iUsedCount, short *isDirect);
void collectBitmapUsed(uint *bUsed, int *bUsedCount);
void test5(uint *iUsed, int iUsedCount, uint *bUsed,
           int bUsedCount);
void test6(uint *iUsed, int iUsedCount, uint *bUsed,
           int bUsedCount);
void test78(uint *iUsed, int iUsedCount, short *isDirect);

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
  bitBlocks = sBlock->size / (BSIZE * 8) + 1;
  hdrBlocks = sBlock->ninodes / IPB + 3 + bitBlocks;

  // Traverse inodes - start at second block
  // Initialize array
  uint iUsed[sBlock->nblocks];
  int iUsedCount = 0;
  for (int i = 0; i < sBlock->nblocks; i++) {
    iUsed[i] = 0;
  }
  short isDirect[sBlock->nblocks];
  for (int i = 0; i < sBlock->nblocks; i++) {
    isDirect[i] = DBLOCK_UNUSED;
  }
  struct dinode *iPtr = (struct dinode *)(fsPtr + (2 * BSIZE));
  for (uint i = 0; i < sBlock->ninodes; i++, iPtr++) {
    // Test 1 - each inode is either unallocated or one of the valid types
    test1(i, iPtr->type);

    // Test 2 - for inuse inodes; points to valid datablock address
    test2(i, iPtr);

    // Test 4 - each dir contains . and ..; . points to dir itself
    test4(i, iPtr);

    // Setup for test 5 and 6
    collectInodeUsed(iPtr, iUsed, &iUsedCount, isDirect);
    
    
    // Test 9
    if (iPtr->type == T_DIR || iPtr->type == T_FILE || iPtr->type == T_DEV) {
      if (iPtr->nlink <= 0) {
        fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
        exit(1);
      }
    }
    
    
    if (iPtr->type == T_DIR) {
      debugPrintf("inode %d is a dir of size %d with links %d\n", i, iPtr->size, iPtr->nlink);
      for (int j = 0; j < NDIRECT; j++) {
        if (iPtr->addrs[j] != 0) {
          debugPrintf("  dblock %d used\n", iPtr->addrs[j]);
        }
      }

      if (iPtr->addrs[NDIRECT]) {
        debugPrintf("  dblock %d used\n", iPtr->addrs[NDIRECT]);
        uint *indirectAddr =
            (uint *)(fsPtr + (iPtr->addrs[NDIRECT] * BSIZE));
        for (int j = 0; j < NINDIRECT; j++) {
          if (indirectAddr[j] != 0) {
            debugPrintf("  dblock %d used\n", indirectAddr[j]);
          }
        }
      }
    }
    if (iPtr->type == T_FILE) {
      debugPrintf("inode %d is a file of size %d with links %d\n", i, iPtr->size, iPtr->nlink);
      for (int j = 0; j < NDIRECT; j++) {
        if (iPtr->addrs[j] != 0) {
          debugPrintf("  dblock %d used\n", iPtr->addrs[j]);
        }
      }

      if (iPtr->addrs[NDIRECT]) {
        debugPrintf("  dblock %d used\n", iPtr->addrs[NDIRECT]);
        uint *indirectAddr =
            (uint *)(fsPtr + (iPtr->addrs[NDIRECT] * BSIZE));
        for (int j = 0; j < NINDIRECT; j++) {
          if (indirectAddr[j] != 0) {
            debugPrintf("  dblock %d used\n", indirectAddr[j]);
          }
        }
      }
    }
    
  }

  // Test 3 - root dir exists; inode num 1; parent is itself
  test3();

  // Initialize array
  int bUsed[sBlock->size];
  for (int i = 0; i < sBlock->size; i++) {
    bUsed[i] = 0;
  }
  int bUsedCount = 0;
  collectBitmapUsed(bUsed, &bUsedCount);

  debugPrintf("inode block count: %d, bitmap block count: %d\n", iUsedCount,
              bUsedCount);
  debugPrintf("inode used data blocks: ");
  for (int i = 0; i < iUsedCount; i++) {
    debugPrintf("%d, ", iUsed[i]);
  }
  debugPrintf("\n");

  debugPrintf("bitmap used data blocks: ");
  for (int i = 0; i < bUsedCount; i++) {
    debugPrintf("%d, ", bUsed[i]);
  }
  debugPrintf("\n");

  // Test 5 - check inode data blocks against bitmap
  test5(iUsed, iUsedCount, bUsed, bUsedCount);

  // Test 6 - check bitmap against inode data blocks
  test6(iUsed, iUsedCount, bUsed, bUsedCount);
  // TODO test 6 is causing goodlarge to fail

  test78(iUsed, iUsedCount, isDirect);

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
void test2(uint i, struct dinode *iPtr) {
  if (iPtr->type != T_FILE) {
    return;
  }

  // Direct block number invalid
  // debugPrintf("file inode %d: ", i);
  for (int j = 0; j < NDIRECT; j++) {
    uint dblockNum = iPtr->addrs[j];
    // debugPrintf("dblockNum %d, ", dblockNum);
    if (!(dblockNum == 0 || ((dblockNum >= 29) && (dblockNum < sBlock->size)))) {
      debugPrintf("\nERROR: inode %d: dblockNum: %d\n", i, dblockNum);
      fprintf(stderr, ERROR2_BAD_DIRECT_DATA);
      exit(1);
    }
  }
  // debugPrintf("\n");

  // Indirect block number invalid
  uint indirectBlockNum = iPtr->addrs[NDIRECT];
  if (!(indirectBlockNum == 0 ||
        ((indirectBlockNum >= 29) && (indirectBlockNum < sBlock->size)))) {
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
    if (!(dblockNum == 0 || ((dblockNum >= 29) && (dblockNum < sBlock->size)))) {
      debugPrintf("inode %d: indirectBlockNum %d, dblockNum: %d \n", i,
                  indirectBlockNum, dblockNum);
      fprintf(stderr, ERROR2_BAD_INDIRECT_DATA);
      exit(1);
    }
  }
}

// Test 3 - root dir exists; inode num 1; parent is itself
void test3() {
  struct dinode *rootiPtr =
      (struct dinode *)(fsPtr + (2 * BSIZE) + (1 * sizeof(struct dinode)));
  if (rootiPtr->type != T_DIR) {
    debugPrintf("ERROR: type of inum 1 (root) not T_DIR\n");
    fprintf(stderr, ERROR3_BAD_ROOT);
    exit(1);
  }
  struct dirent *rootDirent =
      (struct dirent *)(fsPtr + (rootiPtr->addrs[0] * BSIZE));
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
void test4(uint i, struct dinode *iPtr) {
  if (iPtr->type == T_FILE || iPtr->type == T_DEV) {
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
        (struct dirent *)(fsPtr + (iPtr->addrs[j] * BSIZE));
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

void collectInodeUsed(struct dinode *iPtr, uint *iUsed,
                      int *iUsedCount, short *isDirect) {
  if (!(iPtr->type == T_DIR || iPtr->type == T_FILE)) {
    return;
  }

  for (int j = 0; j < NDIRECT; j++) {
    if (iPtr->addrs[j] != 0) {
      iUsed[*iUsedCount] = iPtr->addrs[j];
      isDirect[*iUsedCount] = DBLOCK_DIRECT;
      (*iUsedCount)++;
    }
  }

  if (iPtr->addrs[NDIRECT]) {
    iUsed[*iUsedCount] = iPtr->addrs[NDIRECT];
    isDirect[*iUsedCount] = DBLOCK_DIRECT;
    (*iUsedCount)++;
    uint *indirectAddr = (uint *)(fsPtr + (iPtr->addrs[NDIRECT] * BSIZE));
    for (int j = 0; j < NINDIRECT; j++) {
      if (indirectAddr[j] != 0) {
        iUsed[*iUsedCount] = indirectAddr[j];
        isDirect[*iUsedCount] = DBLOCK_INDIRECT;
        (*iUsedCount)++;
      }
    }
  }
}

void collectBitmapUsed(uint *bUsed, int *bUsedCount) {
  unsigned char *bitmap =
      (unsigned char *)(fsPtr + (BBLOCK(0, sBlock->ninodes) * BSIZE));
  int blockNum = 0;
  for (int i = 0; i < BSIZE; i++) {
    for (int j = 0; j < 8; j++, blockNum++) {
      uint bit = (bitmap[i] >> j) & 1;
      if (bit == 1) {
        bUsed[(*bUsedCount)++] = blockNum;
      }
    }
  }
}

void test5(uint *iUsed, int iUsedCount, uint *bUsed,
           int bUsedCount) {
  for (int i = 0; i < iUsedCount; i++) {
    bool isInodeUsedBlockInBitmap = false;
    uint iUsedBlockNum = iUsed[i];
    // debugPrintf("used inode data block %d\n", iUsedBlockNum);
    for (int j = 29; j < bUsedCount; j++) {
      if (bUsed[j] == iUsedBlockNum) {
        isInodeUsedBlockInBitmap = true;
        break;
      }
    }
    if (!isInodeUsedBlockInBitmap) {
      debugPrintf("ERROR: block %d not marked in bitmap\n", iUsedBlockNum);
      fprintf(stderr, ERROR5_INODE_NOT_MARKED);
      exit(1);
    }
  }
}

void test6(uint *iUsed, int iUsedCount, uint *bUsed,
           int bUsedCount) {
  for (int i = 29; i < bUsedCount; i++) {
    bool isBitmapUsedByInode = false;
    uint bUsedBlockNum = bUsed[i];
    // debugPrintf("used bitmap data block %d\n", bUsedBlockNum);
    for (int j = 0; j < iUsedCount; j++) {
      if (iUsed[j] == bUsedBlockNum) {
        isBitmapUsedByInode = true;
        break;
      }
    }
    if (!isBitmapUsedByInode) {
      debugPrintf("ERROR: bitmap %d not ref'd by inode\n", bUsedBlockNum);
      fprintf(stderr, ERROR6_BITMAP_NOT_MARKED);
      exit(1);
    }
  }
}

void test78(uint *iUsed, int iUsedCount, short *isDirect) {
  bool dblockUnique = true;
  for (int i = 0; i < iUsedCount; i++) {
    uint currBlockNum = iUsed[i];
    for (int j = 0; j < iUsedCount; j++) {
      if (i == j) {
        continue;
      } else {
        if (currBlockNum == iUsed[j]) {
          if (isDirect[i] == DBLOCK_DIRECT) {
            fprintf(stderr, ERROR7_DIRECT_UNIQUE);
            exit(1);
          } else if (isDirect[i] == DBLOCK_INDIRECT) {
            fprintf(stderr, ERROR8_DIRECT_UNIQUE);
            exit(1);
          }
        }
      }
    }
  }
}