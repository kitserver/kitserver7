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
	wchar_t gdbDir[BUFLEN];
	wchar_t logName[BUFLEN];
	int gameVersion;
} PESINFO;

enum {
	gvPES2008demo,		// PES2008 PC DEMO
	gvPES2008demoSet,	// PES2008 PC DEMO (Settings)
};

#endif