// Tool to unpack a stream and report what each element is
// Used to help debug what is going on after the fact.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

unsigned char buf[65536];
int bufSize = 0;

bool processOld(int str) {
    // the old format is very similar to the new one
    int pos;
    int tmp;
    int end;
    int base;

    tmp = str%12;
    switch (tmp) {
    case 0:
    case 1:
    case 2: printf("Tone stream...\n"); break;
    case 3: printf("Noise stream...\n"); break;
    case 4:
    case 5:
    case 6:
    case 7: printf("Volume stream...\n"); break;
    case 8:
    case 9:
    case 10:
    case 11: printf("Timestream...\n"); break;
    }

    // get the tonetable to mark the end of the stream table
    int tonetable = buf[2]*256+buf[3];
    if (tonetable >= bufSize) {
        printf("Tonetable appears to be invalid\n");
        return false;
    }

    // find the stream
    tmp = buf[0]*256+buf[1];    // stream table
    tmp += str*2;

    if (tmp+1 >= bufSize) {
        printf("Stream is not found in file\n");
        return false;
    }

    pos = buf[tmp]*256+buf[tmp+1];
    if (pos >= tmp) {
        printf("Stream pointer doesn't appear valid\n");
        return false;
    }
    base = pos;
    tmp+=2;
    if (tmp+1 > tonetable) {
        // this is probably the last stream, so use start of
        // the stream table
        end = buf[0]*256+buf[1];
    } else {
        end = buf[tmp]*256+buf[tmp+1];
    }

    // okay, work the stream and just report each entry
    while (pos < end) {
        printf("0x%04X: ", pos);

        int ctrl = buf[pos++];
        int size = 0;
        switch (ctrl&0xc0) {
        case 0x00:  // inline
            size = ctrl&0x3f;
            printf("INLINE - %3d bytes: ", size);
            // print first 10
            for (int idx=0; (idx<10)&&(idx<size); ++idx) {
                printf("%02X ", buf[pos+idx]);
            }
            if (size > 10) {
                printf("...");
            }
            printf("\n");
            pos+=size;
            break;

        case 0x40:  // RLE
            size = ctrl&0x3f;
            printf("RLE    - %3d bytes: %02X\n", size, buf[pos]);
            pos++;
            break;

        case 0x80:  // short back reference
            size = ctrl&0x3f;
            printf("SHORT  - %3d bytes: ", size);
            // print first 10
            tmp = base + buf[pos];
            for (int idx=0; (idx<10)&&(idx<size); ++idx) {
                printf("%02X ", buf[tmp+idx]);
            }
            if (size > 10) {
                printf("...");
            }
            printf("\n");
            pos++;
            break;

        case 0xc0:  // long back ref
            size = ctrl&0x3f;
            printf("LONG   - %3d bytes: ", size);
            // print first 10
            tmp = buf[pos] * 256 + buf[pos+1];
            for (int idx=0; (idx<10)&&(idx<size); ++idx) {
                printf("%02X ", buf[tmp+idx]);
            }
            if (size > 10) {
                printf("...");
            }
            printf("\n");
            pos+=2;
            break;
        }
    }

    return true;
}

bool processNew(int str) {
    int pos;
    int tmp;
    int end;

    tmp = str%9;
    switch (tmp) {
    case 0:
    case 1:
    case 2: printf("Tone stream...\n"); break;
    case 3: printf("Noise stream...\n"); break;
    case 4:
    case 5:
    case 6:
    case 7: printf("Volume stream...\n"); break;
    case 8: printf("Timestream...\n"); break;
    }

    // get the tonetable to mark the end of the stream table
    int tonetable = buf[2]*256+buf[3];
    if (tonetable >= bufSize) {
        printf("Tonetable appears to be invalid\n");
        return false;
    }

    // find the stream
    tmp = buf[0]*256+buf[1];    // stream table
    tmp += str*2;

    if (tmp+1 > tonetable) {
        printf("Stream is not found in file\n");
        return false;
    }

    pos = buf[tmp]*256+buf[tmp+1];
    if (pos >= tmp) {
        printf("Stream pointer doesn't appear valid\n");
        return false;
    }
    if (pos == 0) {
        printf("Undefined stream (no data present)\n");
        return true;
    }

    tmp+=2;
    if (tmp+1 >= tonetable) {
        // this is probably the last stream, so use start of
        // the stream table
        end = buf[0]*256+buf[1];
    } else {
        end = 0;
        // allow for null streams, end on the next one ;)
        while (end == 0) {
            end = buf[tmp]*256+buf[tmp+1];
            tmp +=2;
            if ((tmp >= tonetable) && (end == 0)) {
                end = tonetable;
            }
        }
    }

    // okay, work the stream and just report each entry
    while (pos < end) {
        printf("0x%04X: ", pos);

        int ctrl = buf[pos++];
        int size = 0;
        switch (ctrl&0xe0) {
        case 0x00:  // inline
        case 0x20:  // also inline (expanded range)
            size = ctrl&0x3f;
            size += 1;
            printf("INLINE - %3d bytes: ", size);
            // print first 10
            for (int idx=0; (idx<10)&&(idx<size); ++idx) {
                printf("%02X ", buf[pos+idx]);
            }
            if (size > 10) {
                printf("...");
            }
            printf("\n");
            pos+=size;
            break;

        case 0x40:  // RLE
            size = ctrl&0x1f;
            size += 3;
            printf("RLE    - %3d bytes: %02X\n", size, buf[pos]);
            pos++;
            break;

        case 0x60:  // 32-bit RLE
            size = ctrl&0x1f;
            size += 2;
            size *= 4;
            printf("RLE32  - %3d bytes: %02X %02X %02X %02X\n", size, buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]);
            pos+=4;
            break;

        case 0x80:  // 16-bit RLE
            size = ctrl&0x1f;
            size += 2;
            size *= 2;
            printf("RLE16  - %3d bytes: %02X %02X\n", size, buf[pos], buf[pos+1]);
            pos+=2;
            break;

        case 0xa0:  // 24-bit RLE
            size = ctrl&0x1f;
            size += 2;
            size *= 3;
            printf("RLE24  - %3d bytes: %02X %02X %02X\n", size, buf[pos], buf[pos+1], buf[pos+2]);
            pos+=3;
            break;
        
        case 0xc0:  // back ref
        case 0xe0:  // also back ref (expanded range)
            size = ctrl&0x3f;
            size += 4;
            printf("BACKREF- %3d bytes: ", size);
            // print first 10
            tmp = buf[pos]*256+buf[pos+1];
            if (tmp == 0) {
                printf("--END--\n");
                return true;
            } else {
                for (int idx=0; (idx<10)&&(idx<size); ++idx) {
                    printf("%02X ", buf[tmp+idx]);
                }
                if (size > 10) {
                    printf("...");
                }
                printf("\n");
            }
            pos+=2;
            break;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
	printf("VGMComp2 Stream Analysis Tool - v20200620\n\n");

    if (argc < 3) {
        printf("AnalyzeStream <name.sbf> <stream index (0-based)> [-old]\n");
        printf("Pass optional switch \"-old\" to analyze an old v1 stream\n");
        printf("Note that if you have multiple songs, you need to manually\n");
        printf("specify a higher stream index (9 streams per song for new)\n");
        return 1;
    }

    FILE *fp=fopen(argv[1], "rb");
    if (NULL == fp) {
        printf("Can't open file '%s', code %d\n", argv[1], errno);
        return 1;
    }
    bufSize = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    if (bufSize < 4) {
        printf("File too small to be an SBF file.\n");
        return 1;
    }

    int str = atoi(argv[2]);
    printf("Processing stream %d\n", str);

    if ((argc > 3) && (0 == strcmp(argv[3], "-old"))) {
        if (!processOld(str)) {
            return 1;
        }
    } else {
        if (!processNew(str)) {
            return 1;
        }
    }

    return 0;
}

