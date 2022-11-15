CC=gcc
CLANG=clang
CLANGFLAGS = -Weverything
WIN32CC=i686-w64-mingw32-gcc
WIN64CC=x86_64-w64-mingw32-gcc
CFLAGS = -s -Wall -Wpedantic
TARGETS = mywft

default: $(TARGETS)

mywft: mywft.c
	$(CC) $(CFLAGS) $^ -o $@

mywft.w32.exe: mywft.c
	$(WIN32CC) $(CFLAGS) $^ -o $@

mywft.w64.exe: mywft.c
	$(WIN64CC) $(CFLAGS) $^ -o $@

clang: mywft.c
	$(CLANG) $(CLANGFLAGS) $^ -o mywft

clean:
	rm $(TARGETS)
