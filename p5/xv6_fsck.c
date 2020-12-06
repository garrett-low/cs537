#include "xv6_fsck.h"
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

void *fs;
struct superblock *sBlock;
int bitBlocks;  // initial bit blocks
int hdrBlocks;  // initial header blocks

int main(int argc, char *argv[]) {
  debugPrintf("constants: IPB %ld, BPB %d\n", IPB, BPB);

  // Invalid # of arguments
  if (!(argc == 2 || argc == 3)) {
    fprintf(stderr, USAGE_ERROR);
    exit(1);
  }

  char *fsName;
  if (argc == 2) {
    fsName = argv[1];
  } else {
    fsName = argv[2];
  }

  if (argc == 2) {
    checkMode(fsName);
  } else {
    repairMode(fsName);
  }

  return 0;
}

void checkMode(char *fsName) {
  // FS image does not exist
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
  int iUsed[sBlock->nblocks];
  int iUsedCount = 0;
  for (int i = 0; i < sBlock->nblocks; i++) {
    iUsed[i] = 0;
  }
  // Initialize array to track data type
  short isDirect[sBlock->nblocks];
  for (int i = 0; i < sBlock->nblocks; i++) {
    isDirect[i] = DBLOCK_UNUSED;
  }
  // Initialize array to count how many times inode referenced in dirs
  int iRefsCount[sBlock->ninodes];
  for (int i = 0; i < sBlock->ninodes; i++) {
    iRefsCount[i] = 0;
  }

  // Traverse inodes
  struct dinode *iPtr = (struct dinode *)(fs + (2 * BSIZE));
  for (int i = 0; i < sBlock->ninodes; i++, iPtr++) {
    test1(i, iPtr->type);  // Test 1 - each inode  unallocated or valid type
    test2(i, iPtr);        // Test 2 - for inuse inodes; points to valid dblock
    test4(i, iPtr);  // Test 4 - dir contains . and ..; . points to dir itself
    collectInodeUsed(iPtr, iUsed, &iUsedCount, isDirect);  // Test 5 and 6 Setup
    test9(iPtr);                    // inodes ref'd in at least one dir
    test10(iPtr);                   // ref'd inode is allocated
    countInodeRefs(i, iRefsCount);  // Test 11 Setup

    // debugPrintUsedBlocks(i, iPtr);
  }
  test3();  // Test 3 - root dir exists; inode num 1; parent is itself
  int bUsed[sBlock->size];  // Test 5 and 6 setup
  for (int i = 0; i < sBlock->size; i++) {
    bUsed[i] = 0;
  }
  int bUsedCount = 0;
  collectBitmapUsed(bUsed, &bUsedCount);
  test5(iUsed, iUsedCount, bUsed, bUsedCount);  // used blocks marked in bmap
  test6(iUsed, iUsedCount, bUsed, bUsedCount);  // used bmap ref'd by inode
  test78(iUsed, iUsedCount, isDirect);  // blocks only ref'd by one inode
  test11(iRefsCount);                   // nlinks matches ref count
  test12(iRefsCount);                   // dir linked once
  test13();
  test14();
}

// Test 1 - each inode is either unallocated or one of the valid types
void test1(int i, short type) {
  if (!(type == 0 || type == T_FILE || type == T_DIR || type == T_DEV)) {
    debugPrintf("ERROR: inode %d, type %d\n", i, type);
    fprintf(stderr, ERROR1_BAD_INODE);
    exit(1);
  }
}

// Test 2 - for inuse inodes; points to valid datablock address
void test2(int i, struct dinode *iPtr) {
  if (iPtr->type != T_FILE) {
    return;
  }

  // Direct block number invalid
  // debugPrintf("file inode %d: ", i);
  for (int j = 0; j < NDIRECT; j++) {
    int dblockNum = iPtr->addrs[j];
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
  int indBlock = iPtr->addrs[NDIRECT];
  if (!(indBlock == 0 ||
        ((indBlock >= hdrBlocks) && (indBlock < sBlock->size)))) {
    debugPrintf("ERROR: inode %d: indBlock %d \n", i, indBlock);
    fprintf(stderr, ERROR2_BAD_INDIRECT_DATA);
    exit(1);
  }
  if (indBlock == 0) {
    return;
  }

  // Traverse indirect blocks for invalid datablock address
  int *dBlock = getIndBlockNum(indBlock);
  for (int j = 0; j < NINDIRECT; j++) {
    int dblockNum = dBlock[j];
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
void test4(int i, struct dinode *iPtr) {
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
      if (dirEntPtr->inum == 0) {  // empty
        continue;
      }
      isDirUsed = true;

      if (strcmp(dirEntPtr->name, ".") == 0) {
        foundCurrDot = true;
        if (dirEntPtr->inum != i) {  // . does not point to itself
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

void collectInodeUsed(struct dinode *iPtr, int *iUsed, int *iUsedCount,
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
    int *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
    for (int j = 0; j < NINDIRECT; j++) {
      if (dBlock[j] != 0) {
        iUsed[*iUsedCount] = dBlock[j];
        isDirect[*iUsedCount] = DBLOCK_INDIRECT;
        (*iUsedCount)++;
      }
    }
  }
}

void collectBitmapUsed(int *bUsed, int *bUsedCount) {
  unsigned char *bitmap =
      (unsigned char *)(fs + (BBLOCK(0, sBlock->ninodes) * BSIZE));
  int blockNum = 0;
  for (int i = 0; i < BSIZE; i++) {
    for (int j = 0; j < 8; j++, blockNum++) {
      int bit = (bitmap[i] >> j) & 1;
      if (bit == 1) {
        bUsed[(*bUsedCount)++] = blockNum;
      }
    }
  }
}

// Test 5 - check inode data blocks against bitmap
void test5(int *iUsed, int iUsedCount, int *bUsed, int bUsedCount) {
  for (int i = 0; i < iUsedCount; i++) {
    bool isInodeUsedBlockInBitmap = false;
    int iUsedBlockNum = iUsed[i];
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

void test6(int *iUsed, int iUsedCount, int *bUsed, int bUsedCount) {
  for (int i = hdrBlocks; i < bUsedCount; i++) {
    bool isBitmapUsedByInode = false;
    int bUsedBlockNum = bUsed[i];
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

void test78(int *iUsed, int iUsedCount, short *isDirect) {
  for (int i = 0; i < iUsedCount; i++) {
    int currBlockNum = iUsed[i];
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

void debugPrintUsedBlocks(int i, struct dinode *iPtr) {
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
      int *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
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
      int *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
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
        int refInum = dirEntPtr[k].inum;
        checkRefInodeIsUsed(refInum);
      }
    }
  }  // direct

  // Traverse indirect blocks
  if (iPtr->addrs[NDIRECT]) {
    int *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
    for (int j = 0; j < NINDIRECT; j++) {
      if (dBlock[j] != 0) {
        // Traverse directory entry list
        struct dirent *dirEntPtr = getDirEnt(dBlock[j]);
        for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
          int refInum = dirEntPtr[k].inum;
          checkRefInodeIsUsed(refInum);
        }
      }
    }
  }  // indirect
}

void checkRefInodeIsUsed(int refInum) {
  if (!refInum) {
    return;
  }
  struct dinode *matchInode = getInode(refInum);
  if (matchInode->type == 0) {
    fprintf(stderr, ERROR10_INODE_REF_MARKED_FREE);
    exit(1);
  }
}

struct dinode *getInode(int inum) {
  return (struct dinode *)(fs + (2 * BSIZE) + (inum * sizeof(struct dinode)));
}

void countInodeRefs(int i, int *iRefsCount) {
  struct dinode *iPtr = getInode(i);
  if (iPtr->type != T_DIR) {
    return;
  }

  // Traverse direct blocks
  for (int j = 0; j < NDIRECT; j++) {
    if (iPtr->addrs[j] != 0) {
      countInodeRefsHelper(iRefsCount, iPtr->addrs[j]);
    }
  }  // direct

  // Traverse indirect blocks
  if (iPtr->addrs[NDIRECT] != 0) {
    int *dBlock = getIndBlockNum(iPtr->addrs[NDIRECT]);
    for (int j = 0; j < NINDIRECT; j++) {
      if (dBlock[j] != 0) {
        countInodeRefsHelper(iRefsCount, dBlock[j]);
      }
    }
  }  // indirect
}

// Traverse directory entry list and count references
void countInodeRefsHelper(int *iRefsCount, int dBlock) {
  struct dirent *dirEntPtr = getDirEnt(dBlock);
  for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
    int refInum = dirEntPtr[k].inum;
    if (refInum == 0) {
      continue;
    }
    // Do not include . and .. as references
    if (strcmp(dirEntPtr[k].name, ".") == 0 ||
        strcmp(dirEntPtr[k].name, "..") == 0) {
      continue;
    }
    iRefsCount[refInum] += 1;
  }
}

void debugPrintUsedBlocks2(int iUsedCount, int bUsedCount, int *iUsed,
                           int *bUsed) {
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

void test11(int *iRefsCount) {
  for (int i = 0; i < sBlock->ninodes; i++) {
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

struct dirent *getDirEnt(int blockNum) {
  return (struct dirent *)(fs + (blockNum * BSIZE));
}

int *getIndBlockNum(int indBlockNum) {
  return (int *)(fs + (indBlockNum * BSIZE));
}

void test12(int *iRefsCount) {
  for (int i = 0; i < sBlock->ninodes; i++) {
    struct dinode *iPtr = getInode(i);
    if (!(iPtr->type == T_DIR)) {
      continue;
    }

    if (iRefsCount[i] > 1) {
      fprintf(stderr, ERROR12_DIR_ONCE);
      exit(1);
    }
  }
}

void test13() {
  for (int i = 0; i < sBlock->ninodes; i++) {
    if (getInode(i)->type != T_DIR) {
      continue;
    }
    int parentInum = getParentInum(getInode(i));
    debugPrintf("inum %d's parent is %d\n", i, parentInum);
    findChildInParent(getInode(parentInum), i);
  }
}

int getParentInum(struct dinode *iPtr) {
  if (iPtr->type != T_DIR) {
    fprintf(stderr, EC1_PARENT_DIR_MISMATCH);
    exit(1);
  }

  struct dirent *rootDir = getDirEnt(iPtr->addrs[0]);
  struct dirent *parentDir = rootDir + 1;

  if (strcmp(parentDir->name, "..") != 0) {
    // debugPrintf("dir %d, name %s\n", parentDir->inum, parentDir->name);
    fprintf(stderr, EC1_PARENT_DIR_MISMATCH);
    exit(1);
  }

  // debugPrintf("parent: %s %d\n", parentDir->name, parentDir->inum);
  return parentDir->inum;
}

void findChildInParent(struct dinode *parent, int childInum) {
  if (parent->type != T_DIR) {
    fprintf(stderr, EC1_PARENT_DIR_MISMATCH);
    exit(1);
  }

  // debugPrintf("LOOKING FOR CHILD INUM %d\n", childInum);
  bool foundChild = false;

  // Traverse direct blocks
  for (int j = 0; j < NDIRECT; j++) {
    if (parent->addrs[j] != 0) {
      struct dirent *dirEntPtr = getDirEnt(parent->addrs[j]);
      for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
        // debugPrintf("inum %d, name %s\n", dirEntPtr[k].inum,
        // dirEntPtr[k].name); Look for child
        if (dirEntPtr[k].inum == childInum) {
          foundChild = true;
          break;
        }
      }
    }
  }  // direct

  // Traverse indirect blocks
  if (parent->addrs[NDIRECT] != 0) {
    int *dBlock = getIndBlockNum(parent->addrs[NDIRECT]);
    for (int j = 0; j < NINDIRECT; j++) {
      if (dBlock[j] != 0) {
        struct dirent *dirEntPtr = getDirEnt(dBlock[j]);
        for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
          // debugPrintf("inum %d, name %s\n", dirEntPtr[k].inum,
          //             dirEntPtr[k].name);
          // Look for child
          if (dirEntPtr[k].inum == childInum) {
            foundChild = true;
            break;
          }
        }
      }
    }
  }  // indirect

  if (!foundChild) {
    fprintf(stderr, EC1_PARENT_DIR_MISMATCH);
    exit(1);
  }
}

void test14() {
  for (int i = 0; i < sBlock->ninodes; i++) {
    if (getInode(i)->type != T_DIR) {
      continue;
    }
    traverseUpDir(getInode(i), 0);
  }
}

void traverseUpDir(struct dinode *iPtr, int stepCount) {
  if (iPtr->type != T_DIR) {
    fprintf(stderr, EC2_ORPHAN_DIR);
    exit(1);
  }
  struct dirent *rootDir = getDirEnt(iPtr->addrs[0]);
  struct dirent *parentDir = rootDir + 1;
  // debugPrintf("traverse() %d: rootDir %d \"%s\" parentDir %d \"%s\"\n",
  // stepCount, rootDir->inum, rootDir->name, parentDir->inum, parentDir->name);
  if (rootDir->inum == 1) {
    // debugPrintf("Found root\n");
    return;
  }
  if (stepCount > sBlock->ninodes) {
    fprintf(stderr, EC2_ORPHAN_DIR);
    exit(1);
  }

  return traverseUpDir(getInode(parentDir->inum), ++stepCount);
}

void repairMode(char *fsName) {
  // FS image does not exist
  int fd = open(fsName, O_RDWR);
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
  fs = mmap(NULL, fsStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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

  int lostFoundDir = getLostFoundDirInum();
  debugPrintf("lost_found inum %d\n", lostFoundDir);

  // Initialize array to count how many times inode referenced in dirs
  int iRefsCount[sBlock->ninodes];
  for (int i = 0; i < sBlock->ninodes; i++) {
    iRefsCount[i] = 0;
  }
  
  for (int i = 0; i < sBlock->ninodes; i++)
  {
    countInodeRefs(i, iRefsCount);  // Test 11 Setup
  }
  

  for (int i = 2; i < sBlock->ninodes; i++) {
    struct dinode *iPtr = getInode(i);
    // Copied from test9
    if (iPtr->type == 0) {
      continue;
    }
    // Perform repair
    if (iPtr->nlink != iRefsCount[i]) {
      debugPrintf("found lost inum %d\n", i);
      iPtr->nlink = 1;  // now ref'd by lost_found dir
      addToDir(lostFoundDir, i);
    }
  }
  
  if (msync(fs, fsStat.st_size, MS_SYNC) != 0) {
    fprintf(stderr, "ERROR: msync failed\n");
    exit(1);
  }
  
  close(fd);
}

int getLostFoundDirInum() {
  struct dinode *rootInode = getInode(1);  // assume lost_found in root dir
  struct dirent *rootDirEnt = getDirEnt(rootInode->addrs[0]);
  for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
    debugPrintf("getLost(): inum %i root %s\n", rootDirEnt[k].inum,
                rootDirEnt[k].name);
    if (strcmp(rootDirEnt[k].name, "lost_found") == 0) {
      return rootDirEnt[k].inum;
    }
  }

  fprintf(stderr, "ERROR: run in repair mode without lost_found dir\n");
  exit(1);
}

void addToDir(int parentInum, int childInum) {
  struct dinode *parent = getInode(parentInum);
  if (parent->type != T_DIR) {
    fprintf(stderr, "ERROR: New parent inode is not a directory\n");
    exit(1);
  }

  bool foundFree = false;

  // Traverse direct blocks
  for (int j = 0; j < NDIRECT; j++) {
    if (parent->addrs[j] != 0) {
      struct dirent *dirEntPtr = getDirEnt(parent->addrs[j]);
      for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
        // Find free entry
        if (dirEntPtr[k].inum == 0) {
          debugPrintf("hello1\n");
          foundFree = true;
          dirEntPtr[k].inum = childInum;
          char name[DIRSIZ];
          sprintf(name, "%d", childInum);
          strcpy(dirEntPtr[k].name, name);
          return;
        }
      }
    }
  }  // direct

  // Traverse indirect blocks
  if (parent->addrs[NDIRECT] != 0) {
    int *dBlock = getIndBlockNum(parent->addrs[NDIRECT]);
    for (int j = 0; j < NINDIRECT; j++) {
      if (dBlock[j] != 0) {
        struct dirent *dirEntPtr = getDirEnt(dBlock[j]);
        for (int k = 0; k < MAXDIR_PER_BLOCK; k++) {
          // Find free entry
          if (dirEntPtr[k].inum == 0) {
            debugPrintf("hello2\n");
            foundFree = true;
            dirEntPtr[k].inum = childInum;
            char name[DIRSIZ];
            sprintf(name, "%d", childInum);
            strcpy(dirEntPtr[k].name, name);
            return;
          }
        }
      }
    }
  }  // indirect

  if (!foundFree) {
    fprintf(stderr, "ERROR: directory is already full\n");
    exit(1);
  }
}