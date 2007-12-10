// ADDRESSES for kserv.cpp
BYTE allowedGames[] = {
	gvPES2008demo,
	gvPES2008,
	gvPES2008v110,
	gvPES2008v120,
};

#define CODELEN 6
enum {
	C_CONTROLLERFIX1, C_CONTROLLERFIX2, C_QUALITYCHECK1, C_QUALITYCHECK2,
    C_SETFILEPOINTEREX, C_UNPACK_BIN,
};

#define NOCODEADDR {0,0,0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
	{
		0x86dd12, 0x86dd22, 0x904b0f, 0x904bc5, 0, 0,
	},
	// [Settings] PES2008 PC DEMO
  NOCODEADDR
  // PES2008
	{
		0, 0, 0xbb6adf, 0xbb6b95, 0x4962e7, 0xc8d911,
	},
	// [Settings] PES2008 PC
  NOCODEADDR
  NOCODEADDR
  // PES2008 1.10
	{
		0, 0, 0xbb68cf, 0xbb6985, 0x496c37, 0xc8f3e1,
	},
  NOCODEADDR
  // PES2008 1.20
	{
		0, 0, 0xbbaf9d, 0xbbb053, 0x496bc7, 0xc907f1,
	},
};

#define DATALEN 1
enum {
	DUMMY,	
};

#define NODATAADDR {0},
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
	NODATAADDR
	// [Settings] PES2008 PC DEMO
	NODATAADDR
  // PES2008
	NODATAADDR
	// [Settings] PES2008 PC
	NODATAADDR
	NODATAADDR
    // PES2008 1.10
    NODATAADDR
	NODATAADDR
    // PES2008 1.20
    NODATAADDR
};

DWORD code[CODELEN];
DWORD data[DATALEN];
