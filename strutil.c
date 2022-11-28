// strutil.c

#include <stdbool.h>
#include <stdint.h>
#include "strutil.h"

// Non-destructive string token finder.
// Tokens are seperated by any amount of ' ' or \t.
// Tokens are printable ascii.
// Anything else ends the string.
// Maximum 8 tokens read.
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
				t->count++;
				wasSpace = false;
			}
		} else {
			finished = true;
		}
	}
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
