// color fade table (each entry is what color to change to)
const unsigned char colorfade[16] = {
	0x00, 0x10, 0xc0, 0x20, 0x10, 0x40, 0x10, 0x10, 
	0x60, 0x80, 0x10, 0xa0, 0x10, 0x10, 0x10, 0xe0
};

// colors for each sound channel
const unsigned char colorchan[4] = {
	0x90, 0x30, 0x50, 0xb0
};

// ball sprite
const unsigned char ballsprite[8] = {
	0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c
};

// tone tingy thingie (defined vertically first, 3 across x 2 down)
// when drawing, r/c is the center of the bottom row
const unsigned char tonehit[6*8] = {
  0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
  0x03, 0x03, 0x03, 0x03, 0x07, 0x07, 0x07, 0x07,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80,
  0xc0, 0xc0, 0xc0, 0xc0, 0xe0, 0xe0, 0xe0, 0xe0
};

// drums - same as above, but r/c is top center
const unsigned char drumhit[6*8] = {
	0x00, 0x01, 0x03, 0x03, 0x01, 0x02, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0x00, 0x80, 0xc0, 0xc0, 0x80, 0x40, 0xc0, 0xc0, 
	0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x80, 0x00
};

// note lookup table - tone counter (0-0x3ff) to tone target
// first value should be 0
const unsigned char tonetarget[1024] = {
0,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,20,19,18,17,17,16,15,15,14,13,13,12,12,11,11,10,10,9,9,8,8,7,7,7,6,6,6,5,5,4,4,4,4,4,2,2,2,2,2,1,1,1,3,3,0,0,0,0,21,21,21,21,21,21,21,21,20,20,20,20,20,20,20,19,19,19,19,19,18,18,18,18,18,17,17,17,17,17,16,16,16,16,16,16,15,15,15,15,15,15,14,14,14,14,14,14,14,13,13,13,13,13,13,13,12,12,12,12,12,12,12,11,11,11,11,11,11,11,11,10,10,10,10,10,10,10,10,9,9,9,9,9,9,9,9,9,8,8,8,8,8,8,8,8,8,7,7,7,7,7,7,7,7,7,7,6,6,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5,5,5,4,4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,18,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/*
Generated in Blassic with:

      5 DIM out(1024)
     10 RESTORE
     20 idx=0
     25 start=0
     30 READ b,c
     40 mid=(c-b)/2+b
     50 out(idx)=tone
     60 idx=idx+1
     70 IF idx<mid THEN 50
     71 PRINT tone;" from  ";start;" to ";idx
     75 tone=tone+1
     76 start=idx
     80 b=c:READ c
     90 IF c <> -1 THEN 40
    100 out(idx)=21
    110 idx=idx+1
    120 IF idx<1024 THEN 100
    130 FOR a=0 to 1023:PRINT out(a);","; : NEXT a
   1000 DATA 53,64,80,85,95,107,120,143,170,190,202,214,226,240,254,269,285,320,381,453,571,762
   1010 DATA -1,-1,-1

Data 1000 is created from the Digiloo note table. We have only 22 buckets and 42 notes,
10 were just note slides, but 11 were manually removed by eye.

Rewrote slightly to use TI musical note table, putting 2 octaves across 
(I drop the two highest flats to get 24 notes in 22 buckets)
Also going lowest note to highest.

5 DIM out(1024)
6 dim in(66)
10 RESTORE
12 for a=1 to 66:read in(a):next a
20 for i=0 to 1023
30 l=0:r=9999
40 for i2=1 to 66
45 if abs(i-in(i2)) < r then r=abs(i-in(i2)):l=i2
50 next i2
60 out(i)=(l-1) mod 22
70 next i
80 out(0)=0:rem lowest note, don't care what the math does
130 FOR a=0 to 1023:PRINT out(a);","; : NEXT a
1000 data 1017,960,906,855,807,762,719,679,641,605,571,539
1010 data 508,480,453,428,404,381,360,339,320,285
1020 rem dropped 302,269
1030 data 254,240,226,214,202,190,180,170,160,151,143,135
1040 data 127,120,113,107,101,95,90,85,80,71
1050 rem dropped 76,67
1060 data 64,60,57,63,50,48,45,42,40,38,36,34
1070 data 32,30,28,27,25,24,22,21,20,19

*/


/////////////////////////////////////////////////////////////////////

// row/col positions for tones (22)
const unsigned char tones[44] = {
	13,1,11,3,9,5,12,7,7,7,10,9,5,9,8,11,11,12,6,13,9,15,4,15,6,17,11,18,8,19,10,21,5,21,12,23,7,23,9,25,11,27,13,29
};

// row/col positions for drums (8)
const unsigned char drums[16] = {
	19,7,17,9,15,11,15,7,19,23,17,21,15,19,15,23
};

// this BASIC program takes the above position data and calculates delay frames for each, based on
// an overall 30 frame delay (divide by 2 for 15)
/*
     10 sx=124
     20 sy=15*8
     29 REM tones (22 pairs)
     30 DATA         13,1,11,3,9,5,12,7,7,7,10,9,5,9,8,11,11,12,6,13,9,15,4,15,6,17,11,18,8,19,10,21,5,21,12,23,7,23,9,25,11,27,13,29
     39 REM drums (8 pairs)
     40 DATA 19,7,17,9,15,11,15,7,19,23,17,21,15,19,15,23
     50 RESTORE
     60 FOR a=1 TO 22+8
     70 READ ty,tx
     73 tx=tx*8
     76 ty=ty*8
     79 IF a<=22 THEN ty=ty-4
     80 d=ABS(tx-sx)+ABS(ty-sy)
     90 d=INT(d/4)
    100 IF d>30 THEN d=30
    110 PRINT d;",";
    120 IF a=22 THEN PRINT
    130 NEXT a
    140 PRINT
*/
const unsigned char delayTones[22] = {
    30,30,30,24,30,24,30,24,16,24,14,24,22,14,22,22,30,22,30,30,30,30
};
const unsigned char delayDrums[8] = {
    25,17,9,17,23,15,7,15
};

// map from 32 to 90 - some shuffling for manual color set
// selection on XZ
const unsigned char charmap[63] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,240,241,242,243,244,245,246,247,248,176,249,177,250,184,251,
    185,192,252,253,193,254,200,255,232,201,233
};

// 16 characters of character set, load at char 240
const unsigned char font[16*8] = {
	0x20,0x50,0x88,0x88,0xF8,0x88,0x88,0x00,	//A
	0xF0,0x48,0x48,0x70,0x48,0x48,0xF0,0x00,	//B
	0x30,0x48,0x80,0x80,0x80,0x48,0x30,0x00,	//C
	0xE0,0x50,0x48,0x48,0x48,0x50,0xE0,0x00,	//D
	0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8,0x00,	//E
	0xF8,0x80,0x80,0xF0,0x80,0x80,0x80,0x00,	//F
	0x70,0x88,0x80,0xB8,0x88,0x88,0x70,0x00,	//G
	0x88,0x88,0x88,0xF8,0x88,0x88,0x88,0x00,	//H
	0x70,0x20,0x20,0x20,0x20,0x20,0x70,0x00,	//I
	0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88,0x00,	//K
	0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88,0x00,	//M
	0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00,	//O
	0xF0,0x88,0x88,0xF0,0xA0,0x90,0x88,0x00,	//R
	0x70,0x88,0x80,0x70,0x08,0x88,0x70,0x00,	//S
	0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00,	//U
	0x88,0x88,0x88,0xA8,0xA8,0xD8,0x88,0x00 	//W
};

// JL, load at 176
const unsigned char fontjl[16*8] = {
	0x38,0x10,0x10,0x10,0x90,0x90,0x60,0x00,
	0x80,0x80,0x80,0x80,0x80,0x80,0xF8,0x00
};

// NP, load at 184
const unsigned char fontnp[16*8] = {
	0x88,0xC8,0xC8,0xA8,0x98,0x98,0x88,0x00,
	0xF0,0x88,0x88,0xF0,0x80,0x80,0x80,0x00
};

// QT, load at 192
const unsigned char fontqt[16*8] = {
	0x70,0x88,0x88,0x88,0xA8,0x90,0x68,0x00,
	0xF8,0x20,0x20,0x20,0x20,0x20,0x20,0x00
};

// VY, load at 200
const unsigned char fontvy[16*8] = {
	0x88,0x88,0x88,0x88,0x50,0x50,0x20,0x00,
	0x88,0x88,0x88,0x70,0x20,0x20,0x20,0x00
};

// XZ, load at 232
const unsigned char fontxz[16*8] = {
	0x88,0x88,0x50,0x20,0x50,0x88,0x88,0x00,
	0xF8,0x08,0x10,0x20,0x40,0x80,0xF8,0x00
};

