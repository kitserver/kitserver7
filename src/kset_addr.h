// ADDRESSES for kset.cpp
BYTE allowedGames[] = {
	gvPES2008demoSet,
};

#define CODELEN 4
enum {
	C_CONTROLLERADDED, C_BEFORECONTROLLERADD, C_SOMEFUNCTION, C_QUALITYCHECK, 
};

#define NOCODEADDR {0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
  NOCODEADDR
	// [Settings] PES2008 PC DEMO
	{
		0x4049c9, 0x4048be, 0x415aaf, 0x4159c0,
	}
};

#define DATALEN 1
enum {
	CONTROLLER_NUMBER,	
};

#define NODATAADDR {0},
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
  NODATAADDR
	// [Settings] PES2008 PC DEMO
	{
		0x45a6a2,
	}
};

DWORD code[CODELEN];
DWORD data[DATALEN];
int gameVersion;