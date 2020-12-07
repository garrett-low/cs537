# 5A: File System Checker

1. Test 1
    - Loop through inodes and check type.

2. Test 2
    - Loop through inodes and traverse addrs array. 
    - Ensure values are within the data block inode numbers.

3. Test 3
    - Traverse directory entries for inode 1. 
    - Assumes current and parent directory are in the first data block.
    - Check that ".." points to itself (inum 1).

4. Test 4
    - Loop through dir inodes.
    - Each dir contains "." and "..".
    - Check that "." points to itself.
  
5. Test 5
   - collect list of blocks marked as used by bitmap
   - collect list of blocked used by inodes
   - blocks used by inodes but not in bitmap -> fail test 5
  
6. Test 6
   - blocks marked as used by bitmap but not in inodes -> fail test 6

7. Test 7
   - Loop through direct blocks of each inode
   - No repeats

8. Test 8
   - Loop through indirect blocks of each inode
   - No repeats
  
9. Test 9
   - Loop through all in-use inodes and ensure that nlink is > 0
  
10. Test 10
    - Traverse all dir inodes.
    - Traverse direct and indirect blocks.
    - Traverse directory entries.
    - Get inum from each directory entry.
    - Get inode from inum.
    - Ensure inode is in use.
  
11. Test 11
    - Collect count of references for all inodes, not including "." and ".."
      - Traverse all dir inodes.
      - Traverse direct and indirect blocks.
      - Traverse directory entries.
      - Add to iRefsCount array that counts inode references, unless not in-use, ".", or ".."
    - Loop through file inodes and ensure nlink equals ref count

12. Test 12
    - Use reference count array from test 11 setup
    - Traverse all dir inodes and ensure ref count <= 1 for all
    - Used <= 1 to avoid conflict with Loop test (?)

## Extra Credit

13. Test 13 - Mismatch
    - Traverse through dir inodes
    - Assume parent dir entry is in 1st data block and is 2nd dir entry
    - Get parent inum
    - Using parent inum, get parent inode
    - Loop through data blocks of parent inode
    - Loop through dir entries of parent inode and find child
    - If cannot find child, then fail

14. Test 14 - Loop
    - Traverse through dir inodes
    - Use recursive traverseUpDir() function to make sure each dir traces back to
    root
    - If does not reach root after ninodes steps, then fail

15. Repair
    - Use open() in O_RDWR mode
    - Use mmap() in PROT_READ | PROT_WRITE, MAP_SHARED mode
    - Use msync() to write back to file
    - Detect orphan inodes by using test 10 check (bad reference count)
    - Find lost_found dir inum
    - Set orphan node nlink to 1
    - Set parent of orphan node to lost_found dir
    - Add orphan node to lost_found dir entries