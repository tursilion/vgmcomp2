// code that is shared between multiple instances of the player
#include "CPlayer.h"

// unpack tags
#define TYPEINLINE 0x00
#define TYPEINLINE2 0x20
#define TYPERLE 0x40
#define TYPERLE32 0x60
#define TYPERLE16 0x80
#define TYPERLE24 0xa0
#define TYPEBACKREF 0xc0
#define TYPEBACKREF2 0xe0

// extract a byte from the buffer, called for every byte
// Normally inline, but if you need to split up your
// output date (say, for banking or split memory), you
// can do the work here.
// (buf is passed in just in case it's useful to have the base)
static inline uint8 getBufferByte(const uint8 *adr) {
    return *adr;
}

static uint8 getDatInline(STREAM *str) {
    // just pull a string of bytes
    return getBufferByte(str->curPtr++);
}
static uint8 getDatRLE(STREAM *str) {
    // pull the single byte - no increment
    return getBufferByte(str->curPtr);
}
static uint8 getDatRLE32(STREAM *str) {
    // pull the last four bytes over and over
    // mainPtr is assumed already incremented
    if (str->curPtr == str->mainPtr) {
        str->curPtr -= 4;
    }
    return getBufferByte(str->curPtr++);
}
static uint8 getDatRLE16(STREAM *str) {
    // pull the last two bytes over and over
    // mainPtr is assumed already incremented
    if (str->curPtr == str->mainPtr) {
        str->curPtr -= 2;
    }
    return getBufferByte(str->curPtr++);
}
static uint8 getDatRLE24(STREAM *str) {
    // pull the last three bytes over and over
    // mainPtr is assumed already incremented
    if (str->curPtr == str->mainPtr) {
        str->curPtr -= 3;
    }
    return getBufferByte(str->curPtr++);
}

// unpack a stream byte - offset and maxbytes are used to write a scaled
// address for the heatmap to display later
// cnt is row count, and maxbytes is used for scaling, max size of data
uint8 getCompressedByte(STREAM *str) {
    unsigned char x,x1,x2;

    // bytes left in the current stream?
    if (str->curBytes > 0) {
        --str->curBytes;
        return str->curType(str);
    }

    // start a new stream
    x = getBufferByte(str->mainPtr++);
    switch (x&0xe0) {
    case TYPEBACKREF:  // long back reference
    case TYPEBACKREF2:
        {
            signed short temp;   // to ensure we get proper sign extension - 16 bit
            str->curType = getDatInline;
            x1 = getBufferByte(str->mainPtr);
            x2 = getBufferByte(str->mainPtr+1);
            temp = (short)(x1*256 + x2);
            // check for end of stream
            if (temp == 0) {
                str->mainPtr = 0;
                return 0;
            }
            str->curPtr = str->mainPtr + temp;  // add before we increment mainptr
            str->mainPtr += 2;
            str->curBytes = (x&0x3f) + 4;
        }
        break;
        
    case TYPERLE32:  // RLE-32
        str->curType = getDatRLE32;
        str->curPtr = str->mainPtr;
        str->mainPtr += 4;
        str->curBytes = ((x&0x1f) + 2)*4;
        break;

    case TYPERLE24:  // RLE-24
        str->curType = getDatRLE24;
        str->curPtr = str->mainPtr;
        str->mainPtr += 3;
        str->curBytes = ((x&0x1f) + 2)*3;
        break;

    case TYPERLE16:  // RLE-16
        str->curType = getDatRLE16;
        str->curPtr = str->mainPtr;
        str->mainPtr += 2;
        str->curBytes = ((x&0x1f) + 2)*2;
        break;

    case TYPERLE:  // RLE
        str->curType = getDatRLE;
        str->curPtr = str->mainPtr;
        str->mainPtr++;
        str->curBytes = (x&0x1f) + 3;
        break;

    case TYPEINLINE:  // inline
    case TYPEINLINE2:
        str->curType = getDatInline;
        str->curPtr = str->mainPtr;
        str->curBytes = (x&0x3f) + 1;
        str->mainPtr += str->curBytes;
        break;
    }

    // recurse call to get the next byte of data
    return getCompressedByte(str);
}

