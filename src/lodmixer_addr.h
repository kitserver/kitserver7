// ADDRESSES for lodmixer.cpp
BYTE allowedGames[] = {
	gvPES2008,
	gvPES2008v110,
	gvPES2008v120,
};

#define CODELEN 2
enum {
	C_SETTINGS_CHECK, C_MODE_CHECK,
};

#define NOCODEADDR {0,0},
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR
	// [Settings] PES2008 PC DEMO
    NOCODEADDR
    // PES2008
	{
        0xbb8be3, 0xaa9542,
	},
	// [Settings] PES2008 PC
    NOCODEADDR
    NOCODEADDR
    // PES2008 1.10
	{
        0xbb89d3, 0xaaa5e2,
	},
    NOCODEADDR
    // PES2008 1.20
    {
        0xbbd083, 0xaacbc2,
    },
};

#define DATALEN 7
enum {
    SCREEN_WIDTH, SCREEN_HEIGHT, WIDESCREEN_FLAG,
    RATIO_4on3, RATIO_16on9,
    LOD_SWITCH1, LOD_SWITCH2,
};

#define NODATAADDR {0,0,0,0,0,0,0},
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
	NODATAADDR
	// [Settings] PES2008 PC DEMO
	NODATAADDR
    // PES2008
    {
        0x12509c8, 0x12509cc,0x12509d0,
        0xdb43dc, 0xdb4754,
        0xdb1fe4, 0xdb1fe0,
    },
	// [Settings] PES2008 PC
	NODATAADDR
	NODATAADDR
    // PES2008 1.10
    {
        0x12519f0, 0x12519f4,0x12519f8,
        0xdb5484, 0xdb57fc,
        0xdb3044, 0xdb3040,
    },
    NODATAADDR
    // PES2008 1.20
    {
        0x1252a08, 0x1252a0c,0x1252a10, 
        0xdb6434, 0xdb67b0,
        0xdb4024, 0xdb4084,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
