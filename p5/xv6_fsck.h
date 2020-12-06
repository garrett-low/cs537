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
#define NINDIRECT (BSIZE / sizeof(int))
#define MAXFILE (NDIRECT + NINDIRECT)

// File system super block
struct superblock {
  int size;     // Size of file system image (blocks)
  int nblocks;  // Number of data blocks
  int ninodes;  // Number of inodes.
};

// Inode types
#define T_DIR 1   // Directory
#define T_FILE 2  // File
#define T_DEV 3   // Special device

// On-disk inode structure
struct dinode {
  short type;              // File type
  short major;             // Major device number (T_DEV only)
  short minor;             // Minor device number (T_DEV only)
  short nlink;             // Number of links to inode in file system
  int size;                // Size of file (bytes)
  int addrs[NDIRECT + 1];  // Data block addresses
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
  short inum;
  char name[DIRSIZ];
};

// P5 - My constants

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
#define ERROR12_DIR_ONCE \
  "ERROR: directory appears more than once in file system.\n"
#define EC1_PARENT_DIR_MISMATCH "ERROR: parent directory mismatch.\n"
#define EC2_ORPHAN_DIR "ERROR: inaccessible directory exists.\n"

#define DBLOCK_UNUSED 0
#define DBLOCK_DIRECT 1
#define DBLOCK_INDIRECT 2

// Test and Setup functions
void test1(int i, short type);
void test2(int i, struct dinode *iPtr);
void test3();
void test4(int i, struct dinode *iPtr);
void test5(int *iUsed, int iUsedCount, int *bUsed, int bUsedCount);
void test6(int *iUsed, int iUsedCount, int *bUsed, int bUsedCount);
void collectBitmapUsed(int *bUsed, int *bUsedCount);
void collectInodeUsed(struct dinode *iPtr, int *iUsed, int *iUsedCount,
                      short *isDirect);
void test78(int *iUsed, int iUsedCount, short *isDirect);
void test9(struct dinode *iPtr);
void test10(struct dinode *iPtr);
void checkRefInodeIsUsed(int refInum);
void test11(int *iRefsCount);
void countInodeRefs(int i, int *iRefsCount);
void countInodeRefsHelper(int *iRefsCount, int dBlock);
void test12(int *iRefsCount);
void test13();
void findChildInParent(struct dinode *parent, int childInum);
void test14();
void traverseUpDir(struct dinode *iPtr, int stepCount);
void checkMode(char* fsName);
void repairMode(char* fsName);
int getLostFoundDirInum();
void addToDir(int parentInum, int childInum);

// Utility functions
struct dinode *getInode(int inum);
int *getIndBlockNum(int indBlockNum);
struct dirent *getDirEnt(int blockNum);
int getParentInum(struct dinode *iPtr);
void debugPrintUsedBlocks(int i, struct dinode *iPtr);
void debugPrintUsedBlocks2(int iUsedCount, int bUsedCount, int *iUsed,
                           int *bUsed);