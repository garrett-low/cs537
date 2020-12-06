#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#include "types.h"
#include "fs.h"

#define LOST_FOUND_INO  0x1D

void ASSERT(int cond, char *msg) {
	if (!cond) {
		fprintf(stderr, "%s\n", msg);
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	int fd = open(argv[1], O_RDONLY);
	ASSERT(fd > 0, "image not found.");	// Nonexistant

	struct stat sbuf;
	ASSERT(fstat(fd, &sbuf) == 0, "image not found.");

	void *img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	ASSERT(img_ptr != NULL, "mmap failed.");

	struct dinode *dip = (struct dinode *)(img_ptr + 2 * BSIZE);


	ushort lostinum[100];
	memset(lostinum, 0, sizeof(lostinum));
	lostinum[0x05] = 1;

	struct dirent *entry = (struct dirent *)
                            (img_ptr + dip[LOST_FOUND_INO].addrs[0] * BSIZE);
    char *end = (char *)entry + BSIZE;
    for (entry = entry + 2; (char *)entry < end; entry++) {
        if (entry->inum == 0)
            continue;
        lostinum[entry->inum]--;
    }

	for (int i = 0; i < 100; i++) {
		ASSERT(lostinum[i] == 0, "ERROR");
	}
	
	printf("PASSED\n");
	return 0;
}
