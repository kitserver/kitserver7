// ADDRESSES for hook.cpp
#define CODELEN 13
enum {
	C_D3DCREATE_CS,
	C_LOADTEXTUREFORPLAYER_CS, C_LOADTEXTUREFORPLAYER,
	TBL_BEGINRENDER1,	TBL_BEGINRENDER2,
	C_EDITCOPYPLAYERNAME_CS, C_COPYSTRING_CS,
    C_SUB_MENUMODE, C_ADD_MENUMODE,
    C_READ_FILE, C_WRITE_FILE,
    C_COPY_DATA, C_COPY_DATA2,
};

#define NOCODEADDR {0,0,0,0,0,0,0,0,0,0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
  {
		0x901ab1,
		0x435f28, 0x83c3e0,
		0xa84a9c, 0xa7decc,
		0, 0,	//TODO
        0, 0,
        0, 0,
        0, 0,
  },
  // [Settings] PES2008 PC DEMO
  NOCODEADDR
  // PES2008
  {
		0xbb3841,
		0x43e578, 0xa72ad0,
		0xdad50c, 0xd98214,
		0x8422c0, 0x401d1a,
        0x427f80, 0xb2ce3d,
        0xa84240, 0xa83ddc,
        0xc96248, 0x41433f,
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
        0x427e20, 0xb2cc5d,
        0xa83a80, 0xa8361c,
        0xc97bd8, 0x413e8f,
  },
  NOCODEADDR
  // PES2008 1.20
  {
		0xbb7d41,
		0, 0, //TODO
		0, 0, //TODO
		0, 0, //TODO
        0x427a20, 0xb32d0d,
        0xa85e80, 0xa85a1c, 
        0xc99b28, 0x414caf,
  },
};

#define DATALEN 10
enum {
	PLAYERDATA, ISREFEREEADDR,
	GENERALINFO, REPLAYGENERALINFO,
    MENU_MODE_IDX, MAIN_SCREEN_INDICATOR, INGAME_INDICATOR,
    NEXT_MATCH_DATA_PTR, CUP_MODE_PTR, EDIT_DATA_PTR,
};

#define NODATAADDR {0,0,0,0,0,0,0,0,0,0},
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
	{
		0xd6dcd4, 0xa86470,
		0, 0, // TODO
        0, 0, 0,
        0, 0, 0,
	},
	// [Settings] PES2008 PC DEMO
	NODATAADDR
	// PES2008 PC
	{
		0x125934c, 0xdaf200,
		0x12544c4, 0x1251e04,
        0x1250fc8, 0x1250fb8, 0x1250e08, 
        0x1250d4c, 0x1251ec8, 0x1250d44,
	},
    // [Settings] PES2008 PC
	NODATAADDR
	NODATAADDR
	// PES2008 PC 1.10
	{
		0x125a374, 0xdb0270,
		0x12554ec, 0x1252e2c,
        0x1251ff0, 0x1251fe0, 0x1251e30,
        0x1251d74, 0x1252ef0, 0x1251d6c,
	},
	NODATAADDR
	// PES2008 PC 1.20
    {
        0, 0,
        0, 0,
        0x1252ff0, 0x1253010, 0x1252e48,
        0x1252d70, 0x1253f08, 0x1252d68,
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
