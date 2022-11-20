CC=clang
GCC=gcc
CFLAGS = -s -O2 -std=c11 -Wall -Wextra -Wpedantic
DEBUGFLAGS = -std=c11 -Weverything
#WIN32CC=i686-w64-mingw32-gcc
#WIN64CC=x86_64-w64-mingw32-gcc
TARGETS = mywft

default: $(TARGETS)

mywft: mywft.c
	$(CC) $(CFLAGS) $^ -o $@

gcc: mywft.c
	$(GCC) $(CFLAGS) $^ -o mywft

debug: mywft.c
	$(CC) $(DEBUGFLAGS) $^ -o mywft

#mywft.w32.exe: mywft.c
#	$(WIN32CC) $(CFLAGS) $^ -o $@

#mywft.w64.exe: mywft.c
#	$(WIN64CC) $(CFLAGS) $^ -o $@

clean:
	rm $(TARGETS)
