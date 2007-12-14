// ADDRESSES for kserv.cpp
BYTE allowedGames[] = {
	gvPES2008demo,
	gvPES2008,
	gvPES2008v110,
	gvPES2008v120,
};

#define CODELEN 8
enum {
	C_CONTROLLERFIX1, C_CONTROLLERFIX2, C_QUALITYCHECK1, C_QUALITYCHECK2,
    C_SETFILEPOINTEREX, C_UNPACK_BIN, 
    C_AFTER_READTEAMKITINFO_1, C_AFTER_READTEAMKITINFO_2,
};

#define NOCODEADDR {0,0,0,0,0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
	{
		0x86dd12, 0x86dd22, 0x904b0f, 0x904bc5, 0, 0,
        0, 0,
	},
	// [Settings] PES2008 PC DEMO
  NOCODEADDR
  // PES2008
	{
		0, 0, 0xbb6adf, 0xbb6b95, 0x4962e7, 0xc8d911,
        0, 0,
	},
	// [Settings] PES2008 PC
  NOCODEADDR
  NOCODEADDR
  // PES2008 1.10
	{
		0, 0, 0xbb68cf, 0xbb6985, 0x496c37, 0xc8f3e1,
        0, 0,
	},
  NOCODEADDR
  // PES2008 1.20
	{
		0, 0, 0xbbaf9d, 0xbbb053, 0x496bc7, 0xc907f1, 
        0xc96a80, 0xc96585,
	},
};

#define DATALEN 2
enum {
	NUMNOPS_1, NUMNOPS_2,	
};

#define NODATAADDR {0,0},
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
	NODATAADDR
	// [Settings] PES2008 PC DEMO
	NODATAADDR
  // PES2008
    { 6, 2, },
	// [Settings] PES2008 PC
	NODATAADDR
	NODATAADDR
    // PES2008 1.10
    { 6, 2, },
    { 6, 2, },
    // PES2008 1.20
    { 6, 2, },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
