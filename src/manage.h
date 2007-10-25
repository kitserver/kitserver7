// manage.h
#ifndef _MANAGE_
#define _MANAGE_

typedef struct _KMOD {
	DWORD id;
	wchar_t nameLong[BUFLEN];
	wchar_t nameShort[BUFLEN];
	DWORD debug;
} KMOD;

typedef struct _PESINFO {
	wchar_t myDir[BUFLEN];
	wchar_t processFile[BUFLEN];
	wchar_t shortProcessFile[BUFLEN];
	wchar_t shortProcessFileNoExt[BUFLEN];
	wchar_t pesDir[BUFLEN];
	wchar_t gdbDir[BUFLEN];
	wchar_t logName[BUFLEN];
	int gameVersion;
	wchar_t lang[32];
	HANDLE hProc;
	UINT bbWidth;
	UINT bbHeight;
	double stretchX;
	double stretchY;
} PESINFO;

enum {
	gvPES2008demo,		// PES2008 PC DEMO
	gvPES2008demoSet,	// PES2008 PC DEMO (Settings)
	gvPES2008,		    // PES2008 PC
	gvPES2008Set,	    // PES2008 PC (Settings)
	gvPES2008fltNODVD,  // PES2008 PC FLT-NODVD
};

#endif
