/*  bmp.c - bitmap functions

	Da Watch Face Tool (dawft)
	dawft: Watch Face Tool for MO YOUNG / DA FIT binary watch face files.

	Copyright 2022 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)

*/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "dawft.h"
#include "bmp.h"


const char * ImgCompressionStr[8] = { "NONE", "RLE_LINE", "RLE_BASIC", "RESERVED", "RESERVED", "RESERVED", "RESERVED", "TRY_RLE" };

//----------------------------------------------------------------------------
//  RGB565 to RGB888 conversion
//----------------------------------------------------------------------------

static RGBTrip RGB565to888(u16 pixel) {	
	pixel = swap_bo_u16(pixel);				// need to reverse the source pixel
	RGBTrip output;
	output.b = (u8)((pixel & 0x001F) << 3);		// first 5 bits
	output.b |= (pixel & 0x001C) >> 3;			// add extra precision of 3 bits
	output.g = (pixel & 0x07E0) >> 3;			// first 6 bits
	output.g |= (pixel & 0x0600) >> 9;			// add extra precision of 2 bits
	output.r = (pixel & 0xF800) >> 8;			// first 5 bits
	output.r |= (pixel & 0xE000) >> 13;			// add extra precision of 3 bits
	return output;
}

static u16 RGB888to565(u8 * buf) {
    u16 output = 0;
	u8 b = buf[0];
	u8 g = buf[1];
	u8 r = buf[2];
	output |= (b & 0xF8) >> 3;            // 5 bits
    output |= (g & 0xFC) << 3;            // 6 bits
    output |= (r & 0xF8) << 8;            // 5 bits
    return output;
}

static u16 RGBTripTo565(RGBTrip * t) {
    u16 output = 0;
    output |= (t->b & 0xF8) >> 3;            // 5 bits
    output |= (t->g & 0xFC) << 3;            // 6 bits
    output |= (t->r & 0xF8) << 8;            // 5 bits
    return output;
}

static u16 ARGB8888to565(u8 * buf) {
	u8 b = buf[0];
	u8 g = buf[1];
	u8 r = buf[2];
	// u8 a = buf[0] // ignore alpha channel
	u16 output = 0;
    output |= (b & 0xF8) >> 3;            // 5 bits
    output |= (g & 0xFC) << 3;            // 6 bits
    output |= (r & 0xF8) << 8;            // 5 bits
    return output;
}

//----------------------------------------------------------------------------
//  SETBMPHEADER - Set up a BMPHeaderClassic or BMPHeaderV4 struct
//----------------------------------------------------------------------------

// Set up a BMP header. bpp must be 16 or 24.
void setBMPHeaderClassic(BMPHeaderClassic * dest, u32 width, u32 height, u8 bpp) {
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
void setBMPHeaderV4(BMPHeaderV4 * dest, u32 width, u32 height, u8 bpp) {
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

int dumpBMP16(char * filename, u8 * srcData, size_t srcDataSize, u32 imgWidth, u32 imgHeight, bool basicRLE) {	
	// Check we have at least a little data available
	if(srcDataSize < 2) {
		printf("ERROR: srcDataSize < 2 bytes!\n");
		return 100;
	}

	// Check if this bitmap has the RLE encoded identifier
	u16 identifier = get_u16(&srcData[0]);
	int isRLE = (identifier == 0x2108);

	BMPHeaderV4 bmpHeader;
	setBMPHeaderV4(&bmpHeader, imgWidth, imgHeight, 16);

	// row width is equal to imageDataSize / imgHeight
	u32 destRowSize = bmpHeader.imageDataSize / imgHeight;

	u8 buf[8192];
	if(destRowSize > sizeof(buf)) {
		printf("ERROR: Image width exceeds buffer size!\n");
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
		remove(filename);
		return 2;
	}

	if(isRLE && !basicRLE) {
		// The newer RLE style has a table at the start with the offsets of each row. (RLE_LINE)
		u8 * lineEndOffset = &srcData[2];
		size_t srcIdx = (2 * imgHeight) + 2; // offset from start of RLEImage to RLEData

		// The srcDataSize must be at least get_u16(&lineEndOffset[imgHeight*2]) 
		size_t dataEnd = get_u16(&lineEndOffset[(imgHeight-1)*2]) - 1;		// This marks the last byte location, plus one.
		if(srcIdx > srcDataSize || dataEnd > srcDataSize) {
			printf("ERROR: Insufficient srcData to decode RLE image.\n");
			fclose(dumpFile);
			remove(filename);
			return 101;
		}

		// for each row
		for(u32 y=0; y<imgHeight; y++) {
			memset(buf, 0, destRowSize);
			u32 bufIdx = 0;

			//printf("line %d, srcIdx %lu, lineEndOffset %d, first color 0x%04x, first count %d\n", y, srcIdx, get_u16(&lineEndOffset[y*2]), 0, srcData[srcIdx+2]);

			
			while(srcIdx < get_u16(&lineEndOffset[y*2])) { // built in end-of-line detection
				u8 count = srcData[srcIdx + 2];
				u8 pixel0 = srcData[srcIdx + 1];
				u8 pixel1 = srcData[srcIdx + 0];

				for(int j=0; j<count; j++) {	// fill out this color
					if(bufIdx+1 >= sizeof(buf)) {
						break;		// don't write past end of buffer. only a problem with erroneous files.      TODO: CHECK THIS... and the other tests I put in here!
					}
					buf[bufIdx] = pixel0;
					buf[bufIdx+1] = pixel1;
					bufIdx += 2;
				}
				srcIdx += 3; // next block of data
			}

			rval = fwrite(buf,1,destRowSize,dumpFile);
			if(rval != destRowSize) {
				fclose(dumpFile);
				remove(filename);
				return 2;
			}
		}
	} else if(isRLE && basicRLE) {
		// This is an OLD RLE style, with no offsets at the start, and no concern for row boundaries. RLE_BASIC.
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
				if(srcIdx+2 >= srcDataSize) {	// Check we have enough data to continue
					printf("ERROR: Insufficient srcData for RLE_BASIC image.\n");
					return 102;
				}
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
				remove(filename);
				return 2;
			}
		}
	} else {
		// Basic RGB565 data
		if(imgHeight * imgWidth * 2 > srcDataSize) {
			printf("ERROR: Insufficient srcData for RGB565 image.\n");
			return 103;
		}
		// for each row
		const u32 srcRowSize = imgWidth * 2;
		size_t srcIdx = 0;
		for(u32 y=0; y<imgHeight; y++) {
			memset(buf, 0, destRowSize);
			u8 * srcPtr = &srcData[srcIdx];
			// for each pixel
			for(u32 x=0; x<imgWidth; x++) {
				u16 pixel = swap_bo_u16(get_u16(&srcPtr[x*2]));
				buf[2*x] = pixel & 0xFF;
				buf[2*x+1] = pixel >> 8;
			}

			rval = fwrite(buf,1,destRowSize,dumpFile);
			if(rval != destRowSize) {
				fclose(dumpFile);
				remove(filename);
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
//  IMG, newIMG, deleteIMG - read bitmap file into basic RGB565 data format
//----------------------------------------------------------------------------

// Does the file have an alpha channel?
// Returns 1 for true, 0 for false, and -1 for error
int bmpFileHasAlpha(char * filename) {
    // read in the whole file
	Bytes * bytes = newBytesFromFile(filename);
	if(bytes==NULL) {
		printf("ERROR: Unable to read file.\n");
		return -1;
	}

	if(bytes->size < BASIC_BMP_HEADER_SIZE) {
		printf("ERROR: File is too small.\n");
		deleteBytes(bytes);
		return -1;
	}

    // process the header
	BMPHeaderClassic * h = (BMPHeaderClassic *)bytes->data;

	int fail = 0;
	if(h->sig != 0x4D42) {
		printf("ERROR: BMP file is not a bitmap.\n");
		fail = 1;
	}

	if(h->dibHeaderSize != 40 && h->dibHeaderSize != 108 && h->dibHeaderSize != 124) {
		printf("ERROR: BMP header format unrecognised.\n");
		fail = 1;
	}

	if(h->planes != 1 || h->reserved1 != 0 || h->reserved2 != 0) {
		printf("ERROR: BMP is unusual, can't read it.\n");
		fail = 1;
	}

	if(h->bpp != 16 && h->bpp != 24 && h->bpp != 32) {
		printf("ERROR: BMP must be RGB565 or RGB888 or ARGB8888.\n");
		fail = 1;
	}
	
	if(h->bpp == 16 && h->compressionType != 3) {
		printf("ERROR: BMP of 16bpp doesn't have bitfields.\n");
		fail = 1;
	}
	
	if((h->bpp == 24 || h->bpp == 32) && (h->compressionType != 0 && h->compressionType != 3)) {
		printf("ERROR: BMP of 24/32bpp must be uncompressed.\n");
		fail = 1;
	}

	if(fail) {
		deleteBytes(bytes);
		return -1;
	}

	// Check if it's a top-down or bottom-up BMP. Normalise height to be positive.
	bool topDown = false;
	if(h->height < 0) {
		topDown = true;
		h->height = -h->height;
	}

	if(h->height < 1 || h->width < 1) {
		printf("ERROR: BMP has no dimensions!\n");
		deleteBytes(bytes);
		return -1;
	}

	if(h->bpp == 32) {
		deleteBytes(bytes);
		return 1;	// recognised alpha format
	}

	deleteBytes(bytes);
	return 0; // not a recognised alpha format
}

// Allocate Img and fill it with pixels from a bmp file. Returns NULL for failure. Delete with deleteImg.
// If bmp file has alpha, and backgroundImg != NULL, use the alpha channel to blend.
Img * newImgFromFile(char * filename, Img * backgroundImg, u32 bpx, u32 bpy) {
    // read in the whole file
	Bytes * bytes = newBytesFromFile(filename);
	if(bytes==NULL) {
		printf("ERROR: Unable to read file.\n");
		return NULL;
	}

	if(bytes->size < BASIC_BMP_HEADER_SIZE) {
		printf("ERROR: File is too small.\n");
		deleteBytes(bytes);
		return NULL;
	}

    // process the header

	BMPHeaderClassic * h = (BMPHeaderClassic *)bytes->data;

	int fail = 0;
	if(h->sig != 0x4D42) {
		printf("ERROR: BMP file is not a bitmap.\n");
		fail = 1;
	}

	if(h->dibHeaderSize != 40 && h->dibHeaderSize != 108 && h->dibHeaderSize != 124) {
		printf("ERROR: BMP header format unrecognised.\n");
		fail = 1;
	}

	if(h->planes != 1 || h->reserved1 != 0 || h->reserved2 != 0) {
		printf("ERROR: BMP is unusual, can't read it.\n");
		fail = 1;
	}

	if(h->bpp != 16 && h->bpp != 24 && h->bpp != 32) {
		printf("ERROR: BMP must be RGB565 or RGB888 or ARGB8888.\n");
		fail = 1;
	}
	
	if(h->bpp == 16 && h->compressionType != 3) {
		printf("ERROR: BMP of 16bpp doesn't have bitfields.\n");
		fail = 1;
	}
	
	if((h->bpp == 24 || h->bpp == 32) && (h->compressionType != 0 && h->compressionType != 3)) {
		printf("ERROR: BMP of 24/32bpp must be uncompressed.\n");
		fail = 1;
	}

	if(fail) {
		deleteBytes(bytes);
		return NULL;
	}

	// Check if it's a top-down or bottom-up BMP. Normalise height to be positive.
	bool topDown = false;
	if(h->height < 0) {
		topDown = true;
		h->height = -h->height;
	}

	if(h->height < 1 || h->width < 1) {
		printf("ERROR: BMP has no dimensions!\n");
		deleteBytes(bytes);
		return NULL;
	}

	u32 rowSize = h->imageDataSize / (u32)h->height;
	if(rowSize < ((u32)h->width * 2)) {		
		// we'll have to calculate it ourselves! size of file is in b->bytes, subtract h->offset.
		h->imageDataSize = (u32)bytes->size - h->offset;
		rowSize = h->imageDataSize / (u32)h->height;
		if(rowSize < ((u32)h->width * 2)) {
			printf("ERROR: BMP imageDataSize (%u) doesn't make sense!\n", h->imageDataSize);
			deleteBytes(bytes);
			return NULL;
		}
	}

	if(h->offset + h->imageDataSize < bytes->size) {
		printf("ERROR: BMP file is too short to contain supposed data.\n");
		deleteBytes(bytes);
		return NULL;
	}

	// Allocate memory to store ImageData and data
	Img * img = malloc(sizeof(Img));
	if(img == NULL) {
		printf("ERROR: Out of memory.\n");
		deleteBytes(bytes);
		return NULL;
	}
	img->w = (u32)h->width;
	img->h = (u32)h->height;
	img->compression = 0;			// No compression
	img->size = img->w * img->h * 2;	// Size is simple to calculate when no compression
	img->data = malloc(img->size);
	if(img->data == NULL) {
		printf("ERROR: Out of memory.\n");
		deleteBytes(bytes);
		deleteImg(img);
		return NULL;
	}

	// Reading the file data depends on bpp
	if(h->bpp == 16) { // RGB565
		// check bitfields are what we expect
		if(bytes->size < sizeof(BMPHeaderClassic)) {
			printf("ERROR: BMP file is too short to contain bitfields.\n");
			deleteBytes(bytes);
			deleteImg(img);
			return NULL;
		}
		if(h->bmiColors[0] != 0xF800 || h->bmiColors[1] != 0x07E0 || h->bmiColors[2] != 0x001F) {
			printf("ERROR: BMP bitfields are not what we expect (RGB565).\n");
			deleteBytes(bytes);
			deleteImg(img);
			return NULL;
		}

		// read in data, row by row
		for(u32 y = 0; y < img->h; y++) {
			u32 row = topDown ? y : (img->h - y - 1);	// row is line in BMP file, y is line in our img
			size_t bmpOffset = h->offset + row * rowSize;
			memcpy(&img->data[y * img->w * 2], &bytes->data[bmpOffset], img->w * 2);
			// swap byte order
			for(u32 j=0; j<(img->w*2); j+=2) {
				u8 * a = &img->data[y*img->w*2 + j];
				u8 * b = &img->data[y*img->w*2 + j + 1];
				u8 temp = *a;
				*a = *b;
				*b = temp;
			}
		}

		// done!
	} else if (h->bpp == 32 && backgroundImg != NULL && h->dibHeaderSize > 40) { 	// ARGB8888 to be blended against backgroundImg
		BMPHeaderV4 * h4 = (BMPHeaderV4 *)&h;
		// check bitfields (if they exist) are what we expect
		if(h->compressionType == 3) {
			if(h4->RGBAmasks[0] != 0xFF000000 || h4->RGBAmasks[1] != 0x00FF0000 || h4->RGBAmasks[2] != 0x0000FF00 || h4->RGBAmasks[3] != 0x000000FF) {
				printf("ERROR: BMP bitfields are not what we expect for 32-bit image (ARGB8888).\n");
				deleteBytes(bytes);
				deleteImg(img);
				return NULL;
			}
		}

	    // read in data, row by row, pixel by pixel
		for(u32 y=0; y < img->h; y++) {
			u32 row = topDown ? y : (img->h - y - 1);
			size_t bmpOffset = h->offset + row * rowSize;
			for(u32 x=0; x < img->w; x++) {
				// get partner pixel from backgroundImg
				u16 pixelIn = get_u16(&backgroundImg->data[2 * backgroundImg->w * (bpy+y) + 2 * (bpx+x)]);
				RGBTrip bgPixel = RGB565to888(pixelIn);

				// get bmp pixel
				u8 b = bytes->data[bmpOffset + x * 4];
				u8 g = bytes->data[bmpOffset + x * 4 + 1];
				u8 r = bytes->data[bmpOffset + x * 4 + 2];
				u8 a = bytes->data[bmpOffset + x * 4 + 3];

				bgPixel.r = (u8)(((u32)(255 - a) * (u32)bgPixel.r + (u32)a * (u32)r) / 255);
				bgPixel.g = (u8)(((u32)(255 - a) * (u32)bgPixel.g + (u32)a * (u32)g) / 255);
				bgPixel.b = (u8)(((u32)(255 - a) * (u32)bgPixel.b + (u32)a * (u32)b) / 255);

				u16 pixelOut = RGBTripTo565(&bgPixel);
				img->data[y * img->w * 2 + 2 * x]     = pixelOut >> 8;
				img->data[y * img->w * 2 + 2 * x + 1] = pixelOut & 0xFF;
			}
		}
	} else { // RGB888 (or ARGB8888 with no background to blend against)
		// check bitfields (if they exist) are what we expect
		if(h->compressionType == 3) {
			if(h->bmiColors[0] != 0xFF0000 || h->bmiColors[1] != 0x00FF00 || h->bmiColors[2] != 0x0000FF) {
				printf("ERROR: BMP bitfields are not what we expect (RGB888).\n");
				deleteBytes(bytes);
				deleteImg(img);
				return NULL;
			}
		}

	    // read in data, row by row, pixel by pixel
		for(u32 y=0; y < img->h; y++) {
			u32 row = topDown ? y : (img->h - y - 1);
			size_t bmpOffset = h->offset + row * rowSize;
			for(u32 x=0; x < img->w; x++) {
				u16 pixel;
				if(h->bpp == 24) {
					pixel = RGB888to565(&bytes->data[bmpOffset + x * 3]);
				} else { // h->bpp == 32
					pixel = ARGB8888to565(&bytes->data[bmpOffset + x * 4]);
				}
				img->data[y * img->w * 2 + 2 * x]     = pixel >> 8;
				img->data[y * img->w * 2 + 2 * x + 1] = pixel & 0xFF;
			}
		}
	}

	// Free filedata
	deleteBytes(bytes);

	// Return Img
	return img;
}

// Delete an Img. Safe to use on already deleted Img.
Img * deleteImg(Img * i) {
    if(i != NULL) {
		if(i->data != NULL) {
			free(i->data);
			i->data = NULL;
		}		
		free(i);
        i = NULL;
    }
	return i;
}

Img * cloneImg(Img * i) {
	// Allocate memory to store ImageData and data
	Img * img = malloc(sizeof(Img));
	if(img == NULL) {
		printf("ERROR: Out of memory.\n");
		return NULL;
	}	
	img->w = i->w;
	img->h = i->h;
	img->compression = i->compression;
	img->size = i->size;
	img->data = malloc(img->size);
	if(img->data == NULL) {
		printf("ERROR: Out of memory.\n");
		deleteImg(img);
		return NULL;
	}
	memcpy(img->data, i->data, img->size);
	return img;
}

//----------------------------------------------------------------------------
//  COMPRESS IMG - Compress using (RLE_LINE) if it shrinks the size
//----------------------------------------------------------------------------
 
int compressImg(Img * img) {
	// Check it is a raw img we got
	if(img == NULL || img->compression != 0) {
		return 100;
	}

	// Check the image isn't too big
	//               ...header size..   ..minimum rle units. 3bpu 
	size_t minSize = (2 + img->h * 2) + (img->w + 255) / 255 * 3 * img->h;

	if(minSize > 65535) { // we can't store 16-bit offsets in a bigger file
		printf("Note: Image too large to be RLE_LINE encoded.\n");
		return 101;
	}

	size_t maxSize = (2 + img->h * 2) + (img->w * img->h * 3); // worst case

	// Allocate a stack of RAM to keep the image in
	u8 * buf = malloc(maxSize);
	if(buf==NULL) {
		printf("ERROR: Out of memory (allocating %zu bytes).\n", maxSize);
		return 102;
	}

	// Set identifier as RLE image
	buf[0] = 0x08;
	buf[1] = 0x21;

	// Calculate offset
	u32 offset = 2 + 2 * img->h;	// id is 2 bytes, offsets are u16 and are of the end of line / running offset

	// For each line
	for(u32 y=0; y<img->h; y++) {
		u8 prev[2] = { 0 };
		u8 runLength = 0;
		for(u32 x=0; x<img->w; x++) {
			u8 curr[2];
			curr[0] = img->data[y*img->w*2 + x*2];
			curr[1] = img->data[y*img->w*2 + x*2 + 1];
			if(x==0) {
				prev[0] = curr[0];
				prev[1] = curr[1];
				runLength = 1;
				continue;
			} 
			if(curr[0] != prev[0] || curr[1] != prev[1]) {
				// end the run and start a new one
				buf[offset]   = prev[0];
				buf[offset+1] = prev[1];
				buf[offset+2] = runLength;
				offset += 3;
				prev[0] = curr[0];
				prev[1] = curr[1];
				runLength = 1;
			} else {
				// increase the run
				runLength ++;
				if(runLength==255) {
					// save and restart the run
					buf[offset]   = prev[0];
					buf[offset+1] = prev[1];
					buf[offset+2] = runLength;
					offset += 3;
					runLength = 0;
				}
			}
		}
		// save remaining run, if anything
		if(runLength > 0) {
			buf[offset]   = prev[0];
			buf[offset+1] = prev[1];
			buf[offset+2] = runLength;
			offset += 3;
		}
		// save offset
		if(offset > 65535) {	// Image exceeded RLE_LINE capabilities. We can't store offsets greater than 16-bits!
			free(buf);
			return 0; // success, but not compressed
		}
		set_u16(&buf[2+y*2], (u16)offset);
	}

	// Check if the size is better
	if(offset >= img->size) {
		free(buf);
		return 0; // success, but not compressed
	}

	// Free the original data and store the new data
	buf = realloc(buf, offset);	// remove any excess memory allocation
	if(buf == NULL) {
		printf("ERROR: realloc() failure.\n");
		return 5;
	}
	
	free(img->data);
	img->data = buf;
	img->size = offset;
	img->compression = RLE_LINE;
	return 0;
}

