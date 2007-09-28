/* KitServer 7 Setup */
/* Version 1.0 (with Win32 GUI) by Juce. */

#include <windows.h>
#include <windef.h>
#include <string.h>
#include <stdio.h>

#define DLL_NAME "kload\0"
#define DLL_NAME_SET "kset\0"
#define DEFAULT_EXE_NAME "PES2008DEMO.exe\0"
#define BUFLEN 4096

#include "imageutil.h"
#include "setupgui.h"
#include "setup.h"
#include "detect.h"

HWND hWnd = NULL;
bool g_noFiles = false;
bool g_advancedMode = false;
char mydir[BUFLEN];
char patchFolderName[BUFLEN]={0};
char patchExeName[BUFLEN]={0};
char patchTitle[BUFLEN]={0};
char installDllPath[BUFLEN]={0};
char installDllSetPath[BUFLEN]={0};

// array of LoadLibrary address on diff. OS
DWORD LoadLibraryAddr[] = {
	0,            // auto-detect
	0x77e7d961,   // WinXP
	0x7c5768fb,   // Win2K 
};

void MyMessageBox(char* fmt, DWORD value);
void MyMessageBox2(char* fmt, char* value);
void UpdateInfo(void);

void MyMessageBox(char* fmt, DWORD value)
{
#ifndef MYDLL_RELEASE_BUILD
	// show message box with error msg
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, fmt, value);
	MessageBox(hWnd, buf, "KitServer 7 DEBUG MyMessage", 0);
#endif
}

void MyMessageBox2(char* fmt, char* value)
{
#ifndef MYDLL_RELEASE_BUILD
	// show message box with error msg
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, fmt, value);
	MessageBox(hWnd, buf, "KitServer 7 DEBUG MyMessage", 0);
#endif
}

/**
 * Installs the kitserver DLL.
 */
void InstallKserv(void)
{
	// disable buttons, 'cause it may take some time
	EnableWindow(g_installButtonControl, FALSE);
	EnableWindow(g_removeButtonControl, FALSE);
	
	char outmsg[1024];
	ZeroMemory(outmsg, 1024);
	
	for (int i=0; i<2; i++) {
		HWND listControl = (i==0)?g_exeListControl:g_setListControl;
		strcat(outmsg, (i==0)?">>> Game EXE:\n":"\n\n>>> Settings EXE:\n");

		char fileName[BUFLEN];
		ZeroMemory(fileName, BUFLEN);
		lstrcpy(fileName, "..\\");
		char* p = fileName + lstrlen(fileName);
	
		// get currently selected item and its text
		int idx = (int)SendMessage(listControl, CB_GETCURSEL, 0, 0);
		if (idx == 0) {
			strcat(outmsg, "No action required.");
			continue;
		}
		SendMessage(listControl, CB_GETLBTEXT, idx, (LPARAM)p);
	
		// check if it's a recognizable EXE-file
		if (GetGameVersion(fileName) == -1)
		{
			// show message box with error msg
			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			sprintf(buf, "\
======== WRONG FILE! =========\n\
File %s is an unknown EXE-file.\n\
Therefore,\n\
KitServer will NOT be attached to it.", fileName);
	
			strcat(outmsg, buf);
			continue;
		}
		
		if (isGame(GetGameVersion(fileName)) != (i==0)) {
			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			sprintf(buf, "\
======== WRONG FILE! =========\n\
File %s is a %s EXE instead of a %s EXE-file.\n\
Therefore,\n\
KitServer will NOT be attached to it.", fileName, (i==0)?"settings":"game",
																														(i==0)?"game":"settings");
			
			strcat(outmsg, buf);
			continue;
		}
	
		// get the address of LoadLibrary function, which is different 
		// Here, we're taking advantage of the fact that Kernel32.dll, 
		// which contains the LoadLibrary function is never relocated, so 
		// the address of LoadLibrary will always be the same.
		HMODULE krnl = GetModuleHandle("kernel32.dll");
		DWORD loadLib = (DWORD)GetProcAddress(krnl, "LoadLibraryA");
		// Well, this doesn't work for Vista, so we don't determine the
		// address ourselves but use the one from the import table, for
		// which we get the pointer with the getImportThunkRVA command
	
	    /*
		// get currently selected item in OS choices list
		int osIdx = (int)SendMessage(g_osListControl, CB_GETCURSEL, 0, 0);
		if (osIdx > 0)
		{
			// override calculated LoadLibrary address with standard address
			// for chosen OS.
			loadLib = LoadLibraryAddr[osIdx];
		}
	    */
	
		DWORD ep, ib;
		DWORD dataOffset, dataVA;
		DWORD codeOffset, codeVA;
		DWORD loadLibAddr, kservAddr, loadLibAddr1;
		DWORD newEntryPoint;
	
		FILE* f = fopen(fileName, "r+b");
		if (f != NULL)
		{
			// Install
			loadLibAddr1 = getImportThunkRVA(f, "kernel32.dll","LoadLibraryA");
			
			if (SeekEntryPoint(f))
			{
				fread(&ep, sizeof(DWORD), 1, f);
				//printf("Entry point: %08x\n", ep);
			}
			if (SeekImageBase(f))
			{
				fread(&ib, sizeof(DWORD), 1, f);
				//printf("Image base: %08x\n", ib);
			}
	
			IMAGE_SECTION_HEADER dataHeader;
			ZeroMemory(&dataHeader, sizeof(IMAGE_SECTION_HEADER));
			dataOffset = 0;
			dataVA = 0;
	
			// find empty space at the end of .rdata
			if (SeekSectionHeader(f, ".rdata")) {
				fread(&dataHeader, sizeof(IMAGE_SECTION_HEADER), 1, f);
	
	            //adjust SizeOfRawData (needed for WE9LEK-nocd)
	            int rem = dataHeader.SizeOfRawData % 0x80;
	            if (rem > 0) {
	                dataHeader.SizeOfRawData += 0x80 - rem;
	                fseek(f, -sizeof(IMAGE_SECTION_HEADER), SEEK_CUR);
	                fwrite(&dataHeader, sizeof(IMAGE_SECTION_HEADER), 1, f);
	            }
	
				dataVA = dataHeader.VirtualAddress + dataHeader.SizeOfRawData;
				if (dataHeader.PointerToRawData != 0)
					dataOffset = dataHeader.PointerToRawData + dataHeader.SizeOfRawData;
				else
					dataOffset = dataHeader.VirtualAddress + dataHeader.SizeOfRawData;
	
				// shift 32 bytes back
				dataOffset -= 0x20;
				dataVA -= 0x20;
			}
	
			MyMessageBox("dataOffset = %08x", dataOffset);
			MyMessageBox("dataVA = %08x", dataVA);
	
			if (dataOffset != 0) {
				// at the found empty place, write the LoadLibrary address, 
				// and the name of kserv.dll
				BYTE buf[0x20], zero[0x20];
				ZeroMemory(zero, 0x20);
				ZeroMemory(buf, 0x20);
	
				fseek(f, dataOffset, SEEK_SET);
				fread(&buf, 0x20, 1, f);
				if (memcmp(buf, zero, 0x20)==0)
				{
					// ok, we found an empty place. Let's live here.
					fseek(f, -0x20, SEEK_CUR);
					DWORD* p = (DWORD*)buf;
					p[0] = ep; // save old empty pointer for easy uninstall
					p[1] = loadLib;
					if (i==0)
						memcpy(buf + 8, installDllPath, lstrlen(installDllPath)+1);
					else
						memcpy(buf + 8, installDllSetPath, lstrlen(installDllSetPath)+1);
					fwrite(buf, 0x20, 1, f);
					
					loadLibAddr = ib + dataVA + sizeof(DWORD);
					//printf("loadLibAddr = %08x\n", loadLibAddr);
					kservAddr = loadLibAddr + sizeof(DWORD);
					//printf("kservAddr = %08x\n", kservAddr);
					
					loadLibAddr = ib + loadLibAddr1;
				}
				else
				{
					//printf("Already installed.\n");
					fclose(f);
	
					// show message box with error msg
					char buf[BUFLEN];
					ZeroMemory(buf, BUFLEN);
					sprintf(buf, "\
======== INFORMATION! =========\n\
KitServer 7 is already installed (1) for\n\
%s.", fileName);
	
					strcat(outmsg, buf);
					continue;
				}
			}
	
			IMAGE_SECTION_HEADER textHeader;
			ZeroMemory(&textHeader, sizeof(IMAGE_SECTION_HEADER));
			codeOffset = 0;
			codeVA = 0;
	
			// find empty space at the end of .text
	        bool textFound = SeekSectionHeader(f, ".text");
	        if (!textFound) textFound = SeekSectionHeader(f,"");
			if (textFound) {
				fread(&textHeader, sizeof(IMAGE_SECTION_HEADER), 1, f);
	
	            //adjust SizeOfRawData (needed for WE9LEK-nocd)
	            int rem = textHeader.SizeOfRawData % 0x40;
	            if (rem > 0) {
	                textHeader.SizeOfRawData += 0x40 - rem;
	                fseek(f, -sizeof(IMAGE_SECTION_HEADER), SEEK_CUR);
	                fwrite(&textHeader, sizeof(IMAGE_SECTION_HEADER), 1, f);
	            }
	
				codeVA = textHeader.VirtualAddress + textHeader.SizeOfRawData;
				if (textHeader.PointerToRawData != 0)
					codeOffset = textHeader.PointerToRawData + textHeader.SizeOfRawData;
				else
					codeOffset = textHeader.VirtualAddress + textHeader.SizeOfRawData;
	
				// shift 32 bytes back.
				codeOffset -= 0x20;
				codeVA -= 0x20;
			} else {
	            MyMessageBox("section header for '.text' not found", 0);
	        }
	
			MyMessageBox("codeOffset = %08x", codeOffset);
			MyMessageBox("codeVA = %08x", codeVA);
	
			if (codeOffset != 0) {
				// at the found place, write the new entry point logic
				BYTE buf[0x20], zero[0x20];
				ZeroMemory(zero, 0x20);
				ZeroMemory(buf, 0x20);
	
				fseek(f, codeOffset, SEEK_SET);
				fread(&buf, 0x20, 1, f);
				if (memcmp(buf, zero, 0x20)==0)
				{
					// ok, we found an empty place. Let's live here.
					fseek(f, -0x20, SEEK_CUR);
					buf[0] = 0x68;  // push
					DWORD* p = (DWORD*)(buf + 1); p[0] = kservAddr;
					buf[5] = 0xff; buf[6] = 0x15; // call
					p = (DWORD*)(buf + 7); p[0] = loadLibAddr;
					buf[11] = 0xe9; // jmp
					p = (DWORD*)(buf + 12); p[0] = ib + ep - 5 - (ib + codeVA + 11);
					fwrite(buf, 0x20, 1, f);
	
					newEntryPoint = codeVA;
				}
				else
				{
					//printf("Already installed.\n");
					fclose(f);
	
					// show message box with error msg
					char buf[BUFLEN];
					ZeroMemory(buf, BUFLEN);
					sprintf(buf, "\
======== INFORMATION! =========\n\
KitServer 7 is already installed (2) for\n\
%s.", fileName);
	
					strcat(outmsg, buf);
					continue;
				}
			}
			if (SeekEntryPoint(f))
			{
				// write new entry point
				fwrite(&newEntryPoint, sizeof(DWORD), 1, f);
				//printf("New entry point: %08x\n", newEntryPoint);
			}
	        /*
			if (SeekCodeSectionFlags(f))
			{
				DWORD flags;
				fread(&flags, sizeof(DWORD), 1, f);
				flags |= 0x80000000; // make code section writeable
				fseek(f, -sizeof(DWORD), SEEK_CUR);
				fwrite(&flags, sizeof(DWORD), 1, f);
			}
	        */
			fclose(f);
	
			// show message box with success msg
			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			sprintf(buf, "\
======== SUCCESS! =========\n\
Setup has installed KitServer 7 for\n\
%s.", fileName);
	
			strcat(outmsg, buf);
			continue;
		}
		else
		{
			// show message box with error msg
			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			sprintf(buf, "\
======== ERROR! =========\n\
Setup failed to install KitServer 7 for\n\
%s.\n\
\n\
(No modifications made.)\n\
Verify that the executable is not\n\
READ-ONLY, and try again.", fileName);
	
			strcat(outmsg, buf);
			continue;
		}
		
	}
	
	UpdateInfo();
	
	MessageBox(hWnd, outmsg, "KitServer 7 Setup Message", 0);	
}

/**
 * Uninstalls the kitserver DLL.
 */
void RemoveKserv(void)
{
	// disable buttons, 'cause it may take some time
	EnableWindow(g_installButtonControl, FALSE);
	EnableWindow(g_removeButtonControl, FALSE);

	char outmsg[1024];
	ZeroMemory(outmsg, 1024);
	
	for (int i=0; i<2; i++) {
		HWND listControl = (i==0)?g_exeListControl:g_setListControl;
		strcat(outmsg, (i==0)?">>> Game EXE:\n":"\n\n>>> Settings EXE:\n");

		char fileName[BUFLEN];
		ZeroMemory(fileName, BUFLEN);
		lstrcpy(fileName, "..\\");
		char* p = fileName + lstrlen(fileName);
	
		// get currently selected item and its text
		int idx = (int)SendMessage(listControl, CB_GETCURSEL, 0, 0);
		if (idx == 0) {
			strcat(outmsg, "No action required.");
			continue;
		}
		SendMessage(listControl, CB_GETLBTEXT, idx, (LPARAM)p);
	
		DWORD ep, ib;
		DWORD dataOffset, dataVA;
		DWORD codeOffset, codeVA;
		DWORD loadLibAddr, kservAddr;
		DWORD newEntryPoint;
	
		FILE* f = fopen(fileName, "r+b");
		if (f != NULL)
		{
			if (SeekEntryPoint(f))
			{
				fread(&ep, sizeof(DWORD), 1, f);
				//printf("Current entry point: %08x\n", ep);
			}
	
			IMAGE_SECTION_HEADER dataHeader;
			ZeroMemory(&dataHeader, sizeof(IMAGE_SECTION_HEADER));
			dataOffset = 0;
			dataVA = 0;
	
			// find empty space at the end of .rdata
			if (SeekSectionHeader(f, ".rdata")) {
				fread(&dataHeader, sizeof(IMAGE_SECTION_HEADER), 1, f);
	
				dataVA = dataHeader.VirtualAddress + dataHeader.SizeOfRawData;
				if (dataHeader.PointerToRawData != 0)
					dataOffset = dataHeader.PointerToRawData + dataHeader.SizeOfRawData;
				else
					dataOffset = dataHeader.VirtualAddress + dataHeader.SizeOfRawData;
	
				// shift 32 bytes back
				dataOffset -= 0x20;
				dataVA -= 0x20;
			}
	
			MyMessageBox("dataOffset = %08x", dataOffset);
			MyMessageBox("dataVA = %08x", dataVA);
	
			if (dataOffset != 0) {
				// if already installed, this location should contain
				// some saved data.
				BYTE zero[0x20];
				ZeroMemory(zero, 0x20);
				fseek(f, dataOffset, SEEK_SET);
	
				// read saved old entry point
				fread(&newEntryPoint, sizeof(DWORD), 1, f);
				if (newEntryPoint == 0)
				{
					//printf("Already uninstalled.\n");
					fclose(f);
	
					// show message box with error msg
					char buf[BUFLEN];
					ZeroMemory(buf, BUFLEN);
					sprintf(buf, "\
======== INFORMATION! =========\n\
KitServer 7 is not installed for\n\
%s.", fileName);
	
					strcat(outmsg, buf);
					continue;
				}
				// zero out the bytes
				fseek(f, -sizeof(DWORD), SEEK_CUR);
				fwrite(zero, 0x20, 1, f);
			}
	
			IMAGE_SECTION_HEADER textHeader;
			ZeroMemory(&textHeader, sizeof(IMAGE_SECTION_HEADER));
			codeOffset = 0;
			codeVA = 0;
	
			// find empty space at the end of .text
	        bool textFound = SeekSectionHeader(f, ".text");
	        if (!textFound) textFound = SeekSectionHeader(f,"");
			if (textFound) {
				fread(&textHeader, sizeof(IMAGE_SECTION_HEADER), 1, f);
	
				codeVA = textHeader.VirtualAddress + textHeader.SizeOfRawData;
				if (textHeader.PointerToRawData != 0)
					codeOffset = textHeader.PointerToRawData + textHeader.SizeOfRawData;
				else
					codeOffset = textHeader.VirtualAddress + textHeader.SizeOfRawData;
	
				// shift 32 bytes back.
				codeOffset -= 0x20;
				codeVA -= 0x20;
			}
	
			MyMessageBox("codeOffset = %08x", codeOffset);
			MyMessageBox("codeVA = %08x", codeVA);
	
			if (codeOffset != 0) {
				// if installed, this should have the new entry point logic
				BYTE zero[0x20];
				BYTE buf[0x20];
				ZeroMemory(zero, 0x20);
				ZeroMemory(buf, 0x20);
	
				fseek(f, codeOffset, SEEK_SET);
				fread(buf, 0x20, 1, f);
				if (memcmp(buf, zero, 0x20)!=0) {
					fseek(f, -0x20, SEEK_CUR);
					fwrite(zero, 0x20, 1, f);
				}
			}
	
			if (SeekEntryPoint(f))
			{
				// write new entry point
				fwrite(&newEntryPoint, sizeof(DWORD), 1, f);
				//printf("New entry point: %08x\n", newEntryPoint);
			}
	        /*
			if (SeekCodeSectionFlags(f))
			{
				DWORD flags;
				fread(&flags, sizeof(DWORD), 1, f);
				if (flags & 0x20000000) {
					// if section was marked as executable (meaning that it's
					// a normal EXE - not compressed one), then turn off the
					// writeable flag that we turned on when installing
					flags &= 0x7fffffff;
				}
				fseek(f, -sizeof(DWORD), SEEK_CUR);
				fwrite(&flags, sizeof(DWORD), 1, f);
			}
	        */
			fclose(f);
	
			// show message box with error msg
			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			sprintf(buf, "\
======== SUCCESS! =========\n\
Setup has removed KitServer 7 from\n\
%s.", fileName);
	
			strcat(outmsg, buf);
			continue;
		}
		else
		{
			// show message box with error msg
			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			sprintf(buf, "\
======== ERROR! =========\n\
Setup failed to remove KitServer 7 from\n\
%s.\n\
\n\
(No modifications made.)\n\
Verify that the executable is not\n\
READ-ONLY, and try again.", fileName);
	
			strcat(outmsg, buf);
			continue;
		}
	}
	
	UpdateInfo();
	
	MessageBox(hWnd, outmsg, "KitServer 7 Setup Message", 0);	
}

/**
 * Updates the information about exe file.
 */
void UpdateInfo(void)
{
	if (g_noFiles) return;

	EnableWindow(g_installButtonControl, FALSE);
	EnableWindow(g_removeButtonControl, FALSE);
	
	bool isValidExe[2] = {false, false};
	bool isInstalled[2] = {false, false};
	
	for (int i=0; i<2; i++) {
		HWND listControl = (i==0)?g_exeListControl:g_setListControl;
		HWND infoControl = (i==0)?g_exeInfoControl:g_setInfoControl;
		
		char fileName[BUFLEN];
		ZeroMemory(fileName, BUFLEN);
		lstrcpy(fileName, "..\\");
		char* p = fileName + lstrlen(fileName);
	
		// get currently selected item and its text
		int idx = (int)SendMessage(listControl, CB_GETCURSEL, 0, 0);
		if (idx == 0) {
			SendMessage(infoControl, WM_SETTEXT, (WPARAM)0, 
					(LPARAM)"\0");
			continue;
		}
		SendMessage(listControl, CB_GETLBTEXT, idx, (LPARAM)p);
	
		// check if it's a recognizable EXE-file
		if (GetGameVersion(fileName) == -1)
		{
			SendMessage(infoControl, WM_SETTEXT, (WPARAM)0, 
					(LPARAM)"Unknown EXE-file, this is not a valid executable "
					"of Pro Evolution Soccer 7!\0");
			continue;
		}
		
		if (isGame(GetGameVersion(fileName)) != (i==0)) {
			if (i==0) {
				SendMessage(infoControl, WM_SETTEXT, (WPARAM)0, 
					(LPARAM)"You selected a settings executable instead a a game "
						"executable.\0");
			} else {
				SendMessage(infoControl, WM_SETTEXT, (WPARAM)0, 
					(LPARAM)"You selected a game executable instead a a settings "
						"executable.\0");
			}
			continue;
		}
		
		isValidExe[i] = true;
	
		FILE* f = fopen(fileName, "rb");
		if (f != NULL)
		{
			IMAGE_SECTION_HEADER dataHeader;
			ZeroMemory(&dataHeader, sizeof(IMAGE_SECTION_HEADER));
			DWORD dataOffset = 0;
			DWORD dataVA = 0;
	
			// find empty space at the end of .rdata
			if (SeekSectionHeader(f, ".rdata")) {
				fread(&dataHeader, sizeof(IMAGE_SECTION_HEADER), 1, f);
	
				dataVA = dataHeader.VirtualAddress + dataHeader.SizeOfRawData;
				if (dataHeader.PointerToRawData != 0)
					dataOffset = dataHeader.PointerToRawData + dataHeader.SizeOfRawData;
				else
					dataOffset = dataHeader.VirtualAddress + dataHeader.SizeOfRawData;
	
				// shift 32 bytes back
				dataOffset -= 0x20;
				dataVA -= 0x20;
			}
	
			MyMessageBox("dataOffset = %08x", dataOffset);
			MyMessageBox("dataVA = %08x", dataVA);
	
			if (dataOffset != 0) {
				// if installed this should have some data
				BYTE zero[0x20];
				ZeroMemory(zero, 0x20);
				fseek(f, dataOffset, SEEK_SET);
	
				// read saved old entry point
				DWORD savedEntryPoint = 0;
				fread(&savedEntryPoint, sizeof(DWORD), 1, f);
	
				// read kitserver DLL name
				char buf[0x18];
				ZeroMemory(buf, 0x18);
				fseek(f, sizeof(DWORD), SEEK_CUR);
				fread(buf, 0x18, 1, f);
				char* dllFilename=strrchr(buf,'\\')+1;
				isInstalled[i] = false;
				
				if (savedEntryPoint != 0 && dllFilename != (char*)1 &&
						lstrcmp(dllFilename, (i==0)?DLL_NAME:DLL_NAME_SET) == 0) {
					isInstalled[i] = true;
				}
				
				if (isInstalled[i])
				{
					char exeFolderName[BUFLEN];
					ZeroMemory(exeFolderName,BUFLEN);
					strncpy(exeFolderName,buf,dllFilename-buf-1);
	
					if (lstrcmp(exeFolderName, patchFolderName) == 0) {
						SendMessage(infoControl, WM_SETTEXT, (WPARAM)0, 
								(LPARAM)"KitServer INSTALLED correctly for this folder.\0");
								
						EnableWindow(g_installButtonControl, FALSE);
						EnableWindow(g_removeButtonControl, TRUE);
					}
					else
					{
						char temp[BUFLEN];
						ZeroMemory(temp,BUFLEN);
						sprintf(temp, "KitServer INSTALLED, but for folder \"%s\".\0", exeFolderName);
						
						SendMessage(infoControl, WM_SETTEXT, (WPARAM)0, 
								(LPARAM)temp);
						EnableWindow(g_installButtonControl, FALSE);
						EnableWindow(g_removeButtonControl, TRUE);
					};
				}
				else
				{
					SendMessage(infoControl, WM_SETTEXT, (WPARAM)0, 
							(LPARAM)"KitServer not installed.\0");
					EnableWindow(g_installButtonControl, TRUE);
					EnableWindow(g_removeButtonControl, FALSE);
	
				}
			}
			else
			{
				SendMessage(infoControl, WM_SETTEXT, (WPARAM)0,
						(LPARAM)"Information unavailable\0");
			}
			fclose(f);
			}
			else
			{
				SendMessage(infoControl, WM_SETTEXT, (WPARAM)0, 
						(LPARAM)"[ERROR: Can't open file.]\0");
			}
	}
	
	if (isValidExe[0] || isValidExe[1]) {
		if (isInstalled[0] || isInstalled[1])
			EnableWindow(g_removeButtonControl, true);
		
		if (!isInstalled[0] || !isInstalled[1])
			EnableWindow(g_installButtonControl, true);
		
	}
}

/**
 * Initializes all controls
 */
void InitControls(void)
{
	// Build the drop-down list
	WIN32_FIND_DATA fData;
	char pattern[4096];
	ZeroMemory(pattern, 4096);

	lstrcpy(pattern, "..\\*.exe");

    int count = 1, selectedIndex = 0;
    int count1 = 1, selectedIndex1 = 0;
	HANDLE hff = FindFirstFile(pattern, &fData);
	if (hff == INVALID_HANDLE_VALUE) 
	{
		// none found.
		g_noFiles = true;
		return;
	}
	
	SendMessage(g_exeListControl, CB_ADDSTRING, (WPARAM)0, (LPARAM)"- no action -");
	SendMessage(g_exeListControl, WM_SETTEXT, (WPARAM)0, (LPARAM)"- no action -");
	SendMessage(g_setListControl, CB_ADDSTRING, (WPARAM)0, (LPARAM)"- no action -");
	SendMessage(g_setListControl, WM_SETTEXT, (WPARAM)0, (LPARAM)"- no action -");
	
	while(true)
	{
		if (lstrcmpi(fData.cFileName, "settings.exe") != 0) // skip settings.exe
		{
			SendMessage(g_exeListControl, CB_ADDSTRING, (WPARAM)0, (LPARAM)fData.cFileName);
			SendMessage(g_exeListControl, WM_SETTEXT, (WPARAM)0, (LPARAM)fData.cFileName);
			count++; //only count the listed exe files
		}
		
		if (lstrcmpi(fData.cFileName, patchExeName) != 0) // skip default game exe
		{
			SendMessage(g_setListControl, CB_ADDSTRING, (WPARAM)0, (LPARAM)fData.cFileName);
			SendMessage(g_setListControl, WM_SETTEXT, (WPARAM)0, (LPARAM)fData.cFileName);
			count1++;
		}
		
        if (lstrcmpi(fData.cFileName, patchExeName) == 0) { // auto-select
            selectedIndex = count - 1;
        }
        
        if (lstrcmpi(fData.cFileName, "settings.exe") == 0) { // auto-select settings.exe
            selectedIndex1 = count1 - 1;
        }

		// proceed to next file
		if (!FindNextFile(hff, &fData)) break;
	}
	FindClose(hff);

    /*
	// populate OS choices box
	char* osChoices[] = {"<auto-detect>", "Windows XP", "Windows 2000"};
	for (int i=0; i<3; i++)
	{
		SendMessage(g_osListControl, CB_ADDSTRING, (WPARAM)0, (LPARAM)osChoices[i]);
		SendMessage(g_osListControl, WM_SETTEXT, (WPARAM)0, (LPARAM)osChoices[i]);
	}
    */

	SendMessage(g_exeListControl, CB_SETCURSEL, (WPARAM)selectedIndex, (LPARAM)0);
	EnableWindow(g_exeListControl, TRUE);

	SendMessage(g_setListControl, CB_SETCURSEL, (WPARAM)selectedIndex1, (LPARAM)0);
	EnableWindow(g_setListControl, TRUE);

    /*
	SendMessage(g_osListControl, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
	if (g_advancedMode)
		EnableWindow(g_osListControl, TRUE);
    */
}

void ShowHelpText()
{
	char buf[BUFLEN];
	strcpy(buf,
	"The most common mistake when trying to install Kitserver is that\n"
	"you didn't extract all the files from the zip file to your installation\n"
	"folder of PES2008 but try to start the setup.exe from\n"
	"elsewhere. This is NO usual installer like other programms have.\n"
	"You have to extract the files manually.\n"
	"\n"
	"For more information on what Kitserver is, what you can do with\n"
	"it and how you install it correctly, please read the manual.html\n"
	"which is located in the in the \"docs\" subfolder.\n"
	"\n"
	"Do you want to open the manual now?"
	);
	
	if (MessageBox(hWnd, buf, "KitServer 7 short help", MB_YESNO) == IDYES) {
		ShellExecute(hWnd, "open", ".\\docs\\manual.html", NULL, NULL, SW_SHOWNORMAL);
	}
	
	return;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int home, away, timecode;
	char buf[BUFLEN];

	switch(uMsg)
	{
		case WM_DESTROY:
			// Exit the application when the window closes
			PostQuitMessage(1);
			return true;

		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				if ((HWND)lParam == g_installButtonControl)
				{
					InstallKserv();
				}
				else if ((HWND)lParam == g_removeButtonControl)
				{
					RemoveKserv();
				}
				else if ((HWND)lParam == g_helpButtonControl)
				{
					ShowHelpText();
				}
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				if ((HWND)lParam == g_exeListControl || (HWND)lParam == g_setListControl)
				{
					UpdateInfo();
				}
			}
			break;
	}
	return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

bool InitApp(HINSTANCE hInstance, LPSTR lpCmdLine)
{
	// set advanced mode flag
	g_advancedMode = (lstrcmp(lpCmdLine, "-os")==0);

	WNDCLASSEX wcx;

	// cbSize - the size of the structure.
	wcx.cbSize = sizeof(WNDCLASSEX);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = (WNDPROC)WindowProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = hInstance;
	wcx.hIcon = LoadIcon(NULL,IDI_APPLICATION);
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = "SETUPCLS";
	wcx.hIconSm = NULL;

	// Register the class with Windows
	if(!RegisterClassEx(&wcx))
		return false;

	return true;
}

HWND BuildWindow(int nCmdShow)
{
	DWORD style, xstyle;
	HWND retval;

	style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	xstyle = WS_EX_LEFT;

	retval = CreateWindowEx(xstyle,
        "SETUPCLS",      // class name
        patchTitle, // title for our window (appears in the titlebar)
        style,
        CW_USEDEFAULT,  // initial x coordinate
        CW_USEDEFAULT,  // initial y coordinate
        WIN_WIDTH, WIN_HEIGHT,   // width and height of the window
        NULL,           // no parent window.
        NULL,           // no menu
        NULL,           // no creator
        NULL);          // no extra data

	if (retval == NULL) return NULL;  // BAD.

	ShowWindow(retval,nCmdShow);  // Show the window
	return retval; // return its handle for future use.
}

void ReadPatchInfo()
{
	char patchFile[BUFLEN];
	sprintf(patchFile,"%s\\patch.cfg",mydir);
	
	FILE* cfg = fopen(patchFile, "rt");
	if (cfg == NULL) return;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (!feof(cfg))
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (lstrcmp(name, "Title")==0)
		{
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;

			strcat(patchTitle, " - \0");
            memcpy(patchTitle + lstrlen(patchTitle), startQuote + 1, endQuote - startQuote - 1);
		}
		else if (lstrcmp(name, "ExeFilename")==0)
		{
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;
				
            memcpy(patchExeName, startQuote + 1, endQuote - startQuote - 1);
		}
	}
	fclose(cfg);

	return;
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MSG msg; int retval;

 	if(InitApp(hInstance, lpCmdLine) == false)
		return 0;
		
	//detect the folder setup is executed from
	char temp[BUFLEN];
	ZeroMemory(temp, BUFLEN);
	GetModuleFileName(hInstance, temp, BUFLEN);
	char *q = temp + lstrlen(temp);
	while ((q != mydir) && (*q != '\\')) { *q = '\0'; q--; }
	*q = '\0';
	strcpy(mydir, temp);
	while ((q != temp) && (*q != '\\')) { q--; }
	strcpy(patchFolderName, q+1);
	
	sprintf(installDllPath,"%s\\%s", patchFolderName, DLL_NAME);
	sprintf(installDllSetPath,"%s\\%s", patchFolderName, DLL_NAME_SET);
	
	ZeroMemory(patchTitle, BUFLEN);
	strcpy(patchTitle, SETUP_WINDOW_TITLE);
	ZeroMemory(patchExeName, BUFLEN);
	strcpy(patchExeName, DEFAULT_EXE_NAME);
	
	//Look for information of a patch
	ReadPatchInfo();

	hWnd = BuildWindow(nCmdShow);
	if(hWnd == NULL)
		return 0;

	// build GUI
	if (!BuildControls(hWnd))
		return 0;

	// Initialize all controls
	InitControls();
	UpdateInfo();

	// show credits
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	strncpy(buf, CREDITS, BUFLEN-1);
	SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)buf);

	while((retval = GetMessage(&msg,NULL,0,0)) != 0)
	{
		if(retval == -1)
			return 0;	// an error occured while getting a message

		if (!IsDialogMessage(hWnd, &msg)) // need to call this to make WS_TABSTOP work
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

