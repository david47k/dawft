# Da Watch Face Tool
Watch Face Tool for MO YOUNG / DA FIT binary watch face files. Allows you to dump (unpack) or create the files.

## License
GNU General Public License version 2 or any later version (GPL-2.0-or-later).

## Usage
Make sure to disconnect your watch first (it will not work if it already has a bluetooth connection). Easiest to just turn off bluetooth on your phone.

```
Usage:   dawft MODE [OPTIONS] [FILENAME]

  MODE:
    info               Display info about binary file.
    dump               Dump data from binary file to folder.
    create             Create binary file from data in folder.
    print_types        Print the data type codes and description.
  OPTIONS:
    folder=FOLDERNAME  Folder to dump data to/read from. Defaults to the face design number.
                       Required for create.
    raw=true           When dumping, dump raw files. Default is false.
    fileType=C         Specify type of binary file (A, B or C). Default is to autodetect or type A.
  FILENAME               Binary watch face file for input (or output). Required for info/dump/create.
```

To build an example watch face:
```
dawft create folder=example1 example1.bin
```

## watchface.txt format
Try dumping an existing watch face to get an idea of how watchface.txt works for your particular watch model.

Also try looking at examples.

Each line starts with a command and is followed by parameters.

e.g.
```
fileType       C
fileID         0x81
dataCount      18
blobCount      44
faceNumber     50001
#              TYPE  INDEX         X    Y    W    H     FILENAME
faceData       0x01    000         0    0  240  280     background000.bmp
faceData       0x12    001        39   11   12   18     db000.bmp
```

Command    | Parameters  | Description
-----------|-------------|------------
fileType   | A or B or C | What type of binary watch face file to create. Depends on watch model.
fileID     | 0x81 or 0x04 or 0x84 | Exact meaning yet to be determined.
dataCount  | integer | How many blob (bitmap) objects to store in the file.
faceNumber | integer | Design number of this face, just use a basic number that won't clash with an existing face. e.g. 50000.
animationFrames | integer | Number of frames in the animation (if there is one).
blobCompression (multiple) | BlobTableIndex CompressionType | What compression to use for each blob. Supported compression types are NONE, RLE_LINE, RLE_BASIC, and TRY_RLE.
faceData (multiple) | DataType BlobTableIndex X Y Width Height Filename | What to display on the watch face. 

### faceData parameters:

For example, looking at the following line:
```
#              TYPE  INDEX         X    Y    W    H     FILENAME
faceData       0x01    000         0    0  240  280     background000.bmp
```
Data Type = 0x01, so this is a "Background image, usually width and height of screen. Seen in Type B & C faces".  
Blob Table Index = 000, so first item stored in the blob table.  
X = 0, Y = 0. The bitmap will be displayed starting at top left of watch face.  
Width = 240, Height = 280. The bitmap will fill that area of the watch face.  
Filename = background000.bmp. This is where the bitmap data will be loaded from and stored in the blob table.  

Looking at the line:
```
#              TYPE  INDEX         X    Y    W    H     FILENAME
faceData       0x12    001        39   11   12   18     db000.bmp
```
Data Type = 0x12. This is a "Year, 2 digits, left aligned".  
Blob Table Index = 001, so starting as the second item stored in the blob table. As this is a digit, it will take up positions 001 to 010 for digits 0 to 9.  
X = 39, Y = 11. This is where the first digit of the year will be displayed on the watch face. The second digit will be displayed to the right of the first digit.  
Width = 12, Height = 18. Each digit will use this much space.  
Filename = db000.bmp. This is the bitmap data for the first of the 10 digits 0-9. The rest of the bitmap data for the digits will be automatically loaded from db001.bmp, db002.bmp etc.  


### Data types
```
DATA TYPES FOR BINARY WATCH FACE FILES
Note: Width and height of digits is of a single digit (bitmap). Digits will be printed with 2px spacing.

Code  Name              Count  Description
0x00  BACKGROUNDS       10     Background (10 parts of 240x24). May contain example time (will be overwritten). Seen in Type A faces.
0x01  BACKGROUND         1     Background image, usually width and height of screen. Seen in Type B & C faces.
0x10  MONTH_NAME        12     JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC.
0x11  MONTH_NUM         10     Month, digits.
0x12  YEAR              10     Year, 2 digits, left aligned.
0x30  DAY_NUM           10     Day number of the month, digits.
0x40  TIME_H1           10     Hh:mm
0x41  TIME_H2           10     hH:mm
0x43  TIME_M1           10     hh:Mm
0x44  TIME_M2           10     hh:mM
0x45  TIME_AM            1     'AM'.
0x46  TIME_PM            1     'PM'.
0x60  DAY_NAME           7     SUN, MON, TUE, WED, THU, FRI, SAT.
0x61  DAY_NAME_CN        7     SUN, MON, TUE, WED, THU, FRI, SAT, chinese symbol option.
0x62  STEPS             10     Step count, left aligned, digits.
0x63  STEPS_CA          10     Step count, centre aligned, digits.
0x64  STEPS_RA          10     Step count, right aligned, digits.
0x65  HR                10     Heart rate, left aligned, digits. (Assumed).
0x66  HR_CA             10     Heart rate, centre aligned, digits. (Assumed).
0x67  HR_RA             10     Heart rate, right aligned, digits.
0x68  KCAL              10     kCals, left aligned, digits.
0x6b  MONTH_NUM_B       10     Month, digits, alternate.
0x6c  DAY_NUM_B         10     Day number of the month, digits, alternate.
0x70  STEPS_PROGBAR     11     Steps progess bar 0,10,20...100%. 11 frames.
0x71  STEPS_LOGO         1     Step count, static logo.
0x72  STEPS_B           10     Step count, left aligned, digits, alternate.
0x73  STEPS_B_CA        10     Step count, centre aligned, digits, alternate.
0x74  STEPS_B_RA        10     Step count, right aligned, digits, alternate.
0x76  STEPS_GOAL        10     Step goal, left aligned, digits.
0x80  HR_PROGBAR        11     Heart rate, progress bar 0,10,20...100%. 11 frames.
0x81  HR_LOGO            1     Heart rate, static logo.
0x82  HR_B              10     Heart rate, left aligned, digits, alternate.
0x83  HR_B_CA           10     Heart rate, centre aligned, digits, alternate.
0x84  HR_B_RA           10     Heart rate, right aligned, digits, alternate.
0x90  KCAL_PROGBAR      11     kCals progress bar 0,10,20...100%. 11 frames.
0x91  KCAL_LOGO          1     kCals, static logo.
0x92  KCAL_B            10     kCals, left aligned, digits.
0x93  KCAL_B_CA         10     kCals, centre aligned, digits.
0x94  KCAL_B_RA         10     kCals, right aligned, digits.
0xa0  DIST_PROGBAR      11     Distance progress bar 0,10,20...100%. 11 frames.
0xa1  DIST_LOGO          1     Distance, static logo.
0xa2  DIST              10     Distance, left aligned, digits.
0xa3  DIST_CA           10     Distance, centre aligned, digits.
0xa4  DIST_RA           10     Distance, right aligned, digits.
0xa5  DIST_KM            1     Distance unit 'KM'.
0xa6  DIST_MI            1     Distance unit 'MI'.
0xc0  BTLINK_UP          1     Bluetooth link up / connected.
0xc1  BTLINK_DOWN        1     Bluetooth link down / not connected.
0xce  BATT_IMG           1     Battery level image.
0xd0  BATT_IMG_B         1     Battery level image, alternate.
0xd1  BATT_IMG_C         1     Battery level image, alternate.
0xd2  BATT              10     Battery level, left aligned, digits. (Assumed).
0xd3  BATT_CA           10     Battery level, centre aligned, digits.
0xd4  BATT_RA           10     Battery level, right aligned, digits.
0xda  BATT_IMG_D         1     Battery level image, alternate.
0xd7  WEATHER_TEMP      13     Weather temperature, left aligned, 11 digits (0-9 and -) and a special 12 & 13th double-width characters for deg. C and deg. F.
0xd8  WEATHER_TEMP_CA   13     Weather temperature, centre aligned, 11 digits (0-9 and -) and a special 12 & 13th double-width characters for deg. C and deg. F.
0xd9  WEATHER_TEMP_RA   13     Weather temperature, right aligned, 11 digits (0-9 and -) and a special 12 & 13th double-width characters for deg. C and deg. F.
0xf0  SEPERATOR          1     Static image used as date or time seperator e.g. / or :.
0xf1  HAND_HOUR          1     Analog time hour hand, at 1200 position.
0xf2  HAND_MINUTE        1     Analog time minute hand, at 1200 position.
0xf3  HAND_SEC           1     Analog time second hand, at 1200 position.
0xf4  HAND_PIN_UPPER     1     Top half of analog time centre pin.
0xf5  HAND_PIN_LOWER     1     Bottom half of analog time centre pin.
0xf6  TAP_TO_CHANGE      1     Series of images. Tap to change. Count is specified by animationFrames.
0xf7  ANIMATION          1     Animation. Count is specified by animationFrames.
0xf8  ANIMATION_F8       1     Animation. Count is specified by animationFrames.
```

## Supported image formats
The program supports specific varieties of Windows BMP files only.
**Export:** Windows BMP, 16-bit RGB565. (The binary watch face files only support RGB565).  
**Import:** Windows BMP: 
- 16-bit RGB565. As is.
- 24-bit RGB888. Will be converted to RGB565 by the program.
- 32-bit ARGB8888. The program will attempt basic alpha blending against the background image. Note that the watch itself does NOT support an alpha channel.

## Supported watches
All Da Fit watches (using MoYoung v2 firmware) should be supported to some extent.  
Currently, only type A and type C watches are supported for unpacking.  
Creating new watchfaces is currently only supported for type C watches.  
Type B watches are compressed, and the decompression has not yet been implemented.  

Tpls | Screen width x height (pixels) | File type | Example models | Example codes (starts with MOY-) | Comments 
-----|------------|-----|----------|---------|--------------
   1 | 	240 x 240 |  A  |  ?       | ?       | square face  
   6 | 	240 x 240 |  A  |  ?       | ?       | round face   
   7 | 	240 x 240 |  A  |  ?       | ?       | round face   
   8 | 	240 x 240 |  A  |  ?       | ?       |              
  13 | 	240 x 240 |  B  |  ?       | ?       |              
  19 | 	240 x 240 |  B  |  ?       | ?       | round face   
  20 | 	240 x 240 |  B  |  ?       | ?       |              
  25 | 	360 x 360 |  B  |  ?       | ?       | round face   
  27 | 	240 x 240 |  A  |  ?       | ?       |              
  28 | 	240 x 240 |  A  |  ?       | ?       |              
  29 | 	240 x 240 |  A  |  ?       | ?       |              
  30 | 	240 x 240 |  B  |  ?       | ?       |              
  33 | 	240 x 280 |  C  | C20      | QHF3    | Needs preview image (140x163) added as last item.
  34 | 	240 x 280 |  B  |  ?       | ?       |              
  36 | 	240 x 295 |  B  |  ?       | ?       |              
  38 | 	240 x 240 |  C  |  ?       | ?       | round face   
  39 | 	240 x 240 |  C  |  ?       | ?       | square face  
  40 | 	320 x 385 |  C  |  ?       | ?       | round face	
  41 | 	360 x 360 |  C  |  ?       | ?       | round face   
  44 | 	240 x 283 |  C  |  ?       | ?       |              
  45 | 	240 x 295 |  C  |  ?       | ?       |              
  46 | 	240 x 288 |  C  |  ?       | ?       |              
  47 | 	200 x 320 |  C  |  ?       | ?       |              
  48 | 	390 x 390 |  C  |  ?       | ?       | round face   
  49 | 	320 x 380 |  C  |  ?       | ?       |              
  51 | 	356 x 400 |  C  |  ?       | ?       |              
  52 | 	454 x 454 |  C  |  ?       | ?       | round face   
  53 | 	368 x 448 |  C  |  ?       | ?       |              
  55 | 	172 x 320 |  C  |  ?       | ?       |              
  56 | 	240 x 286 |  C  |  ?       | ?       | Some faces show 240x280, but have y-offset of 3.  
  59 | 	320 x 386 |  C  |  ?       | ?       | Some faces show 320x380, but have y-offset of 3.  
  60 | 	240 x 284 |  C  |  ?       | ?       | Some faces show 240x280, but have y-offset of 2.  


## Uploading the watch face
You can use [dawfu - da watch face uploader](https://github.com/david47k/dawfu).

## Other Da Fit / Mo Young watch projects
