// ADDRESSES for lodmixer.cpp
BYTE allowedGames[] = {
	gvPES2008,
};

#define CODELEN 1
enum {
	C_SETTINGS_CHECK,
};

#define NOCODEADDR {0},
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR
	// [Settings] PES2008 PC DEMO
    NOCODEADDR
    // PES2008
	{
        0xbb8be3,
	},
	// [Settings] PES2008 PC
    NOCODEADDR
};

#define DATALEN 5
enum {
    SCREEN_WIDTH, SCREEN_HEIGHT, WIDESCREEN_FLAG,
    RATIO_4on3, RATIO_16on9,
};

#define NODATAADDR {0,0,0,0,0},
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
	NODATAADDR
	// [Settings] PES2008 PC DEMO
	NODATAADDR
    // PES2008
    {
        0x12509c8, 0x12509cc,0x12509d0,
        0xdb43dc, 0xdb4754,
    },
	// [Settings] PES2008 PC
	NODATAADDR
};

DWORD code[CODELEN];
DWORD data[DATALEN];
