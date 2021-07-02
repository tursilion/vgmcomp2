// bestPacker.cpp : Runs, runs, and re-runs vgmcomp to try and find ideal packs of multiple songs
// While it's not a comprehensive search, it will come up with reasonably adequate mappings
// of songs to individual packs based on best compression size within a maximum you specify
// It's a little different from the other tools in that it just shells out, so it needs
// to have vgmcomp2.exe in the path, and the files local, to work

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <string>
#include <vector>
#include <Windows.h>

using namespace std;

const int bufSize=1024*1024;	// 1MB should be plenty big
char szBuf[bufSize+1024];
const int EXECUTE_FAILED = 0xfffe;
const int INVALID_EXIT_CODE = 0xfffd;

bool verbose = true;
int maxSize = 65536;
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
	printf("VGMComp2 'Best' Packing Tool - v20210702\n\n");

    if (argc < 3) {
        printf("bestPacket [-maxsize <n>] [-args \"<args for vgmcomp2>\"] filename1 filename2 [...]\n");
        printf("Tries all your songs and outputs a set of lists resulting in best packing\n");
        printf("(This is a good one for -deepdive, if you are going to walk away from it)\n");
        printf("-maxsize - pass in the maximum size in bytes, used to make it fit in bank switching (default 65536)\n");
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
            if (arg >= argc-2) {
                printf("Not enough arguments for maxsize!\n");
                return 1;
            }
            maxSize = atoi(argv[arg]);
        } else if (strcmp(argv[arg], "-args") == 0) {
            ++arg;
            if (arg >= argc-2) {
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

    if (arg+2 >= argc) {
        printf("Not enough arguments.\n");
        return -1;
    }

	vector<string> filenames;
	vector<string> failedfiles;
	vector<string> cmdlines;
	vector<int> cmdcounts;	// these should be in a tuple...

    for (int idx=argc-1; idx>=arg; --idx) {
		FILE *fp=NULL;
		fopen_s(&fp, argv[idx], "r");
		if (NULL == fp) {
			printf("** FAILED TO OPEN FILE %s\n", argv[idx]);
			return 1;
		} else {
			fclose(fp);
		}
		filenames.push_back(argv[idx]);
	}

	printf("Going to work with %d filenames, maximum output size of %d bytes\n", filenames.size(), maxSize);

	// build the basic command line - including the first file
retryStart:
	string thisFile = filenames.back();
	cmdlines.push_back(string("vgmcomp2 "));
	cmdlines.back() += outArgs;
	cmdlines.back() += ' ';
	cmdlines.back() += thisFile;
	filenames.pop_back();
	cmdcounts.push_back(0);

	printf("Placed %s...\n", thisFile.c_str());

	// make sure the basic commandline works
	string newcmd = cmdlines.back() + ' ' + testOutput;
	if (EXECUTE_FAILED == doExecuteCommand(newcmd.c_str())) {
		printf("******\n%s\n%s\n***** EXECUTE FAILED ON FIRST FILE - ABORTING.\n", newcmd.c_str(), szBuf);
		return 1;
	}

	// okay, we succeeded, we think. Get the output size
	char *p = strstr(szBuf, "compressed to ");
	if (NULL == p) {
		printf("******\n%s\n***** Can't find compression result ON FIRST FILE - ABORTING.\n", szBuf);
		return 1;
	}

	int bestSize = atoi(p+14);
	if (bestSize > maxSize) {
		printf("***** Can't match maxSize of %d bytes on just %s (got %d bytes), failing this file.\n", maxSize, thisFile.c_str(), bestSize);
		failedfiles.push_back(thisFile);
		if (filenames.size()) goto retryStart;
	}
	cmdcounts.back() = bestSize;		// just in case nothing beats the single file

	while (filenames.size()) {
		// okay! We have got the first filename in place, and we seem to be all right, so see how many files we can add to it
		int currentTestFile = filenames.size()-1;
		bestSize = maxSize;
		int bestIdx = -1;
		string bestFile;
		while (currentTestFile >= 0) {
			thisFile = filenames[currentTestFile];
			if (thisFile.empty()) {
				--currentTestFile;
				continue;
			}
			newcmd = cmdlines.back() + ' ' + thisFile + ' ' + testOutput;
			if (EXECUTE_FAILED == doExecuteCommand(newcmd.c_str())) {
				printf("******\n%s\n%s\n***** EXECUTE FAILED ON %s FILE - ABORTING.\n", newcmd.c_str(), szBuf, thisFile.c_str());
				return 1;
			}

			// if we didn't get an output size, it may have failed for too many notes or such, so no worries
			char *p = strstr(szBuf, "compressed to ");
			if (NULL != p) {
				int tst = atoi(p+14);
				if (tst <= bestSize) {
					// remember this is the file we want
					bestFile = thisFile;
					bestIdx = currentTestFile;
					bestSize = tst;
					if (verbose) printf("File %s reduced bestSize to %d\n", thisFile.c_str(), bestSize);
				}
			}
			--currentTestFile;
		}

		// okay, we went through this list - if we found a best one, add it to the list and then repeat
		// if we didn't, then we are done with this list
		if (bestIdx == -1) {
			// this command is complete - start up a new one
			if (verbose) printf("Finished command string '%s' with best size %d bytes\n", cmdlines.back().c_str(), cmdcounts.back());

		tryagain:
			thisFile = "";
			while (thisFile.empty()) {
				if (filenames.size() == 0) break;
				thisFile = filenames.back();
				filenames.pop_back();
			}
			if (filenames.size() == 0) continue;

			cmdlines.push_back(string("vgmcomp2 "));
			cmdlines.back() += outArgs;
			cmdlines.back() += ' ';
			cmdlines.back() += thisFile;
			if (verbose) printf("Testing %s...\n", thisFile.c_str());
			cmdcounts.push_back(0);

			// kind of dumb to duplicate this code...
			newcmd = cmdlines.back() + ' ' + testOutput;
			if (EXECUTE_FAILED == doExecuteCommand(newcmd.c_str())) {
				printf("******\n%s\n%s\n***** EXECUTE FAILED ON FIRST FILE - ABORTING.\n", newcmd.c_str(), szBuf);
				return 1;
			}

			// okay, we succeeded, we think. Get the output size
			char *p = strstr(szBuf, "compressed to ");
			if (NULL == p) {
				printf("******\n%s\n***** Can't find compression result ON FIRST FILE - ABORTING.\n", szBuf);
				return 1;
			}

			bestSize = atoi(p+14);
			if (bestSize > maxSize) {
				printf("***** Can't match maxSize of %d bytes on just %s (got %d bytes), failing this file.\n", maxSize, thisFile.c_str(), bestSize);
				failedfiles.push_back(thisFile);
				goto tryagain;
			}
			cmdcounts.back() = bestSize;		// just in case nothing beats the single file
			bestSize = maxSize;
			bestIdx = -1;
		} else {
			// we can just append this filename and keep searching
			cmdlines.back() += ' ' + bestFile;
			filenames[bestIdx] = "";
			printf("Placed %s...\n", bestFile.c_str());
			cmdcounts.back() = bestSize;
		}
	}

	printf("* Search finished. %d commandlines found.\n", cmdlines.size());
	for (unsigned int idx=0; idx<cmdlines.size(); ++idx) {
		printf("@rem Output size: %d bytes\n", cmdcounts[idx]);
		printf("%s OutFile%02d.sbf\n", cmdlines[idx].c_str(), idx);
	}
	if (failedfiles.size() > 0) {
		printf("** WARNING **\nFailed files:\n");
		for (unsigned int idx=0; idx<failedfiles.size(); ++idx) {
			printf(" - %s\n", failedfiles[idx].c_str());
		}
	}

	printf("\n**DONE**\n");
}
