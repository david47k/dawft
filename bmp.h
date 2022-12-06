// bmp.h


//----------------------------------------------------------------------------
//  BMP HEADER - STRUCTS
//----------------------------------------------------------------------------

#pragma pack (push)
#pragma pack (2)

typedef struct _BMPHeaderClassic {
	u16 sig;				// "BM"												BITMAPFILEHEADER starts here
	u32 fileSize;			// unreliable - size in bytes of file
	u16 reserved1;			// 0
	u16 reserved2;  		// 0
	u32 offset;				// offset to start of image data 					BITMAPFILEHEADER ends here
	u32 dibHeaderSize;		// 40 = size of BITMAPINFOHEADER					BITMAPINFOHEADER starts here, BITMAPINFO starts here, BITMAPV4HEADER starts here
	i32 width;				// pixels
	i32 height;				// pixels
	u16 planes;				// 1
	u16 bpp;				// 16
	u32 compressionType;	// 0=BI_RGB. 3=BI_BITFIELDS. Must be set to BI_BITFIELDS for RGB565 format.
	u32 imageDataSize;		// including padding - unreliable
	u32 hres;				// pixels per metre
	u32 vres;				// pixels per meter
	u32 clrUsed;			// colors in image, or 0
	u32 clrImportant;		// colors in image, or 0							BITMAPINFOHEADER ends here
	u32 bmiColors[3];		// masks for R, G, B components (for 16bpp)
}  BMPHeaderClassic;

#define BASIC_BMP_HEADER_SIZE (sizeof(BMPHeaderClassic)-12)

typedef struct _BMPHeaderV4 {
	u16 sig;				// "BM"												BITMAPFILEHEADER starts here
	u32 fileSize;			// unreliable - size in bytes of file
	u16 reserved1;			// 0
	u16 reserved2;  		// 0
	u32 offset;				// offset to start of image data 					BITMAPFILEHEADER ends here
	u32 dibHeaderSize;		// 108 for BITMAPV4HEADER							BITMAPINFOHEADER starts here, BITMAPINFO starts here, BITMAPV4HEADER starts here
	i32 width;				// pixels
	i32 height;				// pixels
	u16 planes;				// 1
	u16 bpp;				// 16
	u32 compressionType;	// 3=BI_BITFIELDS. Must be set to BI_BITFIELDS for RGB565 format.
	u32 imageDataSize;		// including padding - unreliable
	u32 hres;				// pixels per metre
	u32 vres;				// pixels per meter
	u32 clrUsed;			// colors in image, or 0
	u32 clrImportant;		// colors in image, or 0							BITMAPINFOHEADER ends here
	u32 RGBAmasks[4];		// masks for R,G,B,A components (if BI_BITFIELDS)	BITMAPINFO ends here
	u32 CSType;
	u32 bV4Endpoints[9];
	u32 gammas[3];
} BMPHeaderV4;

#pragma pack (1)

typedef struct _RGBTrip {
	u8 r;
	u8 g;
	u8 b;
} RGBTrip;

#pragma pack (pop)


//----------------------------------------------------------------------------
//  BMP HEADER - FUNCTIONS
//----------------------------------------------------------------------------

void setBMPHeaderClassic(BMPHeaderClassic * dest, u32 width, u32 height, u8 bpp);
void setBMPHeaderV4(BMPHeaderV4 * dest, u32 width, u32 height, u8 bpp);
int dumpBMP16(char * filename, u8 * srcData, size_t srcDataSize, u32 imgWidth, u32 imgHeight, bool basicRLE);


//----------------------------------------------------------------------------
//  IMG - STORE BASIC IMAGE DATA
//----------------------------------------------------------------------------

// Img is a simple RGB565 image data struct
typedef struct _Img {
    u32 w;					// width in pixels
    u32 h;					// height in pixels
	u32 compression;	 	// 0 = NONE, 1 = RLE_LINE
	u32 size;				// size of data in bytes
    u8 * data;				// each pixel is 2 bytes when uncompressed
} Img;

typedef enum _ImgCompression {
	NONE = 0,
	RLE_LINE = 1,
	RLE_BASIC = 2,
	TRY_RLE = 7,				// only valid when creating new files
} ImgCompression;

extern const char * ImgCompressionStr[8];

Img * newImgFromFile(char * filename, Img * backgroundImg, u32 bpx, u32 bpy);
Img * deleteImg(Img * i);
Img * cloneImg(Img * i);
int compressImg(Img * img);
