// ADDRESSES for kserv.cpp
BYTE allowedGames[] = {
	//gvPES2008demo,
	gvPES2008,
	gvPES2008v110,
	gvPES2008v120,
};

#define CODELEN 23
enum {
	C_CONTROLLERFIX1, C_CONTROLLERFIX2, C_QUALITYCHECK1, C_QUALITYCHECK2,
    C_SETFILEPOINTEREX, C_UNPACK_BIN, C_AFTER_UNPACK_BIN, 
    C_AFTER_READTEAMKITINFO_1, C_AFTER_READTEAMKITINFO_2,
    C_SUB_MENUMODE, C_ADD_MENUMODE, C_START_READKITINFO, C_START_READKITINFO_2, 
    C_START_READKITINFO_3, C_END_READKITINFO, C_AFTER_READTEAMKITINFO_3, 
    C_ON_LEAVE_CUPS, C_ON_LEAVE_CUPS_2, C_ON_ENTER_CUPS,
    C_BEFORE_LOAD_BIN,
    C_AT_READ_KIT_CHOICE, C_AT_READ_KIT_CHOICE_2,
    C_PTR_CHECK,
};

#define NOCODEADDR {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    {
        0x86dd12, 0x86dd22, 0x904b0f, 0x904bc5, 
        0, 0, 0,
        0, 0, 
        0, 0, 0, 0, 
        0, 0, 0, 
        0, 0, 0,
        0,
        0, 0,
        0,
    },
    // [Settings] PES2008 PC DEMO
    NOCODEADDR
    // PES2008
    {
        0, 0, 0xbb6adf, 0xbb6b95, 
        0x4962e7, 0xc8d911, 0xc8d916,
        0xc93970, 0xc93475, 
        0x427f80, 0xb2ce3d, 0xb9c963, 0xb9c552, 
        0xabf0f8, 0xb9c7ff, 0xc93057, 
        0xc5ee6c, 0x90dd64, 0x90ddc3,
        0xa883c6,
        0x9b30fa, 0x84b4bc,
        0x46b4ab,
    },
    // [Settings] PES2008 PC
    NOCODEADDR
    NOCODEADDR
    // PES2008 1.10
    {
        0, 0, 0xbb68cf, 0xbb6985, 
        0x496c37, 0xc8f3e1, 0xc8f3e6,
        0xc95440, 0xc94f45, 
        0x427e20, 0xb2cc5d, 0xb9c783, 0xb9c372, 
        0xac09a8, 0xb9c61f, 0xc94b27, 
        0xc5d1dc, 0x90b024, 0x90b083,
        0xa87fbe,
        0x9b300a, 0x84bdcc,
        0x46c08b,
    },
    NOCODEADDR
    // PES2008 1.20
    {
        0, 0, 0xbbaf9d, 0xbbb053, 
        0x496bc7, 0xc907f1, 0xc907f6, 
        0xc96a80, 0xc96585, 
        0x427a20, 0xb32d0d, 0xba3b03, 0xba36f2, 
        0xac69d8, 0xba399f, 0xc96167, 
        0xc6151c, 0x90cfe4, 0x90d043,
        0xa8a664,
        0x9b504a, 0x84deac,
        0x46c2db,
    },
};

#define DATALEN 9 
enum {
	NUMNOPS_1, NUMNOPS_2, TEAM_KIT_INFO_BASE,
    MENU_MODE_IDX, MAIN_SCREEN_INDICATOR, INGAME_INDICATOR,
    NEXT_MATCH_DATA_PTR, CUP_MODE_PTR,
    BIN_SIZES_TABLE,
};

#define NODATAADDR {0,0,0,0,0,0,0,0,0},
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
    NODATAADDR
    // [Settings] PES2008 PC DEMO
    NODATAADDR
    // PES2008
    { 
        6, 2, 0x1250d44, 
        0x1250fc8, 0x1250fb8, 0x1250e08, 
        0x1250d4c, 0x1251ec8, 
        0x1316b60,
    },
    // [Settings] PES2008 PC
    NODATAADDR
    NODATAADDR
    // PES2008 1.10
    { 
        6, 2, 0x1251d6c,  
        0x1251ff0, 0x1251fe0, 0x1251e30,
        0x1251d74, 0x1252ef0, 
        0x1317b80,
    },
    NODATAADDR
    // PES2008 1.20
    { 
        6, 2, 0x1252d68,
        0x1252ff0, 0x1253010, 0x1252e48,
        0x1252d70, 0x1253f08,
        0x1318b80,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
