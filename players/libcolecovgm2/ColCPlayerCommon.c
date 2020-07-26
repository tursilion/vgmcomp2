// code that is shared between multiple instances of the player
#include "ColCPlayer.h"
#include <string.h>

// unpack tags
#define TYPEINLINE 0x00
#define TYPEINLINE2 0x20
#define TYPERLE 0x40
#define TYPERLE32 0x60
#define TYPERLE16 0x80
#define TYPERLE24 0xa0
#define TYPEBACKREF 0xc0
#define TYPEBACKREF2 0xe0

// Global stream object used to make access a direct memory reference
// STREAM objects are only 8 bytes in size
STREAM globalStr;

// wrapper for external test code
uint8 getCompressedByte(STREAM *str, const uint8 *buf) {
    (void)buf;

    // manual unrolled LDIs doesn't work out faster in the end
    memcpy(&globalStr, str, sizeof(STREAM));
    uint8 x = getCompressedByteRaw();
    memcpy(str, &globalStr, sizeof(STREAM));
    
    return x;
}

static uint8 getDatInline() {
    // just pull a string of bytes
    return *(globalStr.curPtr++);
}
static uint8 getDatRLE() {
    // pull the single byte - no increment
    return *(globalStr.curPtr);
}
static uint8 getDatRLE32() {
    // pull the last four bytes over and over
    // mainPtr is assumed already incremented
    if (globalStr.curPtr == globalStr.mainPtr) {
        globalStr.curPtr -= 4;
    }
    return *(globalStr.curPtr++);
}
static uint8 getDatRLE16() {
    // pull the last two bytes over and over
    // mainPtr is assumed already incremented
    if (globalStr.curPtr == globalStr.mainPtr) {
        globalStr.curPtr -= 2;
    }
    return *(globalStr.curPtr++);
}
static uint8 getDatRLE24() {
    // pull the last three bytes over and over
    // mainPtr is assumed already incremented
    if (globalStr.curPtr == globalStr.mainPtr) {
        globalStr.curPtr -= 3;
    }
    return *(globalStr.curPtr++);
}

// each curBytes is -1 for the one byte we are now returning
uint8 startInline(unsigned char x) {
    globalStr.curType = getDatInline;
    globalStr.curPtr = globalStr.mainPtr;
    globalStr.curBytes = (x&0x3f) + 1 - 1;
    globalStr.mainPtr += globalStr.curBytes + 1;
    // getDatInline
    return *(globalStr.curPtr++);
}

uint8 startRLE(unsigned char x) {
    globalStr.curType = getDatRLE;
    globalStr.curPtr = globalStr.mainPtr;
    globalStr.mainPtr++;
    globalStr.curBytes = (x&0x1f) + 3 - 1;
    // getDatRLE
    return *(globalStr.curPtr);
}

uint8 startRLE32(unsigned char x) {
    globalStr.curType = getDatRLE32;
    globalStr.curPtr = globalStr.mainPtr;
    globalStr.mainPtr += 4;
    globalStr.curBytes = ((x&0x1f) + 2)*4 - 1;
    // getDatRLE32 without the test
    return *(globalStr.curPtr++);
}

uint8 startRLE16(unsigned char x) {
    globalStr.curType = getDatRLE16;
    globalStr.curPtr = globalStr.mainPtr;
    globalStr.mainPtr += 2;
    globalStr.curBytes = ((x&0x1f) + 2)*2 - 1;
    // getDatRLE16 without the test
    return *(globalStr.curPtr++);
}

uint8 startRLE24(unsigned char x) {
    globalStr.curType = getDatRLE24;
    globalStr.curPtr = globalStr.mainPtr;
    globalStr.mainPtr += 3;
    globalStr.curBytes = ((x&0x1f) + 2)*3 - 1;
    // getDatRLE24 without the test
    return *(globalStr.curPtr++);
}

uint8 startBackref(unsigned char x) {
    signed short temp;   // to ensure we get proper sign extension - 16 bit
    unsigned char x1,x2;

    globalStr.curType = getDatInline;
    x1 = *(globalStr.mainPtr);
    x2 = *(globalStr.mainPtr+1);
    temp = ((short)x1)*256+x2;
    // check for end of stream
    if (temp == 0) {
        globalStr.mainPtr = 0;
        return 0;
    }
    globalStr.curPtr = globalStr.mainPtr + temp;  // add before we increment mainptr
    globalStr.mainPtr += 2;
    globalStr.curBytes = (x&0x3f) + 4 - 1;
    // getDatInline
    return *(globalStr.curPtr++);
}

uint8 (*const startArray[8])(unsigned char) = {
    startInline,startInline,startRLE,startRLE32,startRLE16,startRLE24,startBackref,startBackref
};

// unpack a stream byte - offset and maxbytes are used to write a scaled
// cnt is row count, and maxbytes is used for scaling, max size of data
// Source is always globalStr
uint8 getCompressedByteRaw() {
    // bytes left in the current stream?
    if (globalStr.curBytes > 0) {
        --globalStr.curBytes;
        return globalStr.curType();
    }

    // start a new stream
    unsigned char x = *(globalStr.mainPtr++);
    return startArray[(x&0xe0)>>5](x);
}

