CC=clang
GCC=gcc
CFLAGS = -s -O2 -std=c11 -Wall -Wextra -Wpedantic
#WIN32CC=i686-w64-mingw32-gcc
#WIN64CC=x86_64-w64-mingw32-gcc
EXE = mywft
TARGETS = $(EXE)

default: release

release: mywft.c
	$(CC) $(CFLAGS) $^ -o $(EXE)

debug: mywft.c
	$(CC) -std=c11 -Weverything $^ -o $(EXE)

safer: mywft.c
	$(CC) -std=c11 -Weverything -fsanitize=address -fno-omit-frame-pointer -O1 $^ -o $(EXE)

safergcc: mywft.c
	$(GCC) $(CFLAGS) -D_FORTIFY_SOURCE=2 $^ -o $(EXE)

#mywft.w32.exe: mywft.c
#	$(WIN32CC) $(CFLAGS) $^ -o $@

#mywft.w64.exe: mywft.c
#	$(WIN64CC) $(CFLAGS) $^ -o $@

clean:
	rm $(TARGETS)
