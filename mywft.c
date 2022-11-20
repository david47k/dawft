/*  mywft.c

	mywft: Watch Face Tool for MO YOUNG / DA FIT binary watch face files.
	Typically obtained by DA FIT app from api.moyoung.com

	Currently targets type (tpls) 33, of 240x280 resolution, used in the following watches:
		DAFIT C20		MOY-QHF3		"TANK"-like. Advertised as 280x320 pixels :/

	File identifier (first byte of file) is 0x04, 0x81 or 0x84 for these faces.

	Copyright 2022 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)

*/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>		// for mkdir()

#pragma pack(1)


//----------------------------------------------------------------------------
//  BASIC TYPES
//----------------------------------------------------------------------------

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;


//----------------------------------------------------------------------------
//  WATCH TYPES EXAMPLES
//----------------------------------------------------------------------------

typedef struct _WatchType {
	const char * type;		// Type shown in DA FIT app, and on watch about screen, example.
	const char * code;		// Type shown on firmware screen, example. Starts with MOY-
	const char * tpls;		// Types that are requested from the watch face api
	u16 width;				// Screen width in pixels
	u16 height;				// Screen height in pixels
	char fileType;			// A, B or C
} WatchType;

/*

fileType	FaceData size	Offset table start		Decompression required for offset table		RLE bitmap has row index
TYPE A 		6				200						No											No
TYPE B		10				400						Yes											?
TYPE C 		10				400						No											Yes

*/

static WatchType watchTypes[] = {
	{ "???", "???",   "1", 240, 240, 'A' },			// square face
	{ "???", "???",   "6", 240, 240, 'A' },			// round face
	{ "???", "???",   "7", 240, 240, 'A' },			// round face
	{ "???", "???",   "8", 240, 240, 'A' },			// 
	{ "???", "???",  "13", 240, 240, 'B' },			// 
	{ "???", "???",  "19", 240, 240, 'B' },			// round face
	{ "???", "???",  "20", 240, 240, 'B' },			// 
	{ "???", "???",  "25", 360, 360, 'B' },			// round face
	{ "???", "???",  "27", 240, 240, 'A' },			// 
	{ "???", "???",  "28", 240, 240, 'A' },			// 
	{ "???", "???",  "29", 240, 240, 'A' },			// 
	{ "???", "???",  "30", 240, 240, 'B' },			// 
	{ "C20", "QHF3", "33", 240, 280, 'C' },			// 
	{ "???", "???",  "34", 240, 280, 'B' },			// 
	{ "???", "???",  "36", 240, 295, 'B' },			// 
	{ "???", "???",  "38", 240, 240, 'C' },			// round face
	{ "???", "???",  "39", 240, 240, 'C' },			// square face
	{ "???", "???",  "40", 320, 385, 'C' },			// round face	
};


//----------------------------------------------------------------------------
//  DATA READING AND BYTE ORDER
//----------------------------------------------------------------------------

static u16 get_u16(const u8 * ptr) {			// gets a LE u16, without care for alignment or system byte order
	return ptr[0] | (u16)( ptr[1] << 8 );
}

static u32 get_u32(const u8 * ptr) {			// gets a LE u32, without care for alignment or system byte order
	return ptr[0] | (u32)( ptr[1] << 8 ) | (u32)( ptr[2] << 16 ) | (u32)( ptr[3] << 24 );
}

static int systemIsLittleEndian() {				// return 0 for big endian, 1 for little endian.
    volatile uint32_t i=0x01234567;
    return (*((volatile uint8_t*)(&i))) == 0x67;
}

static uint16_t swapByteOrder2(uint16_t input) {
    uint16_t output = (uint16_t)((input & 0xFF) << 8);
    output |= (input&0xFF00)>>8;
    return output;
}


static uint32_t swapByteOrder4(uint32_t input) {
	uint32_t output = 0;
    output |= (input&0x000000FF) << 24;
    output |= (input&0x0000FF00) <<  8;
    output |= (input&0x00FF0000) >> 8;
    output |= (input&0xFF000000) >> 24;
    return output;
}


//----------------------------------------------------------------------------
//  BMP HEADER
//----------------------------------------------------------------------------

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
	u32 imageDataSize;		// including padding
	u32 hres;				// pixels per metre
	u32 vres;				// pixels per meter
	u32 clrUsed;			// colors in image, or 0
	u32 clrImportant;		// colors in image, or 0							BITMAPINFOHEADER ends here
	u32 bmiColors[3];		// masks for R, G, B components (for 16bpp)
} BMPHeaderClassic;

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
	u32 imageDataSize;		// including padding
	u32 hres;				// pixels per metre
	u32 vres;				// pixels per meter
	u32 clrUsed;			// colors in image, or 0
	u32 clrImportant;		// colors in image, or 0							BITMAPINFOHEADER ends here
	u32 RGBAmasks[4];		// masks for R,G,B,A components (if BI_BITFIELDS)	BITMAPINFO ends here
	u32 CSType;
	u32 bV4Endpoints[9];
	u32 gammas[3];
} BMPHeaderV4;

// Set up a BMP header. bpp must be 16 or 24.
static void setBMPHeaderClassic(BMPHeaderClassic * dest, u32 width, u32 height, u8 bpp) {
	// Note: 24bpp images should only dump (dest->offset) bytes of this header, not the whole thing (don't need last 12 bytes)
	*dest = (BMPHeaderClassic){ 0 };
	dest->sig = 0x4D42;
	if(bpp == 16) {
		dest->offset = sizeof(BMPHeaderClassic);
	} else if(bpp == 24) {
		dest->offset = sizeof(BMPHeaderClassic) - 12;
	}
	dest->dibHeaderSize = 40;						// 40 for BITMAPINFOHEADER
	dest->width = (i32)width;
	dest->height = -(i32)height;
	dest->planes = 1;
	dest->bpp = bpp;
	if(bpp == 16) {
		dest->compressionType = 3;					// BI_BITFIELDS=3
		dest->bmiColors[0] = 0xF800;				
		dest->bmiColors[1] = 0x07E0;				
		dest->bmiColors[2] = 0x001F;				
	} else if(bpp == 24) {
		dest->compressionType = 0;					// BI_RGB=0
	}
	u32 rowSize = (((bpp/8) * width) + 3) & 0xFFFFFFFC;
	dest->imageDataSize = rowSize * height;
	dest->fileSize = dest->imageDataSize + dest->offset;
	dest->hres = 2835;								// 72dpi
	dest->vres = 2835;								// 72dpi
}

// Set up a BMP header. bpp must be 16 or 24.
static void setBMPHeaderV4(BMPHeaderV4 * dest, u32 width, u32 height, u8 bpp) {
	*dest = (BMPHeaderV4){ 0 };
	dest->sig = 0x4D42;
	dest->offset = sizeof(BMPHeaderV4);
	dest->dibHeaderSize = 108; 						// 108 for BITMAPV4HEADER
	dest->width = (i32)width;
	dest->height = -(i32)height;
	dest->planes = 1;
	dest->bpp = bpp;
	u32 rowSize = (((bpp/8) * width) + 3) & 0xFFFFFFFC;
	if(bpp == 16) {
		dest->compressionType = 3; 					// BI_BITFIELDS=3
		dest->RGBAmasks[0] = 0xF800;
		dest->RGBAmasks[1] = 0x07E0;
		dest->RGBAmasks[2] = 0x001F;	
	} else if(bpp == 24) {
		dest->compressionType = 0; 					// BI_RGB=0
	}
	dest->imageDataSize = rowSize * height;
	dest->fileSize = dest->imageDataSize + sizeof(BMPHeaderV4);
	dest->hres = 2835;								// 72dpi
	dest->vres = 2835;								// 72dpi
}

typedef struct _RGBTrip {
	u8 r;
	u8 g;
	u8 b;
} RGBTrip;

static RGBTrip RGB565to888(u16 pixel) {	
	pixel = swapByteOrder2(pixel);				// need to reverse the source pixel
	RGBTrip output;
	output.r = (u8)((pixel & 0x001F) << 3);		// first 5 bits
	output.r |= (pixel & 0x001C) >> 3;			// add extra precision of 3 bits
	output.g = (pixel & 0x07E0) >> 3;			// first 6 bits
	output.g |= (pixel & 0x0600) >> 9;			// add extra precision of 2 bits
	output.b = (pixel & 0xF800) >> 8;			// first 5 bits
	output.b |= (pixel & 0xE000) >> 13;			// add extra precision of 3 bits
	return output;
}


//----------------------------------------------------------------------------
//  BINARY FILE STRUCTURE
//----------------------------------------------------------------------------


/* For reference: 
typedef struct _FaceDataA {
	u8 type;				// type of object we are giving dimensions e.g. hours, day, steps etc.
	u8 x;			
	u8 y;
	u8 w;
	u8 h;
	u8 idx;					// index into offset table
} FaceDataA; 				// size is 6 bytes

typedef struct _FaceHeaderA {
	u8 fileID;	
	u8 dataCount;
	u8 blobCount;	
	u16 faceNumber;	
	FaceDataA faceData[32];	// sizeof(FaceDataA) = 6 bytes
	u8 padding[3];	
	u32 offsets[250];		// offsets of bitmap data for the face. Table starts at 200. Offsets start from end of header data i.e. at 1700.
	u16 sizes[250];			// sizes of the bitmap data, in bytes. Unreliable.
} FaceHeaderA;				// size is 1700 bytes
*/

typedef struct _FaceData {
	u8 type;				// type of object we are giving dimensions e.g. hours, day, steps etc.
	u8 idx;					// index into offset table
	u16 x;
	u16 y;
	u16 w;
	u16 h;
} FaceData; 				// size is 10 bytes

typedef struct _FaceHeader {
	u8 fileID;				// 0x81 or 0x04 or 0x84
	u8 dataCount;			// how many data objects
	u8 blobCount;			// how many blob (bitmap) objects
	u16 faceNumber;			// design number of this face, in this case 0x1E38 or 7736
	FaceData faceData[39];	// sizeof(FaceData) = 10 bytes
	u8 padding[5];
	u32 offsets[250];		// offsets of bitmap data for the face. Table starts at 400. Offsets start from end of header data i.e. at 1900.
	u16 sizes[250];			// sizes of the bitmap data, in bytes. For Type B, sizes are the uncompressed sizes. Unreliable. 
} FaceHeader;				// size is 1900 bytes

static void setHeader(FaceHeader * h, const u8 * buf, char fileType) {
	if(fileType != 'A' && fileType != 'B' && fileType != 'C') {
		printf("ERROR: Invalid fileType in setHeader!\n");
		return;
	}
	
	*h = (FaceHeader){ 0 };

	h->fileID = buf[0];
	h->dataCount = buf[1];
	h->blobCount = buf[2];
	h->faceNumber = get_u16(&buf[3]);

	u32 idx = 5;
	
	// load faceData
	if(fileType != 'A') {
		for(int i=0; i<39; i++) {
			h->faceData[i].type = buf[idx];
			h->faceData[i].idx = buf[idx+1];
			h->faceData[i].x = get_u16(&buf[idx+2]);
			h->faceData[i].y = get_u16(&buf[idx+4]);
			h->faceData[i].w = get_u16(&buf[idx+6]);
			h->faceData[i].h = get_u16(&buf[idx+8]);
			idx += 10;
		}
		memcpy(h->padding, &buf[idx], 5);
		idx += 5;
	} else { // fileType == 'A'
		for(int i=0; i<32; i++) {
			h->faceData[i].type = buf[idx];
			h->faceData[i].idx = buf[idx+5];
			h->faceData[i].x = buf[idx+1];
			h->faceData[i].y = buf[idx+2];
			h->faceData[i].w = buf[idx+3];
			h->faceData[i].h = buf[idx+4];
			idx += 6;
		}
		memcpy(h->padding, &buf[idx], 3);
		idx += 3;
	}
	
	// load offsets
	for(int i=0; i<250; i++) {
		h->offsets[i] = get_u32(&buf[idx]);
		idx += 4;
	}

	// load sizes
	for(int i=0; i<250; i++) {
		h->sizes[i] = get_u16(&buf[idx]);
		idx += 2;
	}

	// Note: if fileType == 'B', offsets are into the *decompressed* data. 
}

//----------------------------------------------------------------------------
//  DATA TYPES
//----------------------------------------------------------------------------

typedef struct _DataType {
	u8 type;
	const char str[16];
	u8 count;
} DataType;

// digits: only w, h of first digit. 0-9.
static const DataType dataTypes[] = {	
	{ 0x00, "BACKGROUNDS", 10 },		// Split into 10 parts of 240x24 each. Often example time is in it, just overwritten. Typical in Type A watch faces.
	{ 0x01, "BACKGROUND", 1 },			// Background image, usually of full width and height of screen, usually the first item. Typical in Type B & C watch faces.
	{ 0x10, "MONTH_NAME", 12 },			// JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC 	
	{ 0x11, "MONTH_NUM", 10 },			// month, digits.
	{ 0x12, "YEAR", 10 },				// year, 2 digits, left aligned 
	{ 0x30, "DAY_NUM", 10 },			// Day number of the month. digits
	{ 0x40, "TIME_H1", 10 },			// Hh:mm
	{ 0x41, "TIME_H2", 10 },			// hH:mm
	{ 0x43, "TIME_M1", 10 },			// hh:Mm
	{ 0x44, "TIME_M2", 10 },			// hh:mM
	{ 0x45, "TIME_AM", 1 },				// AM 
	{ 0x46, "TIME_PM", 1 },				// PM 
	{ 0x60, "DAY_NAME", 7 },			// SUN, MON, TUE, WED, THU, FRI, SAT.
	{ 0x61, "DAY_NAME_CN", 7 },			// SUN, MON, TUE, WED, THU, FRI, SAT but in chinese characters
	{ 0x62, "STEPS", 10 },				// steps, standard LA, digits 
	{ 0x63, "STEPS_CA", 10 },			// steps, centered, digits 
	{ 0x64, "STEPS_RA", 10 },			// steps, right aligned, digits 
	{ 0x67, "HR_RA", 10 },				// Heart rate, right aligned, digits
	{ 0x68, "KCAL", 10 },				// kcals, LA, digits 
	{ 0x6b, "MONTH_NUM_B", 10 }, 		// 10 digits font, but sometimes there are random blobs at the end
	{ 0x6c, "DAY_NUM_B", 10 },			// digits
	{ 0x70, "STEPS_PROGBAR", 11 },		// Steps progess bar 0,10,20...100%. 11 frames. #6232, 2153, 2154, 3110.
	{ 0x71, "STEPS_LOGO", 1 },			// Step count, static logo
	{ 0x72, "STEPS_B", 10 },			// Step count, LA, digits
	{ 0x73, "STEPS_B_CA", 10 },			// Step count, centered, digits
	{ 0x74, "STEPS_B_RA", 10 },			// Step count, RA, digits
	{ 0x76, "STEPS_GOAL", 1 },			// Step goal, LA, digits.
	{ 0x80, "HR_LOGO_80", 1 },			// Heart rate logo, could be progress bar ???
	{ 0x81, "HR_LOGO", 1 },				// Heart rate logo, static
	{ 0x82, "HR_B", 10 },				// Heart rate, LA, digits
	{ 0x83, "HR_B_CA", 10 },			// Heart rate, centered, digits
	{ 0x84, "HR_B_RA", 10 },			// Heart rate, right aligned, digits
	{ 0x90, "KCAL_PROGBAR", 11 },		// kcals progress bar, 0,10,20...100%. 11 frames. In #2154.
	{ 0x91, "KCAL_LOGO", 1 },			// kcals, static image
	{ 0x92, "KCAL_B", 10 },				// kcals, LA, digits 
	{ 0x93, "KCAL_B_CA", 10 },			// kcals, CA, digits 
	{ 0x94, "KCAL_B_RA", 10 },			// kcals, RA, digits 
	{ 0xA0, "DIST_PROGBAR", 11 },		// distance progress bar 0,10,20...100%.
	{ 0xA1, "DIST_LOGO", 1 },			// distance, static logo
	{ 0xA2, "DIST", 10 },				// distance, LA, digits
	{ 0xA3, "DIST_CA", 10 },			// distance, CA, digits
	{ 0xA4, "DIST_RA", 10 },			// distance, RA, digits
	{ 0xA5, "DIST_KM", 1 },				// KM for kilometers. LA.   in #3990 for example
	{ 0xA6, "DIST_MI", 1 },				// MI for miles. LA.        in #3990 for example
	{ 0xC0, "BTLINK_UP", 1 },			// bluetooth link active 
	{ 0xC1, "BTLINK_DOWN", 1 },			// bluetooth link inactive
	{ 0xCE, "BATT_IMG", 1 },			// battery charge image
	{ 0xD0, "BATT_IMG_B", 1 },			// battery charge image
	{ 0xD1, "BATT_IMG_C", 1 },			// battery charge image
	{ 0xD3, "BATT_CA", 10},				// Battery charge, centre aligned, digits 
	{ 0xD4, "BATT_RA", 10},				// Battery charge, right aligned, digits 
	{ 0xDA, "BATT_IMG_C", 1 },			// Battery charge image, 5 bars.
	{ 0xD8, "TEMP_CA", 10 },			// Temperature, centre aligned, digits.
	{ 0xF0, "TIME_SEP_HM", 1 },			// Hour/minute seperator. 
	{ 0xF1, "HAND_HOUR", 1 },			// Hour hand (at 1200 positio ).
	{ 0xF2, "HAND_MINUTE", 1 },			// Hour hand (at 1200 positio ).
	{ 0xF3, "HAND_SEC", 1 },			// Hour hand (at 1200 positio ).
	{ 0xF4, "HAND_PIN_UPPER", 1 },		// top half of dot. in #2102, 2105-6,2109,2112-2113,2117-8,2142,2145-2149
	{ 0xF5, "HAND_PIN_LOWER", 1 },		// bottom half of dot.
	{ 0xF6, "SPACED_OUT", 4 },			// 4-frames: seasons or time of day? on #3135, 3156, 5315.
	{ 0xF7, "DINOSAUR", 8 },			// 8-frame animation. #3145, #163.  #3087, #3088 has it as a 7-frame animation.
	{ 0xF8, "SPACEMAN", 21 },			// 21-frame animation. #3085, #3086, #3248.  #3112 has it as a 14-frame animation.
};		
								
static const char dataTypeStrUnknown[12] = "UNKNOWN";

static const char * getDataTypeStr(u8 type) {
	int length = sizeof(dataTypes) / sizeof(DataType);
	for(int i=0; i<length; i++) {
		if(dataTypes[i].type == type) {
			return dataTypes[i].str;
		}
	}
	return dataTypeStrUnknown;
}

static int getDataTypeIdx(u8 type) {
	int length = sizeof(dataTypes) / sizeof(DataType);
	for(int i=0; i<length; i++) {
		if(dataTypes[i].type == type) {
			return i;
		}
	}
	return -1; // failed to find
}

// Find the faceData index given an offset index
static int getFaceDataIndexFromOffsetIndex(int offsetIndex, FaceHeader * h) {
	int fdi = 0;
	int matchIdx = -1;
	for(fdi=0; fdi < h->dataCount; fdi++) {
		int count = 1;
		int typeIdx = getDataTypeIdx(h->faceData[fdi].type);
		if(typeIdx >= 0) {
			count = dataTypes[typeIdx].count;
		}
		if(offsetIndex >= h->faceData[fdi].idx && offsetIndex < (h->faceData[fdi].idx + count)) {
			matchIdx = fdi; // we found a match
			break;
		}
	}
	return matchIdx; // -1 for failure, index for success
}


//----------------------------------------------------------------------------
//  DUMPBLOB - dump binary data to file
//----------------------------------------------------------------------------

static int dumpBlob(char * filename, u8 * srcData, size_t length) {
	// open the dump file
	FILE * dumpFile = fopen(filename,"wb");
	if(dumpFile==NULL) {
		return 1;
	}
	int idx = 0;

	// write the data to the dump file
	while(length > 0) {
		size_t bytesToWrite = 4096;
		if(length < bytesToWrite) {
			bytesToWrite = length;
		}

		size_t rval = fwrite(&srcData[idx],1,bytesToWrite,dumpFile);
		if(rval != bytesToWrite) {
			return 2;
		}
		idx += bytesToWrite;
		length -= bytesToWrite;
	}

	// close the dump file
	fclose(dumpFile);

	return 0; // SUCCESS
}


//----------------------------------------------------------------------------
//  DUMPBMP - dump binary data to bitmap file
//----------------------------------------------------------------------------

/* For 24bpp:
	for(u32 x=0; x<imgWidth; x++) {
		RGBTrip pixel = RGB565to888(get_u16(&srcPtr[2*x]));
		buf[3*x] = pixel.r;
		buf[3*x+1] = pixel.g;
		buf[3*x+2] = pixel.b;
	}
*/

static int dumpBMP16(char * filename, u8 * srcData, u32 imgWidth, u32 imgHeight, u8 oldRLE) {	
	// Check if this bitmap has the RLE encoded identifier
	u16 identifier = get_u16(&srcData[0]);
	int isRLE = (identifier == 0x2108);

	BMPHeaderV4 bmpHeader;
	setBMPHeaderV4(&bmpHeader, imgWidth, imgHeight, 16);

	// row width is equal to imageDataSize / imgHeight
	u32 destRowSize = bmpHeader.imageDataSize / imgHeight;

	u8 buf[8192];
	if(destRowSize > sizeof(buf)) {
		printf("Image width exceeds buffer size!\n");
		return 3;
	}

	// open the dump file
	FILE * dumpFile = fopen(filename,"wb");
	if(dumpFile==NULL) {
		return 1;
	}

	// write the header 	
	size_t rval = fwrite(&bmpHeader,1,sizeof(bmpHeader),dumpFile);
	if(rval != sizeof(bmpHeader)) {
		fclose(dumpFile);
		return 2;
	}

	if(isRLE && !oldRLE) {
		// The newer RLE style has a table at the start with the offsets of each row.
		u8 * lineEndOffset = &srcData[2];
		size_t srcIdx = (2 * imgHeight) + 2; // offset from start of RLEImage to RLEData

		// for each row
		for(u32 y=0; y<imgHeight; y++) {
			memset(buf, 0, destRowSize);
			u32 bufIdx = 0;

			//printf("line %d, srcIdx %lu, lineEndOffset %d, first color 0x%04x, first count %d\n", y, srcIdx, lineEndOffset[y], 0, srcData[srcIdx+2]);
			
			while(srcIdx < get_u16(&lineEndOffset[y*2])) { // built in end-of-line detection			
				u8 count = srcData[srcIdx + 2];
				u8 pixel0 = srcData[srcIdx + 1];
				u8 pixel1 = srcData[srcIdx + 0];

				for(int j=0; j<count; j++) {	// fill out this color
					buf[bufIdx] = pixel0;
					buf[bufIdx+1] = pixel1;
					bufIdx += 2;
				}
				srcIdx += 3; // next block of data
			}

			rval = fwrite(buf,1,destRowSize,dumpFile);
			if(rval != destRowSize) {
				fclose(dumpFile);
				return 2;
			}
		}
	} else if(isRLE && oldRLE) {
		// This is an OLD RLE style, with no offsets at the start, and no concern for row boundaries
		u32 srcIdx = 2;		
		u8 pixel0 = 0;
		u8 pixel1 = 0;
		u8 count = 0;
		for(u32 y=0; y<imgHeight; y++) {
			memset(buf, 0, destRowSize);
			u32 pixelCount = 0;
			u32 i = 0;
			if(count > 0) {
				// still some pixels leftover from last row
				while(i < count && i < imgWidth) {
					buf[i*2] = pixel0;
					buf[i*2+1] = pixel1;
					i++;
					pixelCount++;
				}
			}
			while(pixelCount < imgWidth) {
				count = srcData[srcIdx + 2];
				pixel0 = srcData[srcIdx + 1];
				pixel1 = srcData[srcIdx + 0];

				i = 0;
				while(i < count && pixelCount < imgWidth) {
					buf[pixelCount*2] = pixel0;
					buf[pixelCount*2+1] = pixel1;
					i++;
					pixelCount++;
				}
				srcIdx += 3;
			}
			if(count>i) {
				count -= i;		// some pixels left over
			} else {
				count = 0;		// no pixels left over
			}
			rval = fwrite(buf, 1, destRowSize, dumpFile);
			if(rval != destRowSize) {
				fclose(dumpFile);
				return 2;
			}
		}
	} else {
		// for each row
		const u32 srcRowSize = imgWidth * 2;
		size_t srcIdx = 0;
		for(u32 y=0; y<imgHeight; y++) {
			memset(buf, 0, destRowSize);
			u8 * srcPtr = &srcData[srcIdx];
			// for each pixel
			for(u32 x=0; x<imgWidth; x++) {
				u16 pixel = swapByteOrder2(get_u16(&srcPtr[x*2]));
				buf[2*x] = pixel & 0xFF;
				buf[2*x+1] = pixel >> 8;
			}

			rval = fwrite(buf,1,destRowSize,dumpFile);
			if(rval != destRowSize) {
				return 2;
			}
			srcIdx += srcRowSize;
		}
	}

	// close the dump file
	fclose(dumpFile);

	return 0; // SUCCESS
}

//----------------------------------------------------------------------------
//  AUTODETECT FILE TYPE
//----------------------------------------------------------------------------

static char autodetectFileType(u8 * fileData, size_t fileSize) {		// Auto-detect file type.
	u8 blobCount = fileData[2];
	int typeACount = 1;
	u8 typeARunning = 1;
	int typeBCount = 1;
	u8 typeBRunning = 1;
	u32 typeBMax = 0;
	char fileType = 0;

	// Count the offsets and compare to blobCount. Type A offset table starts at 200. Type B/C offset table starts at 400.
	for(u32 i=1; i<250; i++) {
		if(typeARunning) {
			if(get_u32(&fileData[200+(i*4)]) != 0) {
				typeACount += 1;
			} else {
				typeARunning = 0;
			}
		}
		if(typeBRunning) {
			u32 offset = get_u32(&fileData[400+(i*4)]);
			if(offset != 0) {
				typeBCount += 1;
				typeBMax = offset;
			} else {
				typeBRunning = 0;
			}
		}
	}		

	if(typeACount == blobCount) {
		printf("Autodetected fileType A\n");
		fileType = 'A';			
	} else if(typeBCount == blobCount) {
		typeBMax += 1900; // add header size
		// Type B will have offsets larger than the file size (as they are into the uncompressed data). Type C should be smaller than the file size.
		if(typeBMax > fileSize) {
			printf("Autodetected fileType B\n");
			// (offset %u is greater than fileSize %zu)\n", typeBMax, fileSize);
			fileType = 'B';
		} else {
			printf("Autodetected fileType C\n");
			// (offset %u is less than fileSize %zu)\n", typeBMax, fileSize);
			fileType = 'C';
		}
	} else {
		printf("WARNING: Unable to autodetect fileType. Defaulting to type A.\n");
		fileType = 'A';
	}
	return fileType;
}

//----------------------------------------------------------------------------
//  MAIN
//----------------------------------------------------------------------------

int main(int argc, char * argv[]) {
    printf("\n%s\n\n","mywft: Watch Face Tool for MO YOUNG / DA FIT binary watch face files.");
    
	// check byte order
	if(!systemIsLittleEndian()) {
		printf("Sorry, this system is big-endian, and this program has only been designed for little-endian systems.\n");
		return 1;
	}

	// find executable name
	char * basename = "mywft";
	if(argc>0) {
		// find the name of the executable. not perfect but it's only used for display and no messy ifdefs.
		char * b = strrchr(argv[0],'\\');
		if(!b) {
			b = strrchr(argv[0],'/');
		}
		basename = b ? b+1 : argv[0];
	}

	// display help
    if(argc<2) {
		printf("Usage: \n%s [mode=info|dump|create] [folder=FOLDERNAME] FILENAME\n\n",basename);
		printf("\t%s\n","FILENAME               Binary watch face file for input (or output).");
		printf("\t%s\n","mode=info              Display info about binary file. Default.");
		printf("\t%s\n","     dump              Dump data from binary file to folder.");
		printf("\t%s\n","     create            Create binary file from data in folder.");
		printf("\t%s\n","folder=FOLDERNAME      Folder to dump data to/read from. Defaults to the face design number.");
		printf("\t%s\n","                       Required for mode=create.");
		printf("\t%s\n","raw=true               When dumping, dump raw files. Default is false.");
		printf("\t%s\n","fileType=8             Specify type of binary file (A, B or C)");
		printf("\n");
		return 0;
    }
		
    char * filename = "";
	char * foldername = "";
	char * filemode = "rb";
	enum _MODE {
		INFO,
		DUMP,
		CREATE
	} mode = INFO;
	u8 raw = 0;
	char fileType = 0;
    
	// read command-line parameters
	for(int i=1; i<argc; i++) {
		if(strncmp(argv[i], "mode=dump",9)==0) {
			mode = DUMP;
			filemode="rb";
		} else if(strncmp(argv[i], "mode=create",11)==0) {
			mode = CREATE;
			filemode = "wb";
		} else if(strncmp(argv[i], "mode=info",9)==0) {
			mode = INFO;
			filemode="rb";
		} else if(strncmp(argv[i],"mode=",5)==0) {
			printf("ERROR: Invalid mode\n");
			return 1;
		} else if(strncmp(argv[i], "raw=true", 8)==0) {
			raw = 1;
		} else if(strncmp(argv[i], "raw=false", 9)==0) {
			raw = 0;
		} else if(strncmp(argv[i], "raw=", 4)==0) {
			printf("ERROR: Invalid raw=\n");
			return 1;
		} else if(strncmp(argv[i], "fileType=A", 10)==0) {
			fileType = 'A';
		} else if(strncmp(argv[i], "fileType=B", 10)==0) {
			fileType = 'B';
		} else if(strncmp(argv[i], "fileType=C", 10)==0) {
			fileType = 'C';
		} else if(strncmp(argv[i], "fileType=", 9)==0) {
			printf("ERROR: Invalid fileType=\n");
			return 1;
		} else if(strncmp(argv[i],"folder=",7)==0) {
			foldername = &argv[i][7];
		} else {
			// must be filename
			filename=argv[i];
		}
	}		

	// Check if we are in CREATE mode
	if(mode==CREATE) {
		printf("ERROR: Create mode not yet implemented.\n");
		return 1;
	}

	// We are in INFO / DUMP mode

	// Open the binary input file
    FILE * f = fopen(filename, filemode);
    if(f==NULL) {
		printf("ERROR: Failed to open input file: '%s'\n", filename);
		return 1;
    }

	// Check file size
    fseek(f,0,SEEK_END);
	long ftr = ftell(f);
	fseek(f,0,SEEK_SET);

	if(ftr < 1700) {
		printf("ERROR: File is less than the minimum header size (1700 bytes)!\n");
		fclose(f);
		return 1;
	}

	size_t fileSize = (size_t)ftr;

	// Allocate buffer and read whole file
	u8 * fileData = malloc(fileSize);
	if(fileData == NULL) {
		printf("ERROR: Unable to allocate enough memory to open file.\n");
		fclose(f);
		return 1;
	}
	if(fread(fileData,1,fileSize,f) != fileSize) {
		printf("ERROR: Read failed.\n");
		fclose(f);
		free(fileData);
		return 1;
	}

	// Now file is loaded, close the file
	fclose(f);

	// Check first byte of file
	if(fileData[0] != 0x81 && fileData[0] != 0x04 && fileData[0] != 0x84) {
		printf("WARNING: Unknown fileID: 0x%02x\n", fileData[0]);
	}

	// Autodetect file type, if unspecified.
	if(fileType==0) {
		fileType = autodetectFileType(fileData, fileSize);
	}

	// determine header size
	u32 headerSize = 1900;
	if(fileType == 'A') {
		headerSize = 1700;
	}

	if(fileSize < headerSize) {
		printf("ERROR: File is less than the header size (%u bytes)!\n", headerSize);
		fclose(f);
		return 1;
	}

	// store discovered data in string, for saving to file, so we can recreate this bin file
	char watchFaceStr[(39+5)*100] = "";		// have enough room for all the lines we need to store
	char lineBuf[128] = "";

	snprintf(lineBuf, sizeof(lineBuf), "fileType       %c\n", fileType);
	strcat(watchFaceStr, lineBuf);

	snprintf(lineBuf, sizeof(lineBuf), "fileID         0x%02x\n", fileData[0]);
	strcat(watchFaceStr, lineBuf);

	// Print header info
	FaceHeader header;
	setHeader(&header, fileData, fileType);
	FaceHeader * h = &header;

	snprintf(lineBuf, sizeof(lineBuf), "dataCount      %u\n", h->dataCount);
	strcat(watchFaceStr, lineBuf);
	snprintf(lineBuf, sizeof(lineBuf), "blobCount      %u\n", h->blobCount);
	strcat(watchFaceStr, lineBuf);
	snprintf(lineBuf, sizeof(lineBuf), "faceNumber     %u\n", h->faceNumber);
	strcat(watchFaceStr, lineBuf);
	snprintf(lineBuf, sizeof(lineBuf), "padding        %02x%02x%02x%02x%02x\n", h->padding[0], h->padding[1], h->padding[2], h->padding[3], h->padding[4] );
	strcat(watchFaceStr, lineBuf);

	// Print faceData header info
	FaceData * background = NULL;
	FaceData * backgrounds = NULL;
	int myDataCount = 0;
	for(u32 i=0; i<(sizeof(h->faceData)/sizeof(h->faceData[0])); i++) {
		if(h->faceData[i].type != 0 || i==0) {		// some formats use type 0 in position 0 as background
			FaceData * fd = &h->faceData[i];			
			snprintf(lineBuf, sizeof(lineBuf), "faceData       0x%02x  %04u  %-15s %4u %4u %4u %4u\n",
				fd->type, fd->idx, getDataTypeStr(fd->type), fd->x, fd->y, fd->w, fd->h
				);
			strcat(watchFaceStr, lineBuf);
			myDataCount++;
			if(fd->type == 0x01 && background == NULL) {
				background = &h->faceData[i];
			}
			if(fd->type == 0x00 && backgrounds == NULL) {
				backgrounds = &h->faceData[i];
			}
		}
	}
	int fail = 0;

	printf("%s", watchFaceStr);		// display all the important data

	if(background == NULL && backgrounds == NULL) {
		printf("WARNING: No background found.\n");
	}

	if(myDataCount != h->dataCount) {
		printf("myDataCount    %u\n", myDataCount);
		printf("ERROR: myDataCount != dataCount!\n");
		fail = 1;		
	}

	// Count offsets to verify number of blobs
	int offsetCount = 1;	// there is always one offset, of 0, at offsets[0]
	for(u32 i=1; i<250; i++) {
		if(h->offsets[i] != 0) {
			offsetCount += 1;
			if(h->offsets[i] >= fileSize && fileType != 'B' && !fail) {
				printf("ERROR: Offset %u is greater than file size, cannot dump this file.\n", h->offsets[i]);	// Unknown file type
				fail = 1;
			}
		} 
	}

	if(fail) {
		free(fileData);
		return 1;
	}

	if(offsetCount != h->blobCount) {
		printf("offsetCount    %u\n", offsetCount);
		printf("WARNING: offsetCount != blobCount!\n");
	}

	u8 nothing[5] = { 0, 0, 0, 0, 0 };
	if(memcmp(h->padding, nothing, 5) != 0) {
		printf("WARNING: padding are not 0!\n");
	}

	// Check if we are in DUMP mode
	if(mode==DUMP) {
		if(fileType == 'B') {
			// What compression method is used in type B files?
			printf("Dumping from fileType B is not supported\n");
			free(fileData);
			return 1;
		}

		char dumpFileName[1024];
		char * folderStr;
		char faceNumberStr[16];
		snprintf(faceNumberStr, sizeof(faceNumberStr), "%d", h->faceNumber);
		if(foldername[0]==0) {		// if empty string
			folderStr = faceNumberStr;
		} else {
			folderStr = foldername;
		}
		
		// create folder if it doesn't exist
		mkdir(folderStr, S_IRWXU);

		// append a /
		char * slash = "/";						// NOT_WIN32_SAFE

		// dump the watch face data
		snprintf(dumpFileName, sizeof(dumpFileName), "%s%swatchface.txt", folderStr, slash);
		FILE * fwf = fopen(dumpFileName,"wb");
		if(fwf == NULL) {
			printf("ERROR: Failed to open '%s' for writing\n", dumpFileName);
			free(fileData);
			return 1;
		}
		size_t res = fwrite(watchFaceStr, 1, strlen(watchFaceStr), fwf);
		if(res != strlen(watchFaceStr)) {
			printf("ERROR: Failed when writing to '%s'\n", dumpFileName);
			free(fileData);
			fclose(fwf);
			return 1;
		}
		fclose(fwf);

		// for each binary blob
		for(int i=0; i<offsetCount; i++) {
			// determine file offset
			u32 fileOffset = headerSize;
			fileOffset += h->offsets[i];
			// printf("offset %u is %u\n", i, h->offsets[i]);
			
			// get faceData index from offset index
			int fdi = getFaceDataIndexFromOffsetIndex(i, h);

			if(raw) {
				// is it RLE?
				int isRLE = (fileData[fileOffset] == 0x08 && fileData[fileOffset+1] == 0x21);

				// determine length of blob
				size_t length = h->sizes[i];	// Sometimes, length is stored in file

				if(!isRLE) { 
					// Sometimes length isn't stored in file.
					// We'll estimate the length

					size_t estimatedLength = 0;
					if(fdi != -1) {
						estimatedLength = h->faceData[fdi].w * h->faceData[fdi].h * 2;
					}
					if(length == 0) {
						length = estimatedLength;
					}
					if(length != estimatedLength && estimatedLength != 0) {
						printf("WARNING: Length mismatch (file: %zu, estimated: %zu)\n", length, estimatedLength);
					}
				}

				if(length == 0) {
					printf("WARNING: Unable to determine length for blob idx %04d, not dumping raw data.\n", i);
				} else {
					// check it won't go past EOF
					if(fileOffset + length > fileSize) {
						printf("WARNING: Unable to dump raw blob %u as it exceeds EOF (%zu>%zu)\n", i, fileOffset+length, fileSize);
						continue;
					}
					// assemble file name to dump to
					snprintf(dumpFileName, sizeof(dumpFileName), "%s%s%04d.raw", folderStr, slash, i);
					printf("Dumping RAW length %6zu to file %s\n", length ,dumpFileName);

					// dump it
					int rval = dumpBlob(dumpFileName, &fileData[fileOffset], length);
					if(rval != 0) {
						printf("Failed to write data to file: '%s'\n", dumpFileName);
					}
				}
			}

			// Dump the bitmaps
			if(fdi != -1) {
				if(dataTypes[fdi].type==0x00 && (fileType=='A') && (h->faceData[fdi].w != 240 || h->faceData[fdi].h != 24)) {
					// override width and height
					h->faceData[fdi].w = 240;
					h->faceData[fdi].h = 24;
					printf("WARNING: Overriding width and height with 240x24 for backgrounds of type 0x00\n");
				}
				snprintf(dumpFileName, sizeof(dumpFileName), "%s%s%04d.bmp", folderStr, slash, i);
				printf("Dumping BMP               to file %s\n",  dumpFileName);
				dumpBMP16(dumpFileName, &fileData[fileOffset], h->faceData[fdi].w, h->faceData[fdi].h, (fileType=='A'));
			} else if(i == (offsetCount - 1)) {
				// this is a small preview image of 140x163, used when selecting backgrounds (for 240x280 images)
				snprintf(dumpFileName, sizeof(dumpFileName), "%s%s%04d.bmp", folderStr, slash, i);
				printf("Dumping BMP               to file %s\n", dumpFileName);
				dumpBMP16(dumpFileName, &fileData[fileOffset], 140, 163, (fileType=='A'));
			}
		}
	}

	free(fileData);	
	printf("\ndone.\n\n");

    return 0; // SUCCESS
}
