CC=clang
GCC=gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic
WIN32CC=i686-w64-mingw32-gcc
WIN64CC=x86_64-w64-mingw32-gcc
SRCFILES = bmp.c strutil.c dawft.c
EXE = dawft
TARGETS = $(EXE) $(EXE).x86.exe $(EXE).x64.exe

default: debug

release: $(SRCFILES)
	$(CC) $(CFLAGS) -s -O2 $^ -o $(EXE)

release-gcc: $(SRCFILES)
	$(GCC) $(CFLAGS) -s -O2 $^ -o $(EXE)

debug: $(SRCFILES)
	$(CC) -g -Og -std=c99 -Weverything -fsanitize=address -fno-omit-frame-pointer $^ -o $(EXE)

debug-gcc: $(SRCFILES)
	$(GCC) $(CFLAGS) -g -Og -D_FORTIFY_SOURCE=2 $^ -o $(EXE)

win: $(SRCFILES)
#	$(WIN32CC) $(CFLAGS) -DWINDOWS $^ -o $(EXE).x86.exe
	$(WIN64CC) $(CFLAGS) -DWINDOWS $^ -o $(EXE).x64.exe

clean:
	rm $(TARGETS)
