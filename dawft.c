/*  dawft.c

	Da Watch Face Tool (dawft)
	dawft: Watch Face Tool for MO YOUNG / DA FIT binary watch face files.
	Typically obtained by DA FIT app from api.moyoung.com

	Tested on the following watches:
		C20		MOY-QHF3-2.0.4		240x280

	File identifier (first byte of file) is 0x04, 0x81 or 0x84 for these faces.
	File type (not directly specified in binary file) we call type A, B or C.

	Copyright 2022 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)

*/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>		// for mkdir()

#include "dawft.h"
#include "bmp.h"


//----------------------------------------------------------------------------
//  WATCH TYPES EXAMPLES
//----------------------------------------------------------------------------

typedef struct _WatchType {
	const char * tpls;		// Types that are requested from the watch face api.
	u16 width;				// Screen width in pixels.
	u16 height;				// Screen height in pixels.
	char fileType;			// A, B or C.
	const char * model;		// Example watch model.
	const char * code;		// Example watch code. Starts with MOY-.
} WatchType;

static WatchType watchTypes[] = {
	{ "1", 		240, 240, 'A', "?", 	"?" },			// square face
	{ "6", 		240, 240, 'A', "?", 	"?" },			// round face
	{ "7", 		240, 240, 'A', "?", 	"?" },			// round face
	{ "8", 		240, 240, 'A', "?", 	"?" },			// 
	{ "13", 	240, 240, 'B', "?", 	"?" },			// 
	{ "19", 	240, 240, 'B', "?", 	"?" },			// round face
	{ "20", 	240, 240, 'B', "?", 	"?" },			// 
	{ "25", 	360, 360, 'B', "?", 	"?" },			// round face
	{ "27", 	240, 240, 'A', "?", 	"?" },			// 
	{ "28", 	240, 240, 'A', "?", 	"?" },			// 
	{ "29", 	240, 240, 'A', "?", 	"?" },			// 
	{ "30", 	240, 240, 'B', "?", 	"?" },			// 
	{ "33", 	240, 280, 'C', "C20", 	"QHF3" },		// 
	{ "34", 	240, 280, 'B', "?", 	"?" },			// 
	{ "36", 	240, 295, 'B', "?", 	"?" },			// 
	{ "38", 	240, 240, 'C', "?", 	"?" },			// round face
	{ "39", 	240, 240, 'C', "?", 	"?" },			// square face
	{ "40", 	320, 385, 'C', "?", 	"?" },			// round face	
	{ "41", 	360, 360, 'C', "?", 	"?" },			// round face
	{ "44", 	240, 283, 'C', "?", 	"?" },			// 
	{ "45", 	240, 295, 'C', "?", 	"?" },			// 
	{ "46", 	240, 288, 'C', "?", 	"?" },			// 
	{ "47", 	200, 320, 'C', "?", 	"?" },			// 
	{ "48", 	390, 390, 'C', "?", 	"?" },			// round face
	{ "49", 	320, 380, 'C', "?", 	"?" },			// 
	{ "51", 	356, 400, 'C', "?", 	"?" },			// 
	{ "52", 	454, 454, 'C', "?", 	"?" },			// round face
	{ "53", 	368, 448, 'C', "?", 	"?" },			// 
	{ "55", 	172, 320, 'C', "?", 	"?" },			// 
	{ "56", 	240, 286, 'C', "?", 	"?" },			// Some show 240x280, but have y-offset of 3.
	{ "59", 	320, 386, 'C', "?", 	"?" },			// Some show 320x380, but have y-offset of 3.
	{ "60", 	240, 284, 'C', "?", 	"?" },			// Some show 240x280, but have y-offset of 2.
};


//----------------------------------------------------------------------------
//  DATA READING AND BYTE ORDER
//----------------------------------------------------------------------------

static int systemIsLittleEndian() {				// return 0 for big endian, 1 for little endian.
    volatile uint32_t i=0x01234567;
    return (*((volatile uint8_t*)(&i))) == 0x67;
}


//----------------------------------------------------------------------------
//  BINARY FILE STRUCTURE
//----------------------------------------------------------------------------

/***

fileType	FaceData size	Offset table start		Decompression required for offset table		RLE bitmap has row index	Different resolutions?
TYPE A 		6				200						No											No							No - always 240x240
TYPE B		10				400						Yes	(LZO?)									N/A							Yes
TYPE C 		10				400						No											Yes							Yes


For reference:

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
	u16 sizes[250];			// sizes of the bitmap data, in bytes. Unreliable. at 1600 is stored the animationFrame count. i.e. sizes[200]
} FaceHeaderA;				// size is 1700 bytes

***/

#pragma pack (push)
#pragma pack (1)

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
	u16 sizes[250];			// sizes of the bitmap data, in bytes. For Type B, sizes are the uncompressed sizes. Unreliable. Table starts at 1400. Animation frame count is at sizes[0].
} FaceHeader;				// size is 1900 bytes

#pragma pack (pop)

// Extra info about a binary file, which is not easy to access from binary file header
typedef struct _ExtraFileInfo {
	char fileType;
	char reservedPadding;
	u16 animationFrames;
} ExtraFileInfo;

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
	const char * description;
} DataType;

// digits: only w, h of first digit. 0-9.
static const DataType dataTypes[] = {	
	{ 0x00, "BACKGROUNDS", 		10, "Background (10 parts of 240x24). May contain example time (will be overwritten). Seen in Type A faces." },
	{ 0x01, "BACKGROUND", 		1, 	"Background image, usually width and height of screen. Seen in Type B & C faces." },
	{ 0x10, "MONTH_NAME", 		12, "JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC." },
	{ 0x11, "MONTH_NUM", 		10, "Month, digits." },
	{ 0x12, "YEAR", 			10, "Year, 2 digits, left aligned." },
	{ 0x30, "DAY_NUM", 			10, "Day number of the month, digits." },
	{ 0x40, "TIME_H1", 			10, "Hh:mm" },
	{ 0x41, "TIME_H2", 			10, "hH:mm" },
	{ 0x43, "TIME_M1", 			10, "hh:Mm" },
	{ 0x44, "TIME_M2", 			10, "hh:mM" },
	{ 0x45, "TIME_AM", 			1,	"'AM'." },
	{ 0x46, "TIME_PM", 			1,	"'PM'." },
	{ 0x60, "DAY_NAME", 		7,	"SUN, MON, TUE, WED, THU, FRI, SAT." },
	{ 0x61, "DAY_NAME_CN", 		7,	"SUN, MON, TUE, WED, THU, FRI, SAT, chinese symbol option." },
	{ 0x62, "STEPS", 			10,	"Step count, left aligned, digits." },
	{ 0x63, "STEPS_CA", 		10,	"Step count, centre aligned, digits." },
	{ 0x64, "STEPS_RA", 		10,	"Step count, right aligned, digits." }, 
	{ 0x65, "HR", 				10, "Heart rate, left aligned, digits. (Assumed)." },
	{ 0x66, "HR_CA", 			10, "Heart rate, centre aligned, digits. (Assumed)." },
	{ 0x67, "HR_RA", 			10, "Heart rate, right aligned, digits." },
	{ 0x68, "KCAL", 			10, "kCals, left aligned, digits." },
	{ 0x6b, "MONTH_NUM_B", 		10,	"Month, digits, alternate." },
	{ 0x6c, "DAY_NUM_B", 		10,	"Day number of the month, digits, alternate." },
	{ 0x70, "STEPS_PROGBAR",	11, "Steps progess bar 0,10,20...100%. 11 frames." },
	{ 0x71, "STEPS_LOGO", 		1,	"Step count, static logo." },
	{ 0x72, "STEPS_B", 			10, "Step count, left aligned, digits, alternate." },
	{ 0x73, "STEPS_B_CA", 		10, "Step count, centre aligned, digits, alternate." },
	{ 0x74, "STEPS_B_RA", 		10, "Step count, right aligned, digits, alternate." },
	{ 0x76, "STEPS_GOAL", 		1,  "Step goal, left aligned, digits." },
	{ 0x80, "HR_PROGBAR", 		11, "Heart rate, progress bar 0,10,20...100%. 11 frames." },
	{ 0x81, "HR_LOGO", 			1,  "Heart rate, static logo." },
	{ 0x82, "HR_B", 			10,	"Heart rate, left aligned, digits, alternate." },
	{ 0x83, "HR_B_CA", 			10, "Heart rate, centre aligned, digits, alternate." },
	{ 0x84, "HR_B_RA", 			10, "Heart rate, right aligned, digits, alternate." },
	{ 0x90, "KCAL_PROGBAR", 	11, "kCals progress bar 0,10,20...100%. 11 frames." },
	{ 0x91, "KCAL_LOGO", 		1,  "kCals, static logo." },
	{ 0x92, "KCAL_B", 			10, "kCals, left aligned, digits." },
	{ 0x93, "KCAL_B_CA", 		10, "kCals, centre aligned, digits." },
	{ 0x94, "KCAL_B_RA", 		10, "kCals, right aligned, digits." },
	{ 0xA0, "DIST_PROGBAR", 	11, "Distance progress bar 0,10,20...100%. 11 frames." },
	{ 0xA1, "DIST_LOGO", 		1, 	"Distance, static logo." },
	{ 0xA2, "DIST", 			10, "Distance, left aligned, digits." },
	{ 0xA3, "DIST_CA", 			10, "Distance, centre aligned, digits." },
	{ 0xA4, "DIST_RA", 			10, "Distance, right aligned, digits." },
	{ 0xA5, "DIST_KM", 			1,  "Distance unit 'KM'." },
	{ 0xA6, "DIST_MI", 			1,  "Distance unit 'MI'." },
	{ 0xC0, "BTLINK_UP", 		1,  "Bluetooth link up / connected." },
	{ 0xC1, "BTLINK_DOWN", 		1,	"Bluetooth link down / not connected." },
	{ 0xCE, "BATT_IMG", 		1,	"Battery level image." },
	{ 0xD0, "BATT_IMG_B", 		1,	"Battery level image, alternate." },
	{ 0xD1, "BATT_IMG_C", 		1,  "Battery level image, alternate." },
	{ 0xD2, "BATT", 			10, "Battery level, left aligned, digits. (Assumed)." },
	{ 0xD3, "BATT_CA", 			10, "Battery level, centre aligned, digits." },
	{ 0xD4, "BATT_RA", 			10, "Battery level, right aligned, digits." },
	{ 0xDA, "BATT_IMG_D", 		1,  "Battery level image, alternate." },
	{ 0xD8, "WEATHER_TEMP_CA", 	10, "Weather temperature, centre aligned, digits." },
	{ 0xF0, "SEPERATOR", 		1,	"Static image used as date or time seperator e.g. / or :." },
	{ 0xF1, "HAND_HOUR", 		1,	"Analog time hour hand, at 1200 position." },
	{ 0xF2, "HAND_MINUTE", 		1,  "Analog time minute hand, at 1200 position." },
	{ 0xF3, "HAND_SEC", 		1,	"Analog time second hand, at 1200 position." },
	{ 0xF4, "HAND_PIN_UPPER", 	1,  "Top half of analog time centre pin." },
	{ 0xF5, "HAND_PIN_LOWER", 	1,  "Bottom half of analog time centre pin." },
	{ 0xF6, "TAP_TO_CHANGE", 	3,  "Series of images. Tap to change. Count is specified by animationFrames." },		// On #3156, #5315. #3135 has it as 3 images. Frame count at 1400 (Type C) or 1600 (Type A).
	{ 0xF7, "ANIMATION",	 	7,  "Animation. Count is specified by animationFrames." }, 							// #3145, #163.  #3087, #3088 has it as a 7-frame animation. #3163 has it as a 36 frame animation. Frame count at 1400 (Type C) or 1600 (Type A)..
	{ 0xF8, "ANIMATION_F8",   	10, "Animation. Count is specified by animationFrames." }, 				// #3085, #3086. #3112 has it as a 14-frame animation. #3248 has it as a 10-frame animation. Frame count at 1400 (Type C) or 1600 (Type A).
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

static void printTypes() {
	printf("DATA TYPES FOR BINARY WATCH FACE FILES\nNote: Width and height of digits is of a single digit (bitmap).\n\n");
	printf("Code  Name              Count  Description\n");	
	u32 typeCount = sizeof(dataTypes)/sizeof(DataType);
	for(u32 i=0; i<typeCount; i++) {
		printf("0x%02x  %-16s  %2u     %s\n", dataTypes[i].type, dataTypes[i].str, dataTypes[i].count, dataTypes[i].description);	
	}
	printf("\n");
}


// Find the faceData index given an offset index
static int getFaceDataIndexFromOffsetIndex(int offsetIndex, FaceHeader * h, ExtraFileInfo * xfi) {
	int matchIdx = -1;
	for(int fdi=0; fdi < h->dataCount; fdi++) {
		int count = 1;
		u8 type = h->faceData[fdi].type;			// we only care about type because it affects count
		int typeIdx = getDataTypeIdx(type);
		if(typeIdx >= 0) {
			if(type >= 0xF6 && type <= 0xF8) {
				count = xfi->animationFrames;
			} else {
				count = dataTypes[typeIdx].count;
			}
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
	char * filename = "";
	char * foldername = "";
	char * filemode = "rb";
	enum _MODE {
		HELP,
		INFO,
		DUMP,
		CREATE,
		PRINT_TYPES,
	} mode = HELP;
	bool raw = false;
	char fileType = 0;

	// display basic program header
    printf("\n%s\n\n","dawft: Watch Face Tool for MO YOUNG / DA FIT binary watch face files.");
    
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
  
	// read mode argument
	if(argc >= 2) {
		if(streq(argv[1], "dump")) {
			mode = DUMP;
			filemode="rb";
		} else if(streq(argv[1], "create")) {
			mode = CREATE;
			filemode = "wb";
		} else if(streq(argv[1], "info")) {
			mode = INFO;
			filemode="rb";
		} else if(streq(argv[1], "print_types")) {
			mode = PRINT_TYPES;
		}
	}

	// display type information
	if(mode==PRINT_TYPES) {
		printTypes();
		return 0;
	}

	// display help
    if(argc<3 || mode == HELP) {
		printf("Usage:   %s MODE [OPTIONS] [FILENAME]\n\n",basename);
		printf("\t%s\n","MODE:");
		printf("\t%s\n","    info               Display info about binary file.");
		printf("\t%s\n","    dump               Dump data from binary file to folder.");
		printf("\t%s\n","    create             Create binary file from data in folder.");
		printf("\t%s\n","    print_types        Print the data type codes and description.");
		printf("\t%s\n","OPTIONS:");
		printf("\t%s\n","    folder=FOLDERNAME  Folder to dump data to/read from. Defaults to the face design number.");
		printf("\t%s\n","                       Required for create.");
		printf("\t%s\n","    raw=true           When dumping, dump raw files. Default is false.");
		printf("\t%s\n","    fileType=C         Specify type of binary file (A, B or C). Default is to autodetect or type A.");
		printf("\t%s\n","FILENAME               Binary watch face file for input (or output). Required for info/dump/create.");
		printf("\n");
		return 0;
    }
		
	// read command-line parameters
	for(int i=2; i<argc; i++) {
		if(streq(argv[i], "raw=true")) {
			raw = 1;
		} else if(streq(argv[i], "raw=false")) {
			raw = 0;
		} else if(streqn(argv[i], "raw=")) {
			printf("ERROR: Invalid raw=\n");
			return 1;
		} else if(streq(argv[i], "fileType=A")) {
			fileType = 'A';
		} else if(streq(argv[i], "fileType=B")) {
			fileType = 'B';
		} else if(streq(argv[i], "fileType=C")) {
			fileType = 'C';
		} else if(streqn(argv[i], "fileType=")) {
			printf("ERROR: Invalid fileType=\n");
			return 1;
		} else if(streqn(argv[i],"folder=") && strlen(argv[i]) >= 8) {
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

	// Save important info to xfi struct
	ExtraFileInfo xfi = { .fileType = fileType, .animationFrames = 0 };

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

	snprintf(lineBuf, sizeof(lineBuf), "fileType        %c\n", fileType);
	strcat(watchFaceStr, lineBuf);

	snprintf(lineBuf, sizeof(lineBuf), "fileID          0x%02x\n", fileData[0]);
	strcat(watchFaceStr, lineBuf);



	// Load header struct from file
	FaceHeader header;
	setHeader(&header, fileData, xfi.fileType);
	FaceHeader * h = &header;

	// Print header info
	snprintf(lineBuf, sizeof(lineBuf), "dataCount       %u\n", h->dataCount);
	strcat(watchFaceStr, lineBuf);
	snprintf(lineBuf, sizeof(lineBuf), "blobCount       %u\n", h->blobCount);
	strcat(watchFaceStr, lineBuf);
	snprintf(lineBuf, sizeof(lineBuf), "faceNumber      %u\n", h->faceNumber);
	strcat(watchFaceStr, lineBuf);
	snprintf(lineBuf, sizeof(lineBuf), "padding         %02x%02x%02x%02x%02x\n", h->padding[0], h->padding[1], h->padding[2], h->padding[3], h->padding[4] );
	strcat(watchFaceStr, lineBuf);

	// Print faceData header info
	FaceData * background = NULL;
	FaceData * backgrounds = NULL;
	int myDataCount = 0;
	
	for(u32 i=0; i<(sizeof(h->faceData)/sizeof(h->faceData[0])); i++) {
		if(h->faceData[i].type != 0 || i==0) {		// some formats use type 0 in position 0 as background
			FaceData * fd = &h->faceData[i];			
			snprintf(lineBuf, sizeof(lineBuf), "faceData        0x%02x  %04u  %-15s %4u %4u %4u %4u\n",
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
			if(fd->type >= 0xF6 && fd->type <= 0xF8) {
				if(fileType == 'A') {
					xfi.animationFrames = h->sizes[200];
				} else {
					xfi.animationFrames = h->sizes[0];		// animation frame count is stored here for type C. TODO test assumption this is correct for Type B
				}
			}
		}
	}

	if(xfi.animationFrames != 0) {
		snprintf(lineBuf, sizeof(lineBuf), "animationFrames %u\n", xfi.animationFrames);
		strcat(watchFaceStr, lineBuf);
	}

	printf("%s", watchFaceStr);		// display all the important data

	if(background == NULL && backgrounds == NULL) {
		printf("WARNING: No background found.\n");
	}

	int fail = 0;

	if(myDataCount != h->dataCount) {
		printf("myDataCount     %u\n", myDataCount);
		if(myDataCount < h->dataCount) {
			printf("WARNING: myDataCount < dataCount!\n");
		}
	}

	// Count offsets to verify number of blobs
	int myBlobCount = 1;	// there is always one offset, of 0, at offsets[0]
	for(u32 i=1; i<250; i++) {
		if(h->offsets[i] != 0) {
			myBlobCount += 1;
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

	if(myBlobCount != h->blobCount) {
		printf("myBlobCount     %u\n", myBlobCount);
		if(myBlobCount < h->blobCount) {
			printf("WARNING: myBlobCount < blobCount!\n");
		}
	}

	u8 nothing[5] = { 0, 0, 0, 0, 0 };
	if(memcmp(h->padding, nothing, 5) != 0) {
		printf("WARNING: padding are not 0!\n");
	}

	// Check if we are in DUMP mode
	if(mode==DUMP) {
		if(fileType == 'B') {
			// What compression method is used in type B files? LZO?
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
		char * slash = DIR_SEPERATOR;

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
		for(int i=0; i<h->blobCount; i++) {
			// determine file offset
			u32 fileOffset = headerSize;
			fileOffset += h->offsets[i];
			// printf("offset %u is %u\n", i, h->offsets[i]);
			
			// get faceData index from offset index
			int fdi = getFaceDataIndexFromOffsetIndex(i, h, &xfi);

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
				if(h->faceData[fdi].type == 0x00 && (fileType=='A') && (h->faceData[fdi].w != 240 || h->faceData[fdi].h != 24)) {
					// override width and height
					h->faceData[fdi].w = 240;
					h->faceData[fdi].h = 24;
					printf("WARNING: Overriding width and height with 240x24 for backgrounds of type 0x00\n");
				}
				snprintf(dumpFileName, sizeof(dumpFileName), "%s%s%04d.bmp", folderStr, slash, i);
				printf("Dumping BMP               to file %s\n",  dumpFileName);
				dumpBMP16(dumpFileName, &fileData[fileOffset], fileSize-fileOffset, h->faceData[fdi].w, h->faceData[fdi].h, (fileType=='A'));
			} else if(i == (h->blobCount - 1)) {
				// this is a small preview image of 140x163, used when selecting backgrounds (for 240x280 images)
				snprintf(dumpFileName, sizeof(dumpFileName), "%s%s%04d.bmp", folderStr, slash, i);
				printf("Dumping BMP               to file %s\n", dumpFileName);
				dumpBMP16(dumpFileName, &fileData[fileOffset], fileSize-fileOffset, 140, 163, (fileType=='A'));
			}
		}
	}

	free(fileData);	
	printf("\ndone.\n\n");

    return 0; // SUCCESS
}
