// strutil.h

typedef struct _TokensIdx {
	uint32_t count;			// number of tokens found
	uint32_t idx[10];	    // start index of each token
	char * ptr[10];  		// pointer to start of each token
	uint32_t length[10];  		// length of token in bytes or chars
} TokensIdx;

void getTokensIdx(char * s, TokensIdx * t);
uint32_t readNum(char * s);
size_t d_strlcat(char * dst, const char * src, size_t dstSize);


