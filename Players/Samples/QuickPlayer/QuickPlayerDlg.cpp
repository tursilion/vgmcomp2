// QuickPlayerDlg.cpp : implementation file
// The TI player loads in low RAM at >2100, the song data loads in high RAM at >A000 to allow 24k
// The Coleco player loads at >8000 and the song data also loads at >A000 for 24k
// We can load up to two songs, the second follows the first.
// There is a 768 byte screen area for text, and after that:
// - two bytes for song one address, or zero for none
// - two bytes for song two address, or zero for none
// - three bytes for SID control for TI SID only
// - one byte for looping (true/false)

#include "stdafx.h"
#include "QuickPlayer.h"
#include "QuickPlayerDlg.h"
#include "quickplayColeco.c"
#include "quickplayTI.c"
#include "qpballsColeco.c"
#include "qpballsTI.c"
#include "qpChuckColeco.c"
#include "qpchuckTI.c"
#include "qpBlinkenColeco.c"
#include "qpBlinkenTI.c"
#include "qpPianoColeco.c"
#include "qpPianoTI.c"

// used for the build code
char song[24*1024];
// remember the text block info so we can fill it in at the end
unsigned int text_offset;
unsigned int text_rows;
unsigned int song_loop_offset;
unsigned int row_byte_offset;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

int CQuickPlayerDlg::getSIDMode(int combo) {
    CWnd *pWnd = GetDlgItem(combo);
    if (NULL != pWnd) {
        CString str;
        pWnd->GetWindowTextA(str);
        if (str=="Noise") return 0x80;
        if (str=="Pulse") return 0x40;
        if (str=="Saw") return 0x20;
        if (str=="Tri") return 0x10;
    }
    return 0;
}

// Assuming PROGRAM image, no 6 byte header
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

// CQuickPlayerDlg dialog
CQuickPlayerDlg::CQuickPlayerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CQuickPlayerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CQuickPlayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CQuickPlayerDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, &CQuickPlayerDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CQuickPlayerDlg::OnBnClickedCancel)
	ON_BN_CLICKED(IDC_BTNSN1, &CQuickPlayerDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CQuickPlayerDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON3, &CQuickPlayerDlg::OnBnClickedButton3)
    ON_BN_CLICKED(IDC_RADIO1, &CQuickPlayerDlg::OnBnClickedRadio1)
    ON_BN_CLICKED(IDC_RADIO2, &CQuickPlayerDlg::OnBnClickedRadio2)
    ON_BN_CLICKED(IDC_BTNSID1, &CQuickPlayerDlg::OnBnClickedBtnsid1)
    ON_BN_CLICKED(IDC_BTNAY1, &CQuickPlayerDlg::OnBnClickedBtnay1)
END_MESSAGE_MAP()


// CQuickPlayerDlg message handlers

BOOL CQuickPlayerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	SendDlgItemMessage(IDC_RADIO1, BM_CLICK);
    SendDlgItemMessage(IDC_COMBOSID1, CB_SETCURSEL, 1, (LPARAM)"Pulse");
    SendDlgItemMessage(IDC_COMBOSID2, CB_SETCURSEL, 1, (LPARAM)"Pulse");
    SendDlgItemMessage(IDC_COMBOSID3, CB_SETCURSEL, 1, (LPARAM)"Pulse");

	SendDlgItemMessage(IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"QuickPlayer");
	SendDlgItemMessage(IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Balls");
	SendDlgItemMessage(IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Blinkenlights");
	SendDlgItemMessage(IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Chuck");
	SendDlgItemMessage(IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)"Piano");
	SendDlgItemMessage(IDC_COMBO1, CB_SETCURSEL, 0, 0);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CQuickPlayerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CQuickPlayerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// disable enter and esc
void CQuickPlayerDlg::OnBnClickedOk()
{
}
void CQuickPlayerDlg::OnBnClickedCancel()
{
}

void CQuickPlayerDlg::OnBnClickedButton1()
{
	// browse
	CFileDialog dlg(TRUE, "VGMComp2 Files (SN)", NULL, 4|2, "SBF Files|*.SBF*||", this);

	if (IDOK == dlg.DoModal()) {
		CWnd *pTxt=GetDlgItem(IDC_SNSONG);
		if (NULL != pTxt) pTxt->SetWindowText(dlg.GetPathName());
	}

}

void CQuickPlayerDlg::OnBnClickedBtnsid1()
{
	// browse
	CFileDialog dlg(TRUE, "VGMComp2 Files (SID)", NULL, 4|2, "SBF Files|*.SBF*||", this);

	if (IDOK == dlg.DoModal()) {
		CWnd *pTxt=GetDlgItem(IDC_SIDSONG);
		if (NULL != pTxt) pTxt->SetWindowText(dlg.GetPathName());
	}
}

void CQuickPlayerDlg::OnBnClickedBtnay1()
{
	// browse
	CFileDialog dlg(TRUE, "VGMComp2 Files (AY)", NULL, 4|2, "SBF Files|*.SBF*||", this);

	if (IDOK == dlg.DoModal()) {
		CWnd *pTxt=GetDlgItem(IDC_AYSONG);
		if (NULL != pTxt) pTxt->SetWindowText(dlg.GetPathName());
	}
}

void CQuickPlayerDlg::OnBnClickedButton2()
{
	// build
	CString text[24];
	CString rawtext;
	size_t songsize;
	CString snfile;
    CString sidfile;
    CString ayfile;
	int playerNum;
	int idx;

	CWnd *pTxt=GetDlgItem(IDC_SNSONG);
	if (NULL == pTxt) {
		AfxMessageBox("Internal error. Can not proceed.");
		return;
	}
	pTxt->GetWindowText(snfile);
	pTxt=GetDlgItem(IDC_AYSONG);
	if (NULL == pTxt) {
		AfxMessageBox("Internal error. Can not proceed.");
		return;
	}
	pTxt->GetWindowText(ayfile);
	pTxt=GetDlgItem(IDC_SIDSONG);
	if (NULL == pTxt) {
		AfxMessageBox("Internal error. Can not proceed.");
		return;
	}
	pTxt->GetWindowText(sidfile);

    CWnd *pLoop = GetDlgItem(IDC_LOOP);
	if (NULL == pTxt) {
		AfxMessageBox("Internal error. Can not proceed.");
		return;
	}
    bool loop = ((CButton*)pLoop)->GetCheck() == BST_CHECKED;

    CWnd *pTISong = GetDlgItem(IDC_RADIO1);
    bool isTIMode = (((CButton*)pTISong)->GetCheck() == BST_CHECKED);

    // offset inside of song[]
    int offset = 0;
    songsize = 0;

	FILE *fp;
    if (snfile.GetLength() > 0) {
	    fp=fopen(snfile, "rb");
	    if (NULL == fp) {
		    AfxMessageBox("Can't load SN file.");
		    return;
	    }
	    songsize+=fread(song, 1, (24*1024)-(6*3), fp);
	    if (!feof(fp)) {
		    fclose(fp);
            if (songsize == 0) {
                AfxMessageBox("Error reading song.");
            } else {
    		    AfxMessageBox("Song too large (24558 bytes max for this tool).");
            }
		    return;
	    }
	    fclose(fp);
	    if (songsize&1) songsize++;		// pad to word size (can't overflow cause odd->even)
        offset = songsize;
    }

	// set up the offsets before we start patching
	if (isTIMode) {
		// The TI build stuffs the address of SongLoop at offset 0xb8.
		song_loop_offset = 0x32+128+6; 	// TIFILES and EA5 headers
		// offset of a single byte to patch for number of rows of text
		// this is part of the LIMI instruction data on the TI Build!!
		row_byte_offset = 0x02+128+6;	// TIFILES and EA5 headers
	} else {
		// The Coleco build stuffs the address of SongLoop at offset 0x28.
		song_loop_offset = 0x28;
		// offset of a single byte to patch for number of rows of text
		// first byte of the unused version data on Coleco
		row_byte_offset = 0x24;
	}

	playerNum = SendDlgItemMessage(IDC_COMBO1, CB_GETCURSEL, 0, 0);
	if (playerNum < 0) {
		AfxMessageBox("Please select a player");
		return;
	}
	// check limitations and load pointer to program
	// rather than rely on removing the const, we'll just copy
	// into a buffer we can modify
	unsigned char *program = (unsigned char*)malloc(32768);	// big enough to make the Coleco cart
	int progsize = 0;
	switch (playerNum) {
		case 0:	// quickplayer
			// no limits
			if (isTIMode) {
				progsize = SIZE_OF_QUICKPLAYTI;
				memcpy(program, quickplayti, progsize);
			} else {
				progsize = SIZE_OF_QUICKPLAYCOLECO;
				memcpy(program, quickplaycoleco, progsize);
			}
			break;

		case 1:	// balls
			// 1 file only
			if ((sidfile.GetLength() > 0)||(ayfile.GetLength() > 0)) {
				AfxMessageBox("This player only supports SN playback");
				return;
			}
			if (isTIMode) {
				progsize = SIZE_OF_QPBALLS;
				memcpy(program, qpballs, progsize);
				// The TI build stuffs the address of SongLoop at offset 0xb8.
				// From there, search the first 50 words for 0x8400, and
				// change it to 0x0000 to mute the player library (only one instance)
				int pSrch = program[song_loop_offset]*256+program[song_loop_offset+1];	// big endian
				pSrch -= 0x2000;						// from address to offset
				while ((pSrch < 0x8191)&&((program[pSrch] != 0x84)||(program[pSrch+1]!=0x00))) pSrch+=2;	// must be even!
				if (pSrch < 8191) {
					OutputDebugString("Patched TI code...\n");
					program[pSrch]=0;	// from LI R8,>8400
					program[pSrch+1]=0;	// to LI R8,>0000 (so writes go to ROM instead)
				}
				if (pSrch >= 8191) {
					AfxMessageBox("Warning: Failed to patch TI code!");
				}

			} else {
				progsize = SIZE_OF_QPBALLSCOLECO;
				memcpy(program, qpBallsColeco, progsize);
				// The Coleco build stuffs the address of SongLoop at offset 0x28.
				// From there, search and change the first 8 instances of 0xD3,0xFF
				// to 00,00 to mute the player library.
				int pSrch = program[song_loop_offset]+program[song_loop_offset+1]*256;	// little endian
				pSrch -= 0x8000;						// from address to offset
				int cnt = 0;
				for (int idx=0; idx<8; idx++) {
					while ((pSrch < 0x8191)&&((program[pSrch] != 0xd3)||(program[pSrch+1]!=0xff))) pSrch++;
					if (pSrch < 8191) {
						OutputDebugString("Patched Coleco (expect 8 of these!)...\n");
						program[pSrch]=0;	// from out (0xff),a
						program[pSrch+1]=0;	// to nop, nop
						++cnt;
					}
				}
				if (cnt != 8) {
					AfxMessageBox("Warning: Failed to patch Coleco code!");
				}
			}
			break;

		case 2:	// blinken
			// no limits
			if (isTIMode) {
				progsize = SIZE_OF_QPBLINKENTI;
				memcpy(program, qpBlinkenTI, progsize);
			} else {
				progsize = SIZE_OF_QPBLINKENCOLECO;
				memcpy(program, qpBlinkenColeco, progsize);
			}
			break;

		case 3:	// chuck
			// 1 file only
			if ((sidfile.GetLength() > 0)||(ayfile.GetLength() > 0)) {
				AfxMessageBox("This player only supports SN playback");
				return;
			}
			if (isTIMode) {
				progsize = SIZE_OF_QPCHUCKTI;
				memcpy(program, qpchuckti, progsize);
			} else {
				progsize = SIZE_OF_QPCHUCKCOLECO;
				memcpy(program, qpChuckColeco, progsize);
			}
			break;

		case 4:	// piano
			// 1 file only
			if ((sidfile.GetLength() > 0)||(ayfile.GetLength() > 0)) {
				AfxMessageBox("This player only supports SN playback");
				return;
			}
			if (isTIMode) {
				progsize = SIZE_OF_QPPIANOTI;
				memcpy(program, qpPianoTI, progsize);
			} else {
				progsize = SIZE_OF_QPPIANOCOLECO;
				memcpy(program, qpPianoColeco, progsize);
			}
			break;

		default:
			AfxMessageBox("Unknown program selection");
			return;
	}

    // song 2 can only be SID /or/ AY
    if (isTIMode) {
        // TI mode
        if (sidfile.GetLength() > 0) {
	        fp=fopen(sidfile, "rb");
	        if (NULL == fp) {
		        AfxMessageBox("Can't load SID file.");
		        return;
	        }
	        songsize+=fread(&song[offset], 1, (24*1024)-(6*3)-songsize, fp);
	        if (!feof(fp)) {
		        fclose(fp);
                if (songsize == offset) {
                    AfxMessageBox("Error reading song.");
                } else {
    		        AfxMessageBox("Song combination too large (24558 bytes max for this tool).");
                }
		        return;
	        }
	        fclose(fp);
	        if (songsize&1) songsize++;		// pad to word size (can't overflow cause odd->even)
        }
    } else {
        // Coleco mode
        if (ayfile.GetLength() > 0) {
	        fp=fopen(ayfile, "rb");
	        if (NULL == fp) {
		        AfxMessageBox("Can't load AY file.");
		        return;
	        }
	        songsize+=fread(&song[offset], 1, (24*1024)-(6*3)-songsize, fp);
	        if (!feof(fp)) {
		        fclose(fp);
                if (songsize == offset) {
                    AfxMessageBox("Error reading song.");
                } else {
    		        AfxMessageBox("Song combination too large (24558 bytes max for this tool).");
                }
		        return;
	        }
	        fclose(fp);
	        if (songsize&1) songsize++;		// pad to word size (can't overflow cause odd->even)
        }
    }
        
	pTxt=GetDlgItem(IDC_EDIT2);
	if (NULL == pTxt) {
		AfxMessageBox("Internal error 2. Can not proceed.");
		return;
	}
	pTxt->GetWindowText(rawtext);

	for (idx=0; idx<24; idx++) {
		int pos=rawtext.Find("\r\n");
		if (-1 == pos) break;
		text[idx]=rawtext.Left(pos+1);
		text[idx].TrimRight("\r\n ");
		rawtext=rawtext.Mid(pos+2);
	}
	if (idx<24) {
		int pos=rawtext.Find("\r\n");
		if (-1 != pos) {
			AfxMessageBox("Too much text!");
			return;
		}
		text[idx]=rawtext;
	}
	for (idx=0; idx<24; idx++) {
		if (text[idx].GetLength() > 32) {
			AfxMessageBox("Text string too long - 32 chars per line max");
			return;
		}
	}

	// find the location to dump the text
	unsigned char *p;
    int maxrows = 0;
    p=program;
    if (isTIMode) {
		// patch the EA5 header to be sure the next file is loaded
		p[128] = 0xff;
		p[129] = 0xff;
    }
	for (idx=0; idx<progsize; idx++) {
		if (0 == memcmp(p, "~~~~DATAHERE~~~~", 12)) break;
		p++;
	}
	text_offset = p-program-128-6;	// we need the offset without any headers

	if (idx>=progsize) {
		AfxMessageBox("Internal error - can't find text buffer. Failing.");
		return;
	}
    if (maxrows == 0) {
        maxrows = atoi((char*)p+16);
        if (maxrows > 24) maxrows = 24;
		text_rows = maxrows;
    }
	
	for (idx=0; idx<maxrows; idx++) {
        // center each line into the buffer
		int l=text[idx].GetLength();
        for (int x=0; x<16-(l/2); ++x) {
            text[idx] = " "+text[idx]+" ";
        }
        while (text[idx].GetLength() > 32) {
            text[idx] = text[idx].Mid(1);
        }
		memcpy(p+(idx*32), text[idx].GetBuffer(), 32);
	}

	// also go ahead and find the flags section
	p=program;
	for (idx=0; idx<progsize; idx++) {
		if (0 == memcmp(p, "~~FLAG", 6)) break;
		p++;
	}
	if (idx>=progsize) {
		AfxMessageBox("Internal error - can't find flags buffer. Failing.");
		return;
	}

	// 0-5 = ~~FLAG
	// 6-7 - first song
	// 8-9 - second song
	// 10-12 - 3 SID flags (ignored elsewhere)
	// 13 - loop
	// 14-15 - chain address (zeroed here)

	// now fill in the options
	if (snfile.GetLength() > 0) {
		// first file is always at 0xA000 (on both systems)
		if (isTIMode) {
			// big endian 9900
			p[6] = 0xa0;
			p[7] = 0x00;
		} else {
			// little endian z80
			p[6] = 0x00;
			p[7] = 0xa0;
		}
	} else {
		p[6] = 0;
		p[7] = 0;
	}

    offset+=0xa000;

    if (isTIMode) {
        if (sidfile.GetLength() > 0) {
            // big endian 9900
            p[8] = offset/256;
            p[9] = offset%256;
            p[10] = getSIDMode(IDC_COMBOSID1);
            p[11] = getSIDMode(IDC_COMBOSID2);
            p[12] = getSIDMode(IDC_COMBOSID3);
        } else {
            p[8] = 0;
            p[9] = 0;
            // don't need to fill in the SID controls
        }
    } else {
        if (ayfile.GetLength() > 0) {
            // little endian z80
            p[8] = offset%256;
            p[9] = offset/256;
            // don't need to fill in the SID controls
        } else {
            p[8] = 0;
            p[9] = 0;
            // don't need to fill in the SID controls
        }
    }

    if (loop) {
        p[13] = 1;
    } else {
        p[13] = 0;
    }

	// chain address is never set here - that's for external programs to set
	p[14] = 0;
	p[15] = 0;

	// now patch in the text information
	if (isTIMode) {
		// big endian
		program[song_loop_offset] = text_offset>>8;
		program[song_loop_offset+1] = text_offset&0xff;
	} else {
		// little endian
		program[song_loop_offset] = text_offset&0xff;
		program[song_loop_offset+1] = text_offset>>8;
	}
	program[row_byte_offset] = text_rows;

	CFileDialog dlg(FALSE);
	if (IDOK == dlg.DoModal()) {
		CString outname=dlg.GetPathName();

		// now dump it
		FILE *fp=fopen(outname, "wb");
		if (NULL == fp) {
			AfxMessageBox("Can't write output.");
			return;
		}

        if (isTIMode) {
		    fwrite(program, 1, progsize, fp);
		    fclose(fp);

		    // increment last char of name and then write the song into that
            // the song can be up to 3 files long
            for (unsigned int offset = 0; offset < songsize; offset+=8192-6) {
		        int pos=outname.Find('.');
		        if (pos==-1) {
			        pos=outname.GetLength()-1;
		        } else {
			        pos--;
			        if (pos < 0) {
				        AfxMessageBox("Bad output filename");
				        return;
			        }
		        }
		        outname.SetAt(pos, outname[pos]+1);
		        fp=fopen(outname, "wb");
		        if (NULL == fp) {
			        AfxMessageBox("Can't write output.");
			        return;
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
            memcpy(&program[0xa000-0x8000], song, songsize);
            fwrite(program, 1, 32768, fp);
            fclose(fp);
        }
	}

	free(program);
}

void CQuickPlayerDlg::OnBnClickedButton3()
{
	// cancel
	EndDialog(IDCANCEL);
}


void CQuickPlayerDlg::OnBnClickedRadio1()
{
    // TI button
    ::EnableWindow(GetDlgItem(IDC_SIDSONG)->GetSafeHwnd(),   TRUE);
    ::EnableWindow(GetDlgItem(IDC_BTNSID1)->GetSafeHwnd(),   TRUE);
    ::EnableWindow(GetDlgItem(IDC_COMBOSID1)->GetSafeHwnd(), TRUE);
    ::EnableWindow(GetDlgItem(IDC_COMBOSID2)->GetSafeHwnd(), TRUE);
    ::EnableWindow(GetDlgItem(IDC_COMBOSID3)->GetSafeHwnd(), TRUE);

    ::EnableWindow(GetDlgItem(IDC_AYSONG)->GetSafeHwnd(), FALSE);
    ::EnableWindow(GetDlgItem(IDC_BTNAY1)->GetSafeHwnd(), FALSE);
}


void CQuickPlayerDlg::OnBnClickedRadio2()
{
    // Coleco button
    ::EnableWindow(GetDlgItem(IDC_SIDSONG)->GetSafeHwnd(),   FALSE);
    ::EnableWindow(GetDlgItem(IDC_BTNSID1)->GetSafeHwnd(),   FALSE);
    ::EnableWindow(GetDlgItem(IDC_COMBOSID1)->GetSafeHwnd(), FALSE);
    ::EnableWindow(GetDlgItem(IDC_COMBOSID2)->GetSafeHwnd(), FALSE);
    ::EnableWindow(GetDlgItem(IDC_COMBOSID3)->GetSafeHwnd(), FALSE);

    ::EnableWindow(GetDlgItem(IDC_AYSONG)->GetSafeHwnd(), TRUE);
    ::EnableWindow(GetDlgItem(IDC_BTNAY1)->GetSafeHwnd(), TRUE);
}


