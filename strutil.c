// strutil.c

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "strutil.h"

// Non-destructive string token finder.
// Tokens are seperated by any amount of ' ' or \t.
// Tokens are printable ascii.
// Anything else ends the string.
// Maximum 10 tokens read.
// Pass in a c string and an (allocated) TokensIdx struct.
void getTokensIdx(char * s, TokensIdx * t) {
	t->count = 0;
	bool finished = false;
	bool wasSpace = true;
	for(uint32_t i=0; !finished; i++) {
		char c = s[i];
		if(c == ' ' || c == '\t') {
			wasSpace = true;
		} else if(c > 32 && c < 127) {
			if(wasSpace) {
				t->idx[t->count] = i;
				t->ptr[t->count] = &s[i];
                t->length[t->count] = 1;
				t->count++;
                if(t->count == 10) {
                   finished = true;
                }
				wasSpace = false;
			} else if(t->count > 0) {
                t->length[t->count - 1] += 1;
            }
		} else {
			finished = true;
		}       
	}
}

// return 1 if it is a hex or decimal unsigned integer readable by readNum
// return 0 otherwise
int isNum(char * s) {
    size_t len = strlen(s);
    if(len < 1) return 0;
    if(s[0] >= '0' && s[0] <= '9') return 1;
    return 0;
}

// read hex or decimal unsigned integer digits
// hex must start with 0x
// overflow is undefined
uint32_t readNum(char * s) {
    uint32_t total = 0;
    bool finished = false;

    // hex digits
	if(s[0] == '0' && s[1] == 'x') {
        for(uint32_t i=2; !finished && i<10; i++) {
            char c = s[i];
            uint8_t n = 0;
            if(c >= '0' && c <= '9') {
                n = c - '0';
            } else if(c >= 'A' && c <= 'F') {
                n = c - 'A' + 10;
            } else if(c >= 'a' && c <= 'f') {
                n = c - 'a' + 10;
            } else {
                finished = true;
            }
            if(!finished) {
                total <<= 4;
                total |= n;
            }
        }
    	return total;
    }

    // decimal digits
    for(uint32_t i=0; !finished && i<10; i++) {
        char c = s[i];
        uint8_t n = 0;
        if(c >= '0' && c <= '9') {
            n = c - '0';
        } else {
            finished = true;
        }
        if(!finished) {
            total *= 10;
            total += n;
        }
    }
    return total;
}

// Append src to end of dst string.
// Probably not compatible with other strlcat, but should be safe.
// dstSize is total size of dst buffer. Returns new length of dst. 
size_t d_strlcat(char * dst, const char * src, size_t dstSize) {
	size_t dstLen = strlen(dst);
	assert(dstLen < dstSize);		// dstLen shouldn't be bigger than dstSize, full stop. 
	if(dstLen >= (dstSize - 1)) {	// this will catch if the assert is optimised out.
		return dstLen;				// buffer already full.
	}
	size_t i = 0;
	while(src[i] != 0 && (dstLen + i) < (dstSize - 1)) {
		dst[dstLen + i] = src[i];
		i++;
	}
	dst[dstLen + i] = 0;
	return(dstLen + i);
}
