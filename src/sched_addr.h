// ADDRESSES for lodmixer.cpp
BYTE allowedGames[] = {
	//gvPES2008,
	//gvPES2008v110,
	gvPES2008v120,
};

#define CODELEN 6
enum {
	C_NUM_GAMES_CHECK, D_SCHEDULE_STRUCT_PTR, C_WROTE_PLAYOFFS,
    C_CHECK_PLAYED, C_READ_NUM_GAMES, C_CHECK_ROUNDS,
};

#define NOCODEADDR {0, 0, 0, 0, 0, 0},
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR
	// [Settings] PES2008 PC DEMO
    NOCODEADDR
    // PES2008
	{
        0, 0, 0,
        0, 0, 0,
	},
	// [Settings] PES2008 PC
    NOCODEADDR
    NOCODEADDR
    // PES2008 1.10
	{
        0, 0, 0,
        0, 0, 0,
	},
    NOCODEADDR
    // PES2008 1.20
    {
        0xc59d6e, 0x1253f08, 0x901fc3,
        0x907b24, 0x858f40, 0x875657,  // 0x85904d
    },
};

#define DATALEN 215

#define NODATAADDR {0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0,\
    0,0,0,0,0},

DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
	NODATAADDR
	// [Settings] PES2008 PC DEMO
	NODATAADDR
    // PES2008
    NODATAADDR
	// [Settings] PES2008 PC
	NODATAADDR
	NODATAADDR
    // PES2008 1.10
	NODATAADDR
    NODATAADDR
    // PES2008 1.20
    {
        0x8572a7, 4, 1, 0, 0x0c,
        0x857314, 2, 2, 1, 0x178,
        0x857334, 2, 2, 1, 0x178,
        0x857371, 2, 2, 1, 0x178,
        0x857387, 2, 2, 1, 0x178,
        0x857398, 2, 2, 1, 0x178,
        0x8573a2, 2, 2, 1, 0x178,
        0x8573ad, 2, 2, 1, 0x178,
        0x8574fc, 1, 2, 0, 0x960,
        0x857506, 1, 2, 0, 0x960,
        0x857515, 1, 2, 0, 0x960,
        0x8578a9, 2, 2, 1, 0x178,
        0x857968, 3, 2, 1, 0x178,
        0x857a6d, 2, 2, 1, 0x178,
        0x857a85, 2, 2, 1, 0x178,
        0x857aa2, 3, 2, 1, 0x178,
        0x857ab0, 3, 2, 1, 0x178,
        0x857ac6, 2, 2, 1, 0x178,
        0x857ea2, 3, 2, 1, 0x178,
        0x858066, 2, 2, 1, 0x178,
        0x85809d, 2, 1, 0, 0x0c,
        0x85817d, 2, 1, 0, 0x0c,
        0x858190, 2, 2, 1, 0x178,
        0x8581a2, 3, 2, 1, 0x178,
        0x8581b8, 2, 2, 1, 0x178,
        0x858451, 3, 2, 1, 0x178,
        0x871a07, 2, 2, 1, 0x178,
        0x871a28, 2, 2, 1, 0x178,
        0x871a43, 3, 2, 1, 0x178,
        0x871ca1, 2, 2, 1, 0x178,
        0x871d09, 2, 2, 1, 0x178,
        0x871d13, 2, 2, 1, 0x178,
        0x871d1b, 2, 2, 1, 0x178,
        0x871d95, 2, 2, 1, 0x178,
        0x871dc9, 2, 2, 1, 0x178,
        0x871dcf, 3, 2, 1, 0x178,
        0x871de2, 2, 2, 1, 0x178,
        0x871ded, 2, 2, 1, 0x178,
        0x871eb0, 2, 2, 1, 0x178,
        0x871eb6, 2, 2, 1, 0x178,
        0x871ebc, 2, 2, 1, 0x178,
        0x872e47, 2, 2, 1, 0x178,
        0x87324b, 2, 2, 1, 0x178,
    },
};

enum {
    OP_REPLACE, OP_ADD, OP_SUB
};

DWORD code[CODELEN];
DWORD data[DATALEN];
