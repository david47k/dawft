CC=clang
GCC=gcc
FLAGS = -s -O2 -std=c99 -Wall -Wextra -Wpedantic
#WIN32CC=i686-w64-mingw32-gcc
#WIN64CC=x86_64-w64-mingw32-gcc
SRCFILES = bmp.c dawft.c
EXE = dawft
TARGETS = $(EXE)

default: release

release: $(SRCFILES)
	$(CC) $(CFLAGS) $^ -o $(EXE)

debug: $(SRCFILES)
	$(CC) -std=c99 -Weverything $^ -o $(EXE)

safer: $(SRCFILES)
	$(CC) -std=c99 -Weverything -fsanitize=address -fno-omit-frame-pointer -O1 $^ -o $(EXE)

safergcc: $(SRCFILES)
	$(GCC) $(CFLAGS) -D_FORTIFY_SOURCE=2 $^ -o $(EXE)

#win-x86: $(SRCFILES)
#	$(WIN32CC) $(CFLAGS) $^ -o $(EXE).x86.exe

#win-x64: $(SRCFILES)
#	$(WIN64CC) $(CFLAGS) $^ -o $(EXE).x64.exe

clean:
	rm $(TARGETS)
