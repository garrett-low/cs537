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
#define ERROR9_INODE_NOT_FOUND \
  "ERROR: inode marked use but not found in a directory.\n"
#define ERROR10_INODE_REF_MARKED_FREE \
  "ERROR: inode referred to in directory but marked free.\n"
#define ERROR11_INODE_REF_COUNT "ERROR: bad reference count for file.\n"

#define DBLOCK_UNUSED 0
#define DBLOCK_DIRECT 1
#define DBLOCK_INDIRECT 2

void *fs;
struct superblock *sBlock;
uint bitBlocks;  // initial bit blocks
uint hdrBlocks;  // initial header blocks

// Test and Setup functions
void test1(uint i, short type);
void test2(uint i, struct dinode *iPtr);
void test3();
void test4(uint i, struct dinode *iPtr);

void test5(uint *iUsed, int iUsedCount, uint *bUsed, int bUsedCount);
void test6(uint *iUsed, int iUsedCount, uint *bUsed, int bUsedCount);
void collectBitmapUsed(uint *bUsed, uint *bUsedCount);
void collectInodeUsed(struct dinode *iPtr, uint *iUsed, uint *iUsedCount,
                      short *isDirect);

void test78(uint *iUsed, int iUsedCount, short *isDirect);
void test9(struct dinode *iPtr);

void test10(struct dinode *iPtr);
void checkRefInodeIsUsed(uint refInum);

void test11(uint *iRefsCount);
void countInodeRefs(uint i, struct dinode *iPtr, uint *iRefsCount);

// Utility functions
struct dinode *getInode(uint inum);
uint* getIndBlockNum(uint indBlockNum);
struct dirent* getDirEnt(uint blockNum);
void debugPrintUsedBlocks(uint i, struct dinode *iPtr);
void debugPrintUsedBlocks2(uint iUsedCount, uint bUsedCount, uint *iUsed,
                           uint *bUsed);

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
  fs = mmap(NULL, fsStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (fs == MAP_FAILED) {
    fprintf(stderr, "Failed to map memory to store FS file.\n");
    exit(1);
  }

  // Superblock info
  sBlock = (struct superblock *)(fs + BSIZE);
  debugPrintf("sBlock: size %d, # data blocks %d, # inodes %d \n", sBlock->size,
              sBlock->nblocks, sBlock->ninodes);
  bitBlocks = sBlock->size / (BSIZE * 8) + 1;
  hdrBlocks = sBlock->ninodes / IPB + 3 + bitBlocks;

  // Initialize inode used blocks array
  uint iUsed[sBlock->nblocks];
  uint iUsedCount = 0;
  for (int i = 0; i < sBlock->nblocks; i++) {
    iUsed[i] = 0;
  }
  // Initialize array to track data type
  short isDirect[sBlock->nblocks];
  for (int i = 0; i < sBlock->nblocks; i++) {
    isDirect[i] = DBLOCK_UNUSED;
  }
  // Initialize array to count how many times inode referenced in dirs
  uint iRefsCount[sBlock->ninodes];
  for (int i = 0; i < sBlock->ninodes; i++) {
    iRefsCount[i] = 0;
  }

  // Traverse inodes
  struct dinode *iPtr = (struct dinode *)(fs + (2 * BSIZE));
  for (uint i = 0; i < sBlock->ninodes; i++, iPtr++) {
    // Test 1 - each inode is either unallocated or one of the valid types
    test1(i, iPtr->type);

    // Test 2 - for inuse inodes; points to valid datablock address
    test2(i, iPtr);

    // Test 4 - each dir contains . and ..; . points to dir itself
    test4(i, iPtr);

    // Test 5 and 6 Setup
    collectInodeUsed(iPtr, iUsed, &iUsedCount, isDirect);

    // Test 9
    test9(iPtr);

    // Test 10
    test10(iPtr);

    // Test 11 Setup
    countInodeRefs(i, iPtr, iRefsCount);

    // debugPrintUsedBlocks(i, iPtr);
  }

  // Test 3 - root dir exists; inode num 1; parent is itself
  test3();

  // Initialize bitmap used blocks array
  int bUsed[sBlock->size];
  for (int i = 0; i < sBlock->size; i++) {
    bUsed[i] = 0;
  }
  uint bUsedCount = 0;
  collectBitmapUsed(bUsed, &bUsedCount);

  // Test 5 - check inode data blocks against bitmap
  test5(iUsed, iUsedCount, bUsed, bUsedCount);

  // Test 6 - check bitmap against inode data blocks
  test6(iUsed, iUsedCount, bUsed, bUsedCount);
  // TODO test 6 is causing goodlarge to fail

  test78(iUsed, iUsedCount, isDirect);

  // Test 11
  test11(iRefsCount);

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
    if (!(dblockNum == 0 ||
          ((dblockNum >= hdrBlocks) && (dblockNum < sBlock->size)))) {
      debugPrintf("\nERROR: inode %d: dblockNum: %d\n", i, dblockNum);
      fprintf(stderr, ERROR2_BAD_DIRECT_DATA);
      exit(1);
    }
  }
  // debugPrintf("\n");

  // Indirect block number invalid
  uint indBlock = iPtr->addrs[NDIRECT];
  if (!(indBlock == 0 || ((indBlock >= hdrBlocks) &&
                                  (indBlock < sBlock->size)))) {
    debugPrintf("ERROR: inode %d: indBlock %d \n", i, indBlock);
    fprintf(stderr, ERROR2_BAD_INDIRECT_DATA);
    exit(1);
  }
  if (indBlock == 0) {
    return;
  }

  // Traverse indirect blocks for invalid datablock address
  uint *dBlock = getIndBlockNum(indBlock);
  for (int j = 0; j < NINDIRECT; j++) {
    uint dblockNum = dBlock[j];
    if (!(dblockNum == 0 ||
          ((dblockNum >= hdrBlocks) && (dblockNum < sBlock->size)))) {
      fprintf(stderr, ERROR2_BAD_INDIRECT_DATA);
      exit(1);
    }
  }
}

// Test 3 - root dir exists; inode num 1; parent is itself
void test3() {
  struct dinode *rootIPtr =
      (struct dinode *)(fs + (2 * BSIZE) + (1 * sizeof(struct dinode)));
  if (rootIPtr->type != T_DIR) {
    debugPrintf("ERROR: type of inum 1 (root) not T_DIR\n");
    fprintf(stderr, ERROR3_BAD_ROOT);
    exit(1);
  }
  struct dirent *rootDir = getDirEnt(rootIPtr->addrs[0]);
  if (!(rootDir->inum == 1 && strcmp(rootDir->name, ".") == 0)) {
    debugPrintf("ERROR: root not inum 1\n");
    fprintf(stderr, ERROR3_BAD_ROOT);
    exit(1);
  }
  rootDir++;
  if (!(rootDir->inum == 1 && strcmp(rootDir->name, "..") == 0)) {
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
  // Traverse direct blocks (assume . and .. are not in indirect blocks)
  for (int j = 0; j < NDIRECT; j++) {
    // Traverse directory entries
    struct dirent *dirEntPtr = getDirEnt(iPtr->addrs[j]);
    for (int k = 0; k < MAXDIR_PER_BLOCK; k++, dirEntPtr++) {
      if (foundCurrDot && foundParentDot) {
        break;
      }
      if (dirEntPtr->inum == 0) { // empty
        continue;
      }
      isDirUsed = true;

      if (strcmp(dirEntPtr->name, ".") == 0) {
        foundCurrDot = true;
        if (dirEntPtr->inum != i) { // . does not point to itself
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

void collectInodeUsed(struct dinode *iPtr, uint *iUsed, uint *iUsedCount,
                      short *isDirect) {
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
    uint *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
    for (int j = 0; j < NINDIRECT; j++) {
      if (dBlock[j] != 0) {
        iUsed[*iUsedCount] = dBlock[j];
        isDirect[*iUsedCount] = DBLOCK_INDIRECT;
        (*iUsedCount)++;
      }
    }
  }
}

void collectBitmapUsed(uint *bUsed, uint *bUsedCount) {
  unsigned char *bitmap =
      (unsigned char *)(fs + (BBLOCK(0, sBlock->ninodes) * BSIZE));
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

void test5(uint *iUsed, int iUsedCount, uint *bUsed, int bUsedCount) {
  for (int i = 0; i < iUsedCount; i++) {
    bool isInodeUsedBlockInBitmap = false;
    uint iUsedBlockNum = iUsed[i];
    // debugPrintf("used inode data block %d\n", iUsedBlockNum);
    for (int j = hdrBlocks; j < bUsedCount; j++) {
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

void test6(uint *iUsed, int iUsedCount, uint *bUsed, int bUsedCount) {
  for (int i = hdrBlocks; i < bUsedCount; i++) {
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

void debugPrintUsedBlocks(uint i, struct dinode *iPtr) {
  if (iPtr->type == T_DIR) {
    debugPrintf("inode %d is a dir of size %d with links %d\n", i, iPtr->size,
                iPtr->nlink);
    for (int j = 0; j < NDIRECT; j++) {
      if (iPtr->addrs[j] != 0) {
        debugPrintf("  dblock %d used\n", iPtr->addrs[j]);
      }
    }

    if (iPtr->addrs[NDIRECT]) {
      debugPrintf("  dblock %d used\n", iPtr->addrs[NDIRECT]);
      uint *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
      for (int j = 0; j < NINDIRECT; j++) {
        if (dBlock[j] != 0) {
          debugPrintf("  dblock %d used\n", dBlock[j]);
        }
      }
    }
  }
  if (iPtr->type == T_FILE) {
    debugPrintf("inode %d is a file of size %d with links %d\n", i, iPtr->size,
                iPtr->nlink);
    for (int j = 0; j < NDIRECT; j++) {
      if (iPtr->addrs[j] != 0) {
        debugPrintf("  dblock %d used\n", iPtr->addrs[j]);
      }
    }

    if (iPtr->addrs[NDIRECT]) {
      debugPrintf("  dblock %d used\n", iPtr->addrs[NDIRECT]);
      uint *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
      for (int j = 0; j < NINDIRECT; j++) {
        if (dBlock[j] != 0) {
          debugPrintf("  dblock %d used\n", dBlock[j]);
        }
      }
    }
  }
}

void test9(struct dinode *iPtr) {
  if (!(iPtr->type == T_DIR || iPtr->type == T_FILE || iPtr->type == T_DEV)) {
    return;
  }
  if (iPtr->nlink <= 0) {
    fprintf(stderr, ERROR9_INODE_NOT_FOUND);
    exit(1);
  }
}

void test10(struct dinode *iPtr) {
  if (iPtr->type != T_DIR) {
    return;
  }

  // Traverse direct blocks
  for (int j = 0; j < NDIRECT; j++) {
    if (iPtr->addrs[j] != 0) {
      // Traverse directory entry list
      struct dirent *dirEntPtr = getDirEnt(iPtr->addrs[j]);
      for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
        uint refInum = dirEntPtr[k].inum;
        checkRefInodeIsUsed(refInum);
      }
    }
  }  // direct

  // Traverse indirect blocks
  if (iPtr->addrs[NDIRECT]) {
    uint *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
    for (int j = 0; j < NINDIRECT; j++) {
      if (dBlock[j] != 0) {
        // Traverse directory entry list
        struct dirent *dirEntPtr = getDirEnt(dBlock[j]);
        for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
          uint refInum = dirEntPtr[k].inum;
          checkRefInodeIsUsed(refInum);
        }
      }
    }
  }  // indirect
}

void checkRefInodeIsUsed(uint refInum) {
  if (!refInum) {
    return;
  }
  struct dinode *matchInode = getInode(refInum);
  if (matchInode->type == 0) {
    fprintf(stderr, ERROR10_INODE_REF_MARKED_FREE);
    exit(1);
  }
}

struct dinode *getInode(uint inum) {
  return (struct dinode *)(fs + (2 * BSIZE) + (inum * sizeof(struct dinode)));
}

void countInodeRefs(uint i, struct dinode *iPtr, uint *iRefsCount) {
  if (iPtr->type != T_DIR) {
    return;
  }

  // Traverse direct blocks
  for (int j = 0; j < NDIRECT; j++) {
    if (iPtr->addrs[j] != 0) {
      // Traverse directory entry list
      struct dirent *dirEntPtr = getDirEnt(iPtr->addrs[j]);
      for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
        uint refInum = dirEntPtr[k].inum;
        if (refInum == 0) {
          continue;
        }
        struct dinode *ip = getInode(refInum);
        iRefsCount[refInum] += 1;
        debugPrintf("dir inode %d: name %s inum %d\n", i, dirEntPtr[k].name, dirEntPtr[k].inum);
      }
    }
  }  // direct

  // Traverse indirect blocks
  if (iPtr->addrs[NDIRECT] != 0) {
    uint *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
    for (int j = 0; j < NINDIRECT; j++) {
      if (dBlock[j] != 0) {
        // Traverse directory entry list
        struct dirent *dirEntPtr = getDirEnt(dBlock[j]);
        for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
          uint refInum = dirEntPtr[k].inum;
          if (refInum == 0) {
            continue;
          }
          struct dinode *ip = getInode(refInum);
          iRefsCount[refInum] += 1;
          debugPrintf("dir inode %d: name %s inum %d\n", i, dirEntPtr[k].name, dirEntPtr[k].inum);
        }
      }
    }
  }  // indirect
}

void debugPrintUsedBlocks2(uint iUsedCount, uint bUsedCount, uint *iUsed,
                           uint *bUsed) {
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
}

void test11(uint *iRefsCount) {
  for (uint i = 0; i < sBlock->ninodes; i++) {
    struct dinode *iPtr = getInode(i);
    if (!(iPtr->type == T_FILE)) {
      continue;
    }

    if (iPtr->nlink != iRefsCount[i]) {
      fprintf(stderr, ERROR11_INODE_REF_COUNT);
      exit(1);
    }
  }
}

struct dirent* getDirEnt(uint blockNum) {
  return (struct dirent *)(fs + (blockNum * BSIZE));
}

uint* getIndBlockNum(uint indBlockNum) {
  return (uint *)(fs + (indBlockNum * BSIZE));
}