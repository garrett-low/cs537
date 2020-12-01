xv6_fsck: xv6_fsck.c
	gcc -o xv6_fsck xv6_fsck.c -Werror $(DEBUG)
	
debug: DEBUG = -D DEBUG

debug: xv6_fsck
	
clean: 
	rm xv6_fsck