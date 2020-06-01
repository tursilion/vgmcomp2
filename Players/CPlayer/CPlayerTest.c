// This file is not built with the player, but you can patch it
// into any ported code to run some very basic tests on GetCompressedByte
// Sample usage in PCSN

#include "CPlayer.h"
#ifndef BUILD_TI99
#include <stdio.h>
#else
#include <vdp.h>
#endif

extern uint8 getCompressedByte(STREAM *str, uint8 *buf);
extern uint8* workBuf;
STREAM test;

// there are 8 stream types to test

// inline1 - 8 bytes
static const unsigned char inline1[] = 
    { 0x07, '1','2','3','4','5','6','7','8', 0xc0,0x00,0x00 };
// inline2 - 33 bytes
static const unsigned char inline2[] =
    { 0x20, '1','2','3','4','5','6','7','8','9','0','9','8','7','6','5','4','3','2','1','0','1','2','3','4','5','6','7','8','9','0','9','8','7', 0xc0,0x00,0x00 };
// RLE - 8 bytes
static const unsigned char rle[] =
    { 0x45, '1', 0xc0,0x00,0x00 };
// 32-bit RLE - 2 sets of 4 each
static const unsigned char rle32[] = 
    { 0x60, '1','2','3','4', 0xc0,0x00,0x00 };
// 16-bit RLE - 4 sets of 2 each
static const unsigned char rle16[] = 
    { 0x82, '1','2', 0xc0,0x00,0x00 };
// 24-bit RLE - 3 sets of 3 each
static const unsigned char rle24[] = 
    { 0xa1, '1','2','3', 0xc0,0x00,0x00 };
// back reference (actually forward here, but the protocol doesn't care which way it goes) - 8 bytes
static const unsigned char backref[] = 
    { 0xc4, 0x00, 0x06, 0xc0,0x00,0x00, '1','2','3','4','5','6','7','8' };
// backref 2 - same but 36 bytes long
static const unsigned char backref2[] = 
    { 0xe0, 0x00, 0x06, 0xc0,0x00,0x00, '1','2','3','4','5','6','7','8','9','0',
                                        '9','8','7','6','5','4','3','2','1','0',
                                        '1','2','3','4','5','6','7','8','9','0',
                                        '9','8','7','6','5','4' };

void runTest(const char *name, const unsigned char *buf, const char *tst) {
    unsigned char x;
    int flag;

    workBuf = (uint8*)buf;
    test.mainPtr = (uint8*)buf;
    test.curPtr = 0;
    test.curBytes = 0;
    test.curType = 0;
    test.framesLeft = 0;

    flag = 1;
    while (test.mainPtr) {
        x = getCompressedByte(&test, (uint8*)buf);
        if (flag) {
            printf("%s\n%2d: ", name, test.curBytes+1);
            flag = 0;
        }
        if (x == 0) break;  // the test cases contain no zero bytes, so should be end of stream
        if (*tst == '\0') break;
        if (x != (*(tst++))) {
            printf("(%02X) ", x);
        } else {
            printf("%c ", x);
        }
    }
    printf("\n");
    if (*tst != 0) {
        printf("Remaining string: %s\n", tst);
    }
    if (x != 0) {
        printf("%d data bytes remained\n", test.curBytes);
    }
}

void PlayerUnitTest() {
    runTest("Inline(8)", inline1, "12345678");
    runTest("Inline2(33)", inline2, "123456789098765432101234567890987");
    runTest("RLE(8)", rle, "11111111");
    runTest("RLE32(8)", rle32, "12341234");
    runTest("RLE16(8)", rle16, "12121212");
    runTest("RLE24(9)", rle24, "123123123");
    runTest("BackRef(8)", backref, "12345678");
    runTest("BackRef2(36)", backref2, "123456789098765432101234567890987654");
}