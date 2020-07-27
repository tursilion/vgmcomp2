// quickplayercmd.cpp : simple command line version
// The TI player loads in low RAM at >2100, the song data loads in high RAM at >A000 to allow 24k
// The Coleco player loads at >8000 and the song data also loads at >A000 for 24k
// We can load up to two songs, the second follows the first.
// There is a 768 byte screen area for text, and after that:
// - two bytes for song one address, or zero for none
// - two bytes for song two address, or zero for none
// - three bytes for SID control for TI SID only
// - one byte for looping (true/false)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "..\QuickPlayer\quickplayColeco.c"
#include "..\QuickPlayer\quickplayTI.c"

unsigned char *textbufferTI=NULL;       // address of the text buffer in the TI file
unsigned char *textbufferCOL=NULL;      // address of the text buffer in the Coleco file
bool isTIMode = false;                  // whether in TI mode (else Coleco)
bool isColecoMode = false;              // only used to make sure you select one
bool loop = false;                      // whether to loop
const char *snPath = NULL;              // path to the SN file
const char *sidPath = NULL;             // path to the SID file
const char *ayPath = NULL;              // path to the AY file
const char *textPath = NULL;            // path to the text file
int sidctl[3] = { 0x40, 0x40, 0x40 };   // SID control bytes for TI player
char text[24][33];                      // lines of text for centering
char rawtext[768+2*32+1];               // room for line endings, regardless of type
char song[24*1024];                     // 24k for the song data
size_t songsize;                        // size of song data

// Write a TIFILES header, assuming PROGRAM image, no 6 byte header
void DoTIFILES(FILE *fp, int nSize) {
	/* create TIFILES header */
	unsigned char h[128];						// header

	memset(h, 0, 128);							// initialize
	h[0] = 7;
	h[1] = 'T';
	h[2] = 'I';
	h[3] = 'F';
	h[4] = 'I';
	h[5] = 'L';
	h[6] = 'E';
	h[7] = 'S';
	h[8] = 0;									// length in sectors HB
	h[9] = nSize/256;							// LB (24*256=6144)
	if (nSize%256) h[9]++;
	h[10] = 1;									// File type (1=PROGRAM)
	h[11] = 0;									// records/sector
	if (nSize%256) {
		h[12]=nSize%256;						// # of bytes in last sector (0=256)
	} else {
		h[12] = 0;								// # of bytes in last sector (0=256)
	}
	h[13] = 1;									// record length 
	h[14] = h[9];								// # of records(FIX)/sectors(VAR) LB!
	h[15] = 0;									// HB 
	fwrite(h, 1, 128, fp);
}

int main(int argc, char *argv[]) 
{
	printf("VGMComp2 Quickplayer Tool - v20200726\n\n");

    if (argc < 4) {
        printf("quickplayercmd (-ti|-coleco) [-loop] [-sn <sn music>] [-sid <sid music>] [-ay <ay music>] [-sidctl c1 c2 c3] [-text <textfile>] <output>\n");
        printf("Generates a playable music file for the target machine. Max 1 song, 24k of music.\n");
        printf("-ti - select TI output - this or -coleco MUST be specified\n");
        printf("-coleco - select ColecoVision output - this or -ti MUST be specified\n");
        printf("-loop - loop the music - if not specified the music will stop at the end\n");
        printf("-sn - specify file for SN chip - no SN is played if blank\n");
        printf("-sid - specify file for SID chip - no SID is played if blank. Valid on TI only. Requires sidctl.\n");
        printf("-ay - specify file for AY chip - no AY is played if blank. Valid on Coleco only.\n");
        printf("-sidctl - set SID control registers - 0x80 for noise, 0x40 for pulse, 0x20 for sawtooth, 0x10 for triangle. All three voices will be pulse if not specified. Other values may cause unintended effects.\n");
        printf("-text - text file to display - maximum line length is 32 characters, and maximum 24 lines. Blank screen is displayed if absent.\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while (argv[arg][0] == '-') {
        if (0 == strcmp(argv[arg], "-ti")) {
            isTIMode = true;
            if (isColecoMode) {
                printf("You can not select both TI and Coleco mode.\n");
                return 1;
            }
        } else if (0 == strcmp(argv[arg], "-coleco")) {
            isColecoMode = true;
            if (isTIMode) {
                printf("You can not select both TI and Coleco mode.\n");
                return 1;
            }
        } else if (0 == strcmp(argv[arg], "-loop")) {
            loop = true;
        } else if (0 == strcmp(argv[arg], "-sn")) {
            ++arg;
            if (arg+1>=argc) {
                printf("Not enough arguments for -sn\n");
                return 1;
            }
            snPath = argv[arg];
        } else if (0 == strcmp(argv[arg], "-sid")) {
            ++arg;
            if (arg+1>=argc) {
                printf("Not enough arguments for -sid\n");
                return 1;
            }
            sidPath = argv[arg];
        } else if (0 == strcmp(argv[arg], "-ay")) {
            ++arg;
            if (arg+1>=argc) {
                printf("Not enough arguments for -ay\n");
                return 1;
            }
            ayPath = argv[arg];
        } else if (0 == strcmp(argv[arg], "-text")) {
            ++arg;
            if (arg+1>=argc) {
                printf("Not enough arguments for -text\n");
                return 1;
            }
            textPath = argv[arg];
        } else if (0 == strcmp(argv[arg], "-sidctl")) {
            ++arg;
            if (arg+3>=argc) {
                printf("Not enough arguments for -sidctl\n");
                return 1;
            }
            for (int idx=0; idx<3; ++idx) {
                sidctl[idx] = 0;
                if (0 == sscanf(argv[arg], "0x%X", &sidctl[idx])) {
                    sidctl[idx] = atoi(argv[arg]);
                }
                if (0 == sidctl[idx]) {
                    printf("Error parsing sidctl value %d\n", idx);
                }
                ++arg;
            }
        }

        ++arg;
        if (arg >= argc) {
            printf("out of arguments\n");
            return 1;
        }
    }

    if ((!isTIMode)&&(!isColecoMode)) {
        printf("You MUST specify -ti or -coleco for output.\n");
        return 1;
    }
    if (arg>=argc) {
        printf("Not enough arguments for filename.\n");
        return 1;
    }

    // much of the rest of this is a straight port of the Windows version...

    // offset inside of song[]
    int offset = 0;
    songsize = 0;

	FILE *fp;
    if (snPath != NULL) {
	    fp=fopen(snPath, "rb");
	    if (NULL == fp) {
		    printf("Can't load SN file, code %d.\n", errno);
		    return 1;
	    }
	    songsize+=fread(song, 1, (24*1024)-(6*3), fp);
	    if (!feof(fp)) {
		    fclose(fp);
            if (songsize == 0) {
                printf("Error reading song.\n");
            } else {
    		    printf("Song too large (24558 bytes max for this tool).\n");
            }
		    return 1;
	    }
	    fclose(fp);
	    if (songsize&1) songsize++;		// pad to word size (can't overflow cause odd->even)
        offset = songsize;
    }

    // song 2 can only be SID /or/ AY
    if (isTIMode) {
        // TI mode
        if (sidPath != NULL) {
	        fp=fopen(sidPath, "rb");
	        if (NULL == fp) {
		        printf("Can't load SID file, code %d\n", errno);
		        return 1;
	        }
	        songsize+=fread(&song[offset], 1, (24*1024)-(6*3)-songsize, fp);
	        if (!feof(fp)) {
		        fclose(fp);
                if (songsize == offset) {
                    printf("Error reading song.\n");
                } else {
    		        printf("Song combination too large (24558 bytes max for this tool).\n");
                }
		        return 1;
	        }
	        fclose(fp);
	        if (songsize&1) songsize++;		// pad to word size (can't overflow cause odd->even)
        }
    } else {
        // Coleco mode
        if (ayPath != NULL) {
	        fp=fopen(ayPath, "rb");
	        if (NULL == fp) {
		        printf("Can't load AY file, code %d\n", errno);
		        return 1;
	        }
	        songsize+=fread(&song[offset], 1, (24*1024)-(6*3)-songsize, fp);
	        if (!feof(fp)) {
		        fclose(fp);
                if (songsize == offset) {
                    printf("Error reading song.\n");
                } else {
    		        printf("Song combination too large (24558 bytes max for this tool).\n");
                }
		        return 1;
	        }
	        fclose(fp);
	        if (songsize&1) songsize++;		// pad to word size (can't overflow cause odd->even)
        }
    }

    memset(text, 0, sizeof(text));
    if (NULL != textPath) {
        memset(rawtext, '\0', sizeof(rawtext));
        fp = fopen(textPath, "r");
        fread(rawtext, 1, sizeof(rawtext)-1, fp);
        fclose(fp);
	
        // this code will limit to 24 lines and 32 chars per line without warning
        char *pwork = rawtext;
	    for (int idx=0; idx<24; idx++) {
            // find line ending
            char *p = strchr(pwork, '\n');
            // no more, then done
            if (NULL == p) break;
            // how far in?
            int cnt = p - rawtext;
            // clamp to 32
            if (cnt > 32) cnt = 32;
            // copy out to text array
            memcpy(text[idx], rawtext, cnt);
            // trim trailing spaces (not leading)
            while (text[idx][strlen(text[idx])-1]==' ') text[idx][strlen(text[idx])-1]='\0';
            // next loop
            pwork = p+1;
	    }
    }
	
	// find the location to dump the text
	unsigned char *p;
    if (isTIMode) {
        p = textbufferTI;
    } else {
        p = textbufferCOL;
    }
	if (NULL == p) {
        if (isTIMode) {
    		p=quickplayti;
        } else {
            p=quickplaycoleco;
        }
        int idx;
		for (idx=0; idx<1024; idx++) {
			if (0 == memcmp(p, "~~~~DATAHERE~~~~", 12)) break;
			p++;
		}
		if (idx>=1024) {
			printf("Internal error - can't find text buffer. Failing.\n");
			return 1;
		}
        if (isTIMode) {
    		textbufferTI=p;
        } else {
    		textbufferCOL=p;
        }
	}

    for (int idx=0; idx<24; idx++) {
        // center each line into the buffer
		int l=strlen(text[idx]);
        for (int x=0; x<16-(l/2); ++x) {
            memmove(&text[idx][1], &text[idx][0], strlen(text[idx])+1);
            text[idx][0] = ' ';
            strcat(text[idx], " ");
        }
        // can happen in the event of odd values
        while (strlen(text[idx]) > 32) {
            text[idx][strlen(text[idx])-1]='\0';
        }
		memcpy(p+(idx*32), text[idx], 32);
	}

    // now fill in the options
    if (snPath != NULL) {
        // first file is always at 0xA000
        if (isTIMode) {
            // big endian 9900
            p[768] = 0xa0;
            p[769] = 0x00;
        } else {
            // little endian z80
            p[768] = 0x00;
            p[769] = 0xa0;
        }
    } else {
        p[768] = 0;
        p[769] = 0;
    }

    offset+=0xa000;

    if (isTIMode) {
        if (snPath != NULL) {
            // big endian 9900
            p[770] = offset/256;
            p[771] = offset%256;
            p[772] = sidctl[0];
            p[773] = sidctl[1];
            p[774] = sidctl[2];
        } else {
            p[770] = 0;
            p[771] = 0;
            // don't need to fill in the SID controls
        }
    } else {
        if (ayPath != NULL) {
            // little endian z80
            p[770] = offset%256;
            p[771] = offset/256;
            // don't need to fill in the SID controls
        } else {
            p[770] = 0;
            p[771] = 0;
            // don't need to fill in the SID controls
        }
    }

    if (loop) {
        p[775] = 1;
    } else {
        p[775] = 0;
    }

	// now dump it
    printf("Writing %s...\n", argv[arg]);
	fp=fopen(argv[arg], "wb");
	if (NULL == fp) {
		printf("Can't write output, code %d\n", errno);
		return 1;
	}

    if (isTIMode) {
		fwrite(quickplayti, 1, SIZE_OF_QUICKPLAYTI, fp);
		fclose(fp);

		// increment last char of name and then write the song into that
        // the song can be up to 3 files long
        char tmpName[2048];
        strcpy(tmpName, argv[arg]);

        for (unsigned int offset = 0; offset < songsize; offset+=8192-6) {
            char *p = strchr(tmpName, '.');
            if (NULL == p) {
                p = strchr(tmpName, '\0')-1;
            } else {
                --p;
                if (p < tmpName) {
				    printf("Bad output filename.\n");
				    return 1;
			    }
		    }
            *p = (*p)+1;
            printf("Writing %s...\n", tmpName);
            fp=fopen(tmpName, "wb");
		    if (NULL == fp) {
			    printf("Can't write output '%s', code %d.", tmpName, errno);
			    return 1;
		    }
            int outputsize = (songsize-offset > 8192-6 ? 8192 : (songsize-offset)+6);
		    DoTIFILES(fp, outputsize);
		    // now write the 6 byte program files header
            if (songsize <= outputsize+offset) {
		        fputc(0x00,fp);
		        fputc(0x00,fp);		// >0000 - last file 
            } else {
                fputc(0xff,fp);
                fputc(0xff,fp);     // >FFFF - more files
            }
		    fputc(outputsize/256, fp);
		    fputc(outputsize%256, fp);	// size of file including header
		    // A000 and up - load address
            int outputadr = 0xa000 + offset;
		    fputc((outputadr/256)&0xff,fp);
		    fputc((outputadr%256),fp);
		    fwrite(&song[offset], 1, outputsize-6, fp);
		    fclose(fp);
        }
    } else {
        // Coleco mode
        // We will just patch and write out the Coleco ROM file
        memcpy(&quickplaycoleco[0xa000-0x8000], song, songsize);
        fwrite(quickplaycoleco, 1, 32768, fp);
        fclose(fp);
    }
}
