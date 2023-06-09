// dawft.h


//----------------------------------------------------------------------------
//  PLATFORM SPECIFIC
//----------------------------------------------------------------------------

#ifndef WINDOWS
#define d_mkdir(s,u) mkdir(s,u)
#define DIR_SEPERATOR "/"
#endif

#ifdef WINDOWS
#define d_mkdir(s,u) mkdir(s)
#define DIR_SEPERATOR "\\"
#endif


//----------------------------------------------------------------------------
//  BASIC TYPES
//----------------------------------------------------------------------------

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;


//----------------------------------------------------------------------------
//  BASIC MACROS
//----------------------------------------------------------------------------

// Boolean string compare.
#define streq(a,b) (strcmp(a,b)==0)

// Boolean string compare.
#define streqn(a,b,n) (strncmp(a,b,n)==0)

// Swap byte order on u16
#define swap_bo_u16(input) (u16)((input & 0xFF) << 8) | ((input & 0xFF00) >> 8)

// gets a LE u16, without care for alignment or system byte order
#define get_u16(p) (u16)( ((const u8*)p)[0] | (((const u8*)p)[1] << 8) )

// gets a LE u32, without care for alignment or system byte order
#define get_u32(p) (u32)( ((const u8*)p)[0] | (((const u8*)p)[1] << 8) | (((const u8*)p)[2] << 16) | (((const u8*)p)[3] << 24) )

// sets a LE u16, without care for alignment or system byte order
void set_u16(u8 * p, u16 v);

//----------------------------------------------------------------------------
//  EXPORTED STRUCTS
//----------------------------------------------------------------------------

typedef struct _Bytes {
    size_t size;
    u8 data[16];        // cover weird padding/align situations
} Bytes;

//----------------------------------------------------------------------------
//  EXPORTED FUNCTIONS
//----------------------------------------------------------------------------

Bytes * newBytesFromFile(char * filename);
Bytes * deleteBytes(Bytes * b);
