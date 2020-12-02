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

void test1(uint i, short type);
void test2(uint i, struct dinode *inodePtr);
void test3();
void test4(uint i, struct dinode *inodePtr);

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
  struct superblock *sBlock;
  sBlock = (struct superblock *)(fsPtr + BSIZE);
  debugPrintf("sBlock: size %d, # data blocks %d, # inodes %d \n", sBlock->size,
              sBlock->nblocks, sBlock->ninodes);

  // Traverse inodes - start at second block
  struct dinode *inodePtr = (struct dinode *)(fsPtr + (2 * BSIZE));
  for (uint i = 0; i < sBlock->ninodes; i++, inodePtr++) {
    // Test 1 - each inode is either unallocated or one of the valid types
    test1(i, inodePtr->type);

    // Test 2 - for inuse inodes; points to valid datablock address
    test2(i, inodePtr);

    // Test 4 - each dir contains . and ..; . points to dir itself
    test4(i, inodePtr);
  }

  // Test 3 - root dir exists; inode num 1; parent is itself
  test3();

  // figure out bitmap

  // do rest of P5

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
    if (!(dblockNum == 0 || ((dblockNum >= 29) && (dblockNum < 995 + 28)))) {
      debugPrintf("\nERROR: inode %d: dblockNum: %d\n", i, dblockNum);
      fprintf(stderr, ERROR2_BAD_DIRECT_DATA);
      exit(1);
    }
  }
  // debugPrintf("\n");

  // Indirect block number invalid
  uint indirectBlockNum = inodePtr->addrs[NDIRECT];
  if (!(indirectBlockNum == 0 ||
        ((indirectBlockNum >= 29) && (indirectBlockNum < 995 + 28)))) {
    debugPrintf("ERROR: inode %d: indirectBlockNum %d \n", i, indirectBlockNum);
    fprintf(stderr, ERROR2_BAD_INDIRECT_DATA);
    exit(1);
  }
  if (indirectBlockNum == 0) {
    return;
  }

  // Traverse indirect blocks for invalid datablock address
  uint *indirectAddr = (uint *)(fsPtr + (indirectBlockNum * BSIZE));
  for (int j = 0; j < 128; j++, indirectAddr++) {
    uint dblockNum = *indirectAddr;
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
      debugPrintf("inode %d: dir->inum %d, dir->name %s\n", i, dirEntPtr->inum,
                  dirEntPtr->name);
      isDirUsed = true;

      if (strcmp(dirEntPtr->name, ".") == 0) {
        foundCurrDot = true;
        debugPrintf("found current dot\n");
        if (dirEntPtr->inum != i) {
          fprintf(stderr, ERROR4_BAD_DIR_FORMAT);
          exit(1);
        }
        continue;
      } else if (strcmp(dirEntPtr->name, "..") == 0) {
        debugPrintf("found parent dot\n");
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