// ADDRESSES for hook.cpp
#define CODELEN 4
enum {
	C_D3DCREATE_CS,
	C_ENDRENDERPLAYERS_CS,
	C_LOADTEXTUREFORPLAYER_CS, C_LOADTEXTUREFORPLAYER,
};

#define NOCODEADDR {0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
	{
		0x901ab1,
		0x88a852,
		0x435f28, 0x83c3e0,
	},
	// [Settings] PES2008 PC DEMO
  NOCODEADDR
  // PES2008
  NOCODEADDR
	// [Settings] PES2008 PC
  NOCODEADDR
  // PES2008 FLT-NODVD
  NOCODEADDR
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
	NODATAADDR
	// [Settings] PES2008 PC
	NODATAADDR
	// PES2008 PC FLT-NODVD
	NODATAADDR
};

#define LTFPLEN 15
#define NOLTFPADDR {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
BYTE ltfpPatchArray[][LTFPLEN] = {
	// PES2008 DEMO
	{
		0x8b, 0xf8,														// MOV EDI, EAX
		0x8b, 0x44, 0x24, 0x38, 							// MOV EAX,DWORD PTR SS:[ESP+38]
		0x83, 0xf8, 0x04,											// CMP EAX,4
		0x0f, 0x87, 0x1f, 0x06, 0x00, 0x00,		//JA 0043655B
	},
	// [Settings] PES2008 PC DEMO
	NOLTFPADDR
	// PES2008 PC
	NOLTFPADDR
	// [Settings] PES2008 PC
	NOLTFPADDR
	// PES2008 PC FLT-NODVD
	NOLTFPADDR
};


DWORD code[CODELEN];
DWORD data[DATALEN];
BYTE ltfpPatch[LTFPLEN];
