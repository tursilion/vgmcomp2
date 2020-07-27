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
#include "quickplayColeco.c"    // remember to remove const
#include "quickplayTI.c"        // remember to remove const and fix EA#5 header for more files

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

unsigned char *textbufferTI=NULL;
unsigned char *textbufferCOL=NULL;

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
	char song[24*1024];
	size_t songsize;
	CString snfile;
    CString sidfile;
    CString ayfile;
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

    // song 2 can only be SID /or/ AY
    CWnd *pTISong = GetDlgItem(IDC_RADIO1);
    bool isTIMode = (((CButton*)pTISong)->GetCheck() == BST_CHECKED);
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
		for (idx=0; idx<1024; idx++) {
			if (0 == memcmp(p, "~~~~DATAHERE~~~~", 12)) break;
			p++;
		}
		if (idx>=1024) {
			AfxMessageBox("Internal error - can't find text buffer. Failing.");
			return;
		}
        if (isTIMode) {
    		textbufferTI=p;
        } else {
    		textbufferCOL=p;
        }
	}
	
	for (idx=0; idx<24; idx++) {
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

    // now fill in the options
    if (snfile.GetLength() > 0) {
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
        if (sidfile.GetLength() > 0) {
            // big endian 9900
            p[770] = offset/256;
            p[771] = offset%256;
            p[772] = getSIDMode(IDC_COMBOSID1);
            p[773] = getSIDMode(IDC_COMBOSID2);
            p[774] = getSIDMode(IDC_COMBOSID3);
        } else {
            p[770] = 0;
            p[771] = 0;
            // don't need to fill in the SID controls
        }
    } else {
        if (ayfile.GetLength() > 0) {
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
		    fwrite(quickplayti, 1, SIZE_OF_QUICKPLAYTI, fp);
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
            memcpy(&quickplaycoleco[0xa000-0x8000], song, songsize);
            fwrite(quickplaycoleco, 1, 32768, fp);
            fclose(fp);
        }
	}
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


