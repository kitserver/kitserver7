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
	// [Settings] PES2008 PC
  NOCODEADDR
  NOCODEADDR
  // PES2008 1.10
  {
		0xbb3661,
		0x43e258, 0xa72930,
		0xdae53c, 0xd99224,
		0x842c10, 0x401d2a,
	},
  NOCODEADDR
  // PES2008 1.20
  {
		0xbb7d41,
		0, 0, //TODO
		0, 0, //TODO
		0, 0, //TODO
  },
};

#define DATALEN 4
enum {
	PLAYERDATA, ISREFEREEADDR,
	GENERALINFO, REPLAYGENERALINFO,
};

#define NODATAADDR {0,0,0,0},
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
	{
		0xd6dcd4, 0xa86470,
		0, 0, // TODO
	},
	// [Settings] PES2008 PC DEMO
	NODATAADDR
	// PES2008 PC
	{
		0x125934c, 0xdaf200,
		0x12544c4, 0x1251e04,
	},
    // [Settings] PES2008 PC
	NODATAADDR
	NODATAADDR
	// PES2008 PC 1.10
	{
		0x125a374, 0xdb0270,
		0x12554ec, 0x1252e2c,
	},
	NODATAADDR
	// PES2008 PC 1.20
	NODATAADDR //TODO
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
    // [Settings] PES2008 PC
	NOLTFPADDR
	NOLTFPADDR
	// PES2008 PC 1.10
	{
		0x8b, 0xf8,														// MOV EDI, EAX
		0x8b, 0x44, 0x24, 0x38, 							// MOV EAX,DWORD PTR SS:[ESP+38]
		0x83, 0xf8, 0x04,											// CMP EAX,4
		0x0f, 0x87, 0x1f, 0x06, 0x00, 0x00,		// JA 0043E88B
	},
	NOLTFPADDR
    // PES2008 PC 1.20
	NOLTFPADDR //TODO
};


DWORD code[CODELEN];
DWORD data[DATALEN];
BYTE ltfpPatch[LTFPLEN];
