CC = gcc
CFLAGS = -Wall -Werror
DEPS = xv6_fsck.h
OBJ = xv6_fsck.o

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< $(DEBUG)

xv6_fsck: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

debug: DEBUG = -D DEBUG

debug: xv6_fsck

clean: 
	rm -f $(OBJ) $(TARGET) *~