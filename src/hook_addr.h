// ADDRESSES for hook.cpp
#define CODELEN 7
enum {
	C_D3DCREATE_CS,
	C_LOADTEXTUREFORPLAYER_CS, C_LOADTEXTUREFORPLAYER,
	TBL_BEGINRENDER1,	TBL_BEGINRENDER2,
	C_EDITCOPYPLAYERNAME_CS, C_COPYSTRING_CS,
};

#define NOCODEADDR {0,0,0,0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
	{
		0x901ab1,
		0x435f28, 0x83c3e0,
		0xa84a9c, 0xa7decc,
		0, 0,	//TODO
	},
	// [Settings] PES2008 PC DEMO
  NOCODEADDR
  // PES2008
  {
		0xbb3841,
		0x43e578, 0xa72ad0,
		0xdad50c, 0xd98214,
		0x8422c0, 0x401d1a,
	},
};

#define DATALEN 2
enum {
	PLAYERDATA, ISREFEREEADDR,
};

#define NODATAADDR {0,0},
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
	{
		0xd6dcd4, 0xa86470,
	},
	// [Settings] PES2008 PC DEMO
	NODATAADDR
	// PES2008 PC
	{
		0x125934c, 0xdaf200,
	}
};

#define LTFPLEN 15
#define NOLTFPADDR {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
BYTE ltfpPatchArray[][LTFPLEN] = {
	// PES2008 DEMO
	{
		0x8b, 0xf8,														// MOV EDI, EAX
		0x8b, 0x44, 0x24, 0x38, 							// MOV EAX,DWORD PTR SS:[ESP+38]
		0x83, 0xf8, 0x04,											// CMP EAX,4
		0x0f, 0x87, 0x1f, 0x06, 0x00, 0x00,		// JA 0043655B
	},
	// [Settings] PES2008 PC DEMO
	NOLTFPADDR
	// PES2008 PC
	{
		0x8b, 0xf8,														// MOV EDI, EAX
		0x8b, 0x44, 0x24, 0x38, 							// MOV EAX,DWORD PTR SS:[ESP+38]
		0x83, 0xf8, 0x04,											// CMP EAX,4
		0x0f, 0x87, 0x1f, 0x06, 0x00, 0x00,		// JA 0043655B
	},
};


DWORD code[CODELEN];
DWORD data[DATALEN];
BYTE ltfpPatch[LTFPLEN];
