// ADDRESSES for kserv.cpp
BYTE allowedGames[] = {
	//gvPES2008demo,
	//gvPES2008,
	//gvPES2008v110,
	gvPES2008v120,    //TODO addresses
};

#define CODELEN 18
enum {
	C_CONTROLLERFIX1, C_CONTROLLERFIX2, C_QUALITYCHECK1, C_QUALITYCHECK2,
    C_SETFILEPOINTEREX, C_UNPACK_BIN, 
    C_AFTER_READTEAMKITINFO_1, C_AFTER_READTEAMKITINFO_2,
    C_SUB_MENUMODE, C_ADD_MENUMODE, C_START_READKITINFO, C_START_READKITINFO_2, 
    C_START_READKITINFO_3, C_END_READKITINFO, C_AFTER_READTEAMKITINFO_3, 
    C_ON_LEAVE_CUPS, C_ON_LEAVE_CUPS_2, C_ON_ENTER_CUPS,
};

#define NOCODEADDR {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
	{
		0x86dd12, 0x86dd22, 0x904b0f, 0x904bc5, 0, 0,
        0, 0, 
        0, 0, 0, 0, 
        0, 0, 0, 
        0, 0, 0,
	},
	// [Settings] PES2008 PC DEMO
  NOCODEADDR
  // PES2008
	{
		0, 0, 0xbb6adf, 0xbb6b95, 0x4962e7, 0xc8d911,
        0, 0, 
        0, 0, 0, 0, 
        0, 0, 0, 
        0, 0, 0,
	},
	// [Settings] PES2008 PC
  NOCODEADDR
  NOCODEADDR
  // PES2008 1.10
	{
		0, 0, 0xbb68cf, 0xbb6985, 0x496c37, 0xc8f3e1,
        0, 0, 
        0, 0, 0, 0, 
        0, 0, 0, 
        0, 0, 0,
	},
  NOCODEADDR
  // PES2008 1.20
	{
		0, 0, 0xbbaf9d, 0xbbb053, 0x496bc7, 0xc907f1, 
        0xc96a80, 0xc96585, 
        0x427a20, 0xb32d0d, 0xba3b03, 0xba36f2, 
        0xac69d8, 0xba399f, 0xc96167, 
        0xc6151c, 0x90cfe4, 0x90d043,
	},
};

#define DATALEN 9 
enum {
	NUMNOPS_1, NUMNOPS_2, TEAM_KIT_INFO_BASE, ML_POINTER,
    MENU_MODE_IDX, MAIN_SCREEN_INDICATOR, INGAME_INDICATOR,
    NEXT_MATCH_DATA_PTR, CUP_MODE_PTR,
};

#define NODATAADDR {0,0,0,0,0,0,0,0,0},
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
	NODATAADDR
	// [Settings] PES2008 PC DEMO
	NODATAADDR
  // PES2008
    { 6, 2, 0, 
      0, 0, 0, 0,
      0, 0, },
	// [Settings] PES2008 PC
	NODATAADDR
	NODATAADDR
    // PES2008 1.10
    { 6, 2, 0, 0, 
      0, 0, 0,
      0, 0, },
    { 6, 2, 0, 0, 
      0, 0, 0,
      0, 0, },
    // PES2008 1.20
    { 6, 2, 0x1252d68, 0x1252d70, 
      0x1252ff0, 0x1253010, 0x1252df8,
      0x1252d70, 0x1253f08 },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
