// ADDRESSES for kserv.cpp
BYTE allowedGames[] = {
	gvPES2008demo,
	gvPES2008,
	gvPES2008fltNODVD,
};

#define CODELEN 4
enum {
	C_CONTROLLERFIX1, C_CONTROLLERFIX2, C_QUALITYCHECK1, C_QUALITYCHECK2,
};

#define NOCODEADDR {0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
	{
		0x86dd12, 0x86dd22, 0x904b0f, 0x904bc5,
	},
	// [Settings] PES2008 PC DEMO
  NOCODEADDR
  // PES2008
	{
		0, 0, 0xbb6adf, 0xbb6b95,
	},
	// [Settings] PES2008 PC
  NOCODEADDR
  // PES2008 FLT-NODVD
	{
		0, 0, 0xbb6adf, 0xbb6b95,
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
  // PES2008 FLT-NODVD
	NODATAADDR
};

DWORD code[CODELEN];
DWORD data[DATALEN];