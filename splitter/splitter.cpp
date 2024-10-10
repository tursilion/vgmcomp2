// splitter.cpp : Chops up a snsbf file to try and fit a target size. Repeatedly runs vgmcomp2
// until it has sent the entire file. Early version, a lot is hard coded...
// It's a little different from the other tools in that it just shells out, so it needs
// to have vgmcomp2.exe in the path, and the files local, to work

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <string>
#include <list>
#include <Windows.h>

using namespace std;

const int bufSize=1024*1024;	// 1MB should be plenty big
char szBuf[bufSize+1024];
const int EXECUTE_FAILED = 0xfffe;
const int INVALID_EXIT_CODE = 0xfffd;

bool verbose = true;
int maxSize = 8100;     // 8k minus a bit for cartridge headers and fluff
int slack = 5;          // 5 percent slack is close enough
const char *testOutput = "dummyxyzx.xxx";

// Runs pCmdLine as child process & redirect its stdIn and stdOut.
// Inputs:  Fullpath of command line
// Outputs: Returns the exit code of the called process
//          INVALID_EXIT_CODE if the code could not be retrieved,
//          EXECUTE_FAILED if anything else went wrong
// fills in szBuf with the output text
int doExecuteCommand(const char *pCmdLine) {
	SECURITY_ATTRIBUTES sa;
	HANDLE hChildStdoutRdTmp, hChildStdoutWr, hChildStdoutRd;
	HANDLE hChildStderrWr;
	HANDLE hChildStdinRd, hChildStdinWrTmp, hChildStdinWr;
	BOOL bSuccess;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD dwRead;

	if (verbose) printf(">> Execute %s\n", pCmdLine);

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	// Create a pipe for Stdout
	if (!CreatePipe(&hChildStdoutRdTmp, &hChildStdoutWr, &sa, 0)) {
		printf("Could not create pipe for Stdout redirection\n");
		return EXECUTE_FAILED;
	}

	// Create a separate handle for Stderr
	bSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutWr, GetCurrentProcess(),
		&hChildStderrWr, 0, TRUE, DUPLICATE_SAME_ACCESS);
	if (!bSuccess) {
		printf("Unable to duplicate handle for Stderr\n");
		CloseHandle(hChildStdoutRdTmp);
		CloseHandle(hChildStdoutWr);
		return EXECUTE_FAILED;
	}

	// Create non-inheritable copies of the stdout read and stdin write (below)
	// This is needed to ensure that the handles are closable
	bSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutRdTmp, GetCurrentProcess(),
		&hChildStdoutRd, 0, FALSE, DUPLICATE_SAME_ACCESS);
	if (!bSuccess) {
		printf("Unable to duplicate handle for Stdout redirection\n");
		CloseHandle(hChildStderrWr);
		CloseHandle(hChildStdoutRd);
		CloseHandle(hChildStdoutWr);
		return EXECUTE_FAILED;
	}
	CloseHandle(hChildStdoutRdTmp);

	// Do the same for Stdin
	if (!CreatePipe(&hChildStdinRd, &hChildStdinWrTmp, &sa, 0)) {
		printf("Could not create pipe for Stdin redirection\n");
		CloseHandle(hChildStderrWr);
		CloseHandle(hChildStdoutRd);
		CloseHandle(hChildStdoutWr);
		return EXECUTE_FAILED;
	}

	bSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdinWrTmp, GetCurrentProcess(),
		&hChildStdinWr, 0, FALSE, DUPLICATE_SAME_ACCESS);
	if (!bSuccess) {
		printf("Unable to duplicate handle for Stdin redirection\n");
		CloseHandle(hChildStderrWr);
		CloseHandle(hChildStdoutRd);
		CloseHandle(hChildStdoutWr);
		CloseHandle(hChildStdinRd);
		CloseHandle(hChildStdinWrTmp);
		return EXECUTE_FAILED;
	}
	CloseHandle(hChildStdinWrTmp);

	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	si.hStdInput = hChildStdinRd;
	si.hStdError = hChildStderrWr;
	si.hStdOutput = hChildStdoutWr;
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWMINNOACTIVE;
	ZeroMemory( &pi, sizeof(pi) );

	const char *pszSystemRoot = ".\\";

	// Start the child process. 
	if( !CreateProcess( NULL, // No module name (use command line). 
		(LPSTR)pCmdLine,         // Command line. 
		NULL,             // Process handle not inheritable. 
		NULL,             // Thread handle not inheritable. 
		TRUE,             // Set handle inheritance to TRUE. 
		0,                // No creation flags. 
		NULL,             // Use parent's environment block. 
		pszSystemRoot,	  // Use SystemRoot as current directory
		&si,              // Pointer to STARTUPINFO structure.
		&pi )             // Pointer to PROCESS_INFORMATION structure.
		) 
	{
		printf("Failed to create process using path %s, return code %d\n", pCmdLine, GetLastError());
		CloseHandle(hChildStderrWr);
		CloseHandle(hChildStdoutWr);
		CloseHandle(hChildStdoutRd);
		CloseHandle(hChildStdinRd);
		CloseHandle(hChildStdinWr);
		return EXECUTE_FAILED;
	}

	// Close the handles used by the child, they are no longer needed
	CloseHandle(hChildStderrWr);
	CloseHandle(hChildStdoutWr);
	CloseHandle(hChildStdinRd);

	int nLength = 0;
	int nPosition = 0;
	int nPrinted = 0;
	DWORD dwAvail;

	// Loop until the process exits, collecting (and printing!) it's output
	while (WAIT_TIMEOUT == WaitForSingleObject(pi.hProcess, 0)) {
		if (PeekNamedPipe(hChildStdoutRd, NULL, 0, NULL, &dwAvail, NULL)) {
			if (dwAvail < 1) {
				Sleep(200);
				continue;
			}
		} else {
			Sleep(200);
			continue;
		}

		bSuccess = ReadFile(hChildStdoutRd, &szBuf[nPosition], bufSize - nPosition, &dwRead, NULL);
		if (!bSuccess) {
			if (ERROR_MORE_DATA != GetLastError()) {
				break;
			}
		}
		if (0 == dwRead) {
			continue;
		}
		nLength = nPosition + dwRead;
		szBuf[nLength] = '\0';

		nPosition = nLength;
		nPrinted = nLength;

		if (nPosition > bufSize-10) {
			printf("** Out of text buffer - preserving first 5k and wrapping the rest.\n");
			nPosition = 5*1024;
		}
	}

	// Get the exit code of the spawned process
	DWORD dwExitCode;
	int nExitCode;
	if (GetExitCodeProcess( pi.hProcess, &dwExitCode )) {
		nExitCode = int(dwExitCode);
	} else {
		nExitCode = INVALID_EXIT_CODE;
	}

	// Close process and thread handles. 
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	
	// get the rest of the output
	for(;;) {
		DWORD dwAvail;
		if (PeekNamedPipe(hChildStdoutRd, NULL, 0, NULL, &dwAvail, NULL)) {
			if (dwAvail < 1) {
				break;
			}
		} else {
			break;
		}

		bSuccess = ReadFile(hChildStdoutRd, &szBuf[nPosition], bufSize - nPosition, &dwRead, NULL);
		if ((!bSuccess)||(0 == dwRead)) {
			if (ERROR_MORE_DATA != GetLastError()) {
				break;
			}
		}
		nLength = nPosition + dwRead;
		szBuf[nLength] = '\0';

		nPosition = nLength;
		nPrinted = nLength;

		if (nPosition > bufSize-10) {
			printf("** Out of text buffer - preserving first 5k and wrapping the rest.\n");
			nPosition = 5*1024;
		}
	}

	// close read and write handles
	CloseHandle(hChildStdoutRd);
	CloseHandle(hChildStdinWr);

	// we're done
	return nExitCode;
}

int main(int argc, char *argv[])
{
	printf("VGMComp2 Splitting Tool - v20241010\n\n");

    if (argc < 3) {
        printf("splitter [-maxsize <n>] [-args \"<args for vgmcomp2>\"] filename\n");
        printf("Splits up the snpsg file repeatedly for vgmcomp2 until the compressed file fits the size requested.\n");
        printf("(This is a good one for -deepdive, if you are going to walk away from it)\n");
        printf("-maxsize - pass in the maximum size in bytes, used to make it fit in bank switching (default 8100)\n");
        printf("-slack - pass a slack value as a whole number percent (default is '5')\n");
        printf("-args - make sure the arguments for vgmcomp2 are in quotation marks! You usually need at least -sn\n");
        printf("-q - quiet verbose output\n");
		printf("Warning: test output file '%s' will be overwritten if it exists.\n", testOutput);
        return 1;
    }

    // check arguments
    int arg = 1;
	const char *outArgs = "";

    while ((arg < argc) && (argv[arg][0] == '-')) {
        if (strcmp(argv[arg], "-q") == 0) {
            verbose = false;
        } else if (strcmp(argv[arg], "-maxsize") == 0) {
            ++arg;
            if (arg >= argc-1) {
                printf("Not enough arguments for maxsize!\n");
                return 1;
            }
            maxSize = atoi(argv[arg]);
        } else if (strcmp(argv[arg], "-slack") == 0) {
            ++arg;
            if (arg >= argc-1) {
                printf("Not enough arguments for slack!\n");
                return 1;
            }
            slack = atoi(argv[arg]);
            if (slack < 1) {
                printf("Slack can not be less than 1! Using 1.\n");
                slack = 1;
            }
        } else if (strcmp(argv[arg], "-args") == 0) {
            ++arg;
            if (arg >= argc-1) {
                printf("Not enough arguments for args!\n");
                return 1;
            }
            outArgs = argv[arg];
        } else {
            printf("Unrecognized option '%s'\n", argv[arg]);
            return -1;
        }
        ++arg;
    }

    if (arg >= argc) {
        printf("Not enough arguments.\n");
        return -1;
    }

	std::list<string> lines;

	FILE *fp=NULL;
	fopen_s(&fp, argv[arg], "r");
	if (NULL == fp) {
		printf("** FAILED TO OPEN FILE %s\n", argv[arg]);
		return 1;
	} else {
        while (!feof(fp)) {
            char buf[256];
            if (NULL == fgets(buf, sizeof(buf), fp)) break;
            lines.push_back(buf);
        }
		fclose(fp);
	}

    int currentTest = lines.size();
    int stepSize = currentTest;
    int fileIndex = 1;
    int lineSlack = currentTest*slack/100+1;
	printf("Input file has %d lines, target size %d bytes with slack of %d%% (%d bytes)\n", lines.size(), maxSize, slack, lineSlack);

	// build the basic command line - including the output file
retryStart:
    char fn[128];
    sprintf(fn, "%s_%02d.sbf", argv[arg], fileIndex);
	string thisFile = testOutput;
    string cmdline;
	cmdline = string("vgmcomp2 ");
	cmdline += outArgs;
	cmdline += ' ';
	cmdline += thisFile;
    cmdline += ' ';
    cmdline += fn;
    std::list<string> workLines = lines;

    if (currentTest < 2) {
        printf("** Warning: ended with %d lines remaining...\n", currentTest);
    } else {
	    if (verbose) printf("Testing %s_%02d (%d lines with %d lines left)...\n", thisFile.c_str(), fileIndex, currentTest, lines.size());

        // write out the test file
        {
            FILE *fp = fopen(testOutput, "w");
            if (NULL == fp) {
                printf("*** Can't write temporary file %s, abort.\n", testOutput);
                return 1;
            }
            for (int idx=0; idx<currentTest; ++idx) {
                if (workLines.empty()) break;
                fprintf(fp, "%s", workLines.front().c_str());
                workLines.pop_front();
            }
            fclose(fp);
        }

	    // run the commandline
	    string newcmd = cmdline;
	    if (EXECUTE_FAILED == doExecuteCommand(newcmd.c_str())) {
		    printf("******\n%s\n%s\n***** EXECUTE FAILED - ABORTING.\n", newcmd.c_str(), szBuf);
		    return 1;
	    }

        // delete the temporary file
        remove(testOutput);

	    // okay, we succeeded, we think. Get the output size
	    char *p = strstr(szBuf, "compressed to ");
	    if (NULL == p) {
		    printf("******\n%s\n***** Can't find compression result - ABORTING.\n", szBuf);
		    return 1;
	    }

	    int bestSize = atoi(p+14);
        if (bestSize > maxSize) {
            if (verbose) printf("Got %d bytes, too large.\n", bestSize);
            remove(fn);
            if (stepSize <= 2) {
                // nothing we can do, we are done. Go back to the previous test
                currentTest -= stepSize;
                if (currentTest < 2) {
                    // this shouldn't happen, but just in case we started funny
                    printf("***** Impossible test - out of lines. Check requirements.\n");
                    return 1;
                }
                stepSize = 0;
                goto retryStart;
            }
            stepSize = (stepSize+1)/2;          // round up
            currentTest -= stepSize;
            if (currentTest < 2) {
                printf("***** Impossible test - out of lines. Check requirements.\n");
                return 1;
            }
            goto retryStart;
        }
        if (bestSize < maxSize-lineSlack) {
            // make sure this wasn't all we had left anyway
            if ((unsigned)currentTest < lines.size()) {
                if (verbose) printf("Got %d bytes, too small.\n", bestSize);
                remove(fn);
                stepSize = (stepSize+1)/2;
                if (stepSize < 1) {
                    printf("Out of steps, this is as close as we can get. Adjust slack!\n");
                } else {
                    currentTest += stepSize;    // make sure to round down
                    if (currentTest < 2) {
                        printf("***** Impossible test - out of lines. Check requirements.\n");
                        return 1;
                    }
                    goto retryStart;
                }
            }
        }
        if (verbose) printf("File %d got size of %d\n", fileIndex, bestSize);
        ++fileIndex;
        lines = workLines;
        currentTest = lines.size();
        stepSize = currentTest;
        if (!lines.empty()) goto retryStart;
    }

	printf("\n**DONE** %d files generated.\n", fileIndex-1);
}
