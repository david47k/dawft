CC=clang
GCC=gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic
#WIN32CC=i686-w64-mingw32-gcc
#WIN64CC=x86_64-w64-mingw32-gcc
SRCFILES = bmp.c strutil.c dawft.c
EXE = dawft
TARGETS = $(EXE)

default: debug

release: $(SRCFILES)
	$(CC) $(CFLAGS) -s -O2 $^ -o $(EXE)

debug: $(SRCFILES)
	$(CC) -std=c99 -Weverything -fsanitize=address -fno-omit-frame-pointer $^ -o $(EXE)

debugcc: $(SRCFILES)
	$(GCC) $(CFLAGS) -D_FORTIFY_SOURCE=2 $^ -o $(EXE)

#win-x86: $(SRCFILES)
#	$(WIN32CC) $(CFLAGS) $^ -o $(EXE).x86.exe

#win-x64: $(SRCFILES)
#	$(WIN64CC) $(CFLAGS) $^ -o $(EXE).x64.exe

clean:
	rm $(TARGETS)
