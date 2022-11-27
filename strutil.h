// strutil.h


typedef struct _TokensIdx {
	uint32_t count;
	uint32_t idx[8];	    // start index
	char * ptr[8];  // pointer to start
	// uint32_t length;  // length of token in bytes or chars
} TokensIdx;

void getTokensIdx(char * s, TokensIdx * t);
uint32_t readNum(char * s);

