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


//----------------------------------------------------------------------------
//  RGB565 to RGB888 conversion
//----------------------------------------------------------------------------

static RGBTrip RGB565to888(u16 pixel) {	
	pixel = swap_bo_u16(pixel);				// need to reverse the source pixel
	RGBTrip output;
	output.r = (u8)((pixel & 0x001F) << 3);		// first 5 bits
	output.r |= (pixel & 0x001C) >> 3;			// add extra precision of 3 bits
	output.g = (pixel & 0x07E0) >> 3;			// first 6 bits
	output.g |= (pixel & 0x0600) >> 9;			// add extra precision of 2 bits
	output.b = (pixel & 0xF800) >> 8;			// first 5 bits
	output.b |= (pixel & 0xE000) >> 13;			// add extra precision of 3 bits
	return output;
}

static u16 RGB888to565(RGBTrip pixel) {
    u16 output = 0;
    output |= (pixel.r & 0xF8) >> 3;            // 5 bits
    output |= (pixel.g & 0xFC) << 3;            // 6 bits
    output |= (pixel.b & 0xF8) << 8;            // 5 bits
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

int dumpBMP16(char * filename, u8 * srcData, size_t srcDataSize, u32 imgWidth, u32 imgHeight, u8 oldRLE) {	
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

		// The srcDataSize must be at least get_u16(&lineEndOffset[imgHeight*2]) 
		size_t dataEnd = get_u16(&lineEndOffset[(imgHeight-1)*2]) - 1;		// This marks the last byte location, plus one.
		if(srcIdx > srcDataSize || dataEnd > srcDataSize) {
			printf("ERROR: Insufficient srcData to decode RLE image\n");
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
				return 2;
			}
		}
	} else if(isRLE && oldRLE) {        // TODO: Fix read (buffer overun) here...
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
				if(srcIdx+2 >= srcDataSize) {	// Check we have enough data to continue
					printf("ERROR: Insufficient srcData for OldRLE image\n");
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
				return 2;
			}
		}
	} else {
		// Basic RGB565 data
		if(imgHeight * imgWidth * 2 > srcDataSize) {
			printf("ERROR: Insufficient srcData for RGB565 image\n");
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

// Allocate Img and fill it with pixels from a bmp file. Returns NULL for failure. Delete with deleteImg.
Img * newImgFromFile(char * filename) {
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
	if(h->sig != 0x424D) {
		printf("ERROR: BMP file is not a bitmap.\n");
		fail = 1;
	}

	if(h->dibHeaderSize != 40 && h->dibHeaderSize != 108 && h->dibHeaderSize != (108+12)) {
		printf("ERROR: BMP header format unrecognised.\n");
		fail = 1;
	}

	if(h->planes != 1 || h->reserved1 != 0 || h->reserved2 != 0) {
		printf("ERROR: BMP is unusual, can't read it.\n");
		fail = 1;
	}

	if(h->bpp != 16 && h->bpp != 24) {
		printf("ERROR: BMP must be RGB565 or RGB888.\n");
		fail = 1;
	}
	
	if(h->bpp == 16 && h->compressionType != 3) {
		printf("ERROR: BMP of 16bpp doesn't have bitfields.\n");
		fail = 1;
	}
	
	if(h->bpp == 24 && h->compressionType != 0) {
		printf("ERROR: BMP of 24bpp must be uncompressed.\n");
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
		printf("ERROR: BMP imageDataSize doesn't make sense.\n");
		deleteBytes(bytes);
		return NULL;
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
	img->compressionType = 0;			// No compression
	img->size = img->w * img->h * 2;	// Size is simple to calculate when no compression
	img->data = malloc(img->w * img->h * 2);
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
		for(u32 i=0; i<img->h; i++) {
			u32 row = topDown ? i : (img->h - i);
			size_t bmpOffset = h->offset + row * rowSize;
			memcpy(&img->data[row * img->w], &bytes->data[bmpOffset], img->w * img->h * 2);
		}

		// done!
	} else { // RGB888
	    // read in data, row by row, pixel by pixel
		for(u32 i=0; i<img->h; i++) {
			u32 row = topDown ? i : (img->h - i);
			size_t bmpOffset = h->offset + row * rowSize;
			for(u32 x=0; x<img->w; x++) {
				u16 pixel = RGB888to565(((RGBTrip *)(&bytes->data[bmpOffset]))[x]);
				img->data[bmpOffset + 2 * x]     = (pixel & 0xFF00) >> 8;
				img->data[bmpOffset + 2 * x + 1] = pixel & 0xFF;
			}
		}
	}

	// Free filedata
	deleteBytes(bytes);

	// Return Img
	return img;
}

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
