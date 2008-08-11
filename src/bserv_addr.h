// ADDRESSES for stadium.cpp
BYTE allowedGames[] = {
    //gvPES2008demo,
    gvPES2008,
    gvPES2008v110,
    gvPES2008v120,
};

#define CODELEN 3
enum { C_GET_BALL_NAME1, C_GET_BALL_NAME2, C_GET_BALL_NAME3 };

#define NOCODEADDR {0,0,0}
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR,
    // [Settings] PES2008 PC DEMO
    NOCODEADDR,
    // PES2008
    {
        0xa57703, 0xaba0da, 0xaba1a1,
    },
    // [Settings] PES2008 PC
    NOCODEADDR,
    NOCODEADDR,
    // PES2008 1.10
    {
        0xa576e3, 0xaba30a, 0xaba3d1,
    },
    NOCODEADDR,
    // PES2008 1.20
    {
        0xa59853, 0xab77ca, 0xab7891,
    },
};

#define DATALEN 5 
enum {
    BIN_SIZES_TABLE, SONGS_INFO_TABLE, BALLS_INFO_TABLE,
    STADIUM_NAMES_TABLE, NEXT_MATCH_DATA_PTR,
};

#define NODATAADDR {0,0,0,0,0},
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
    NODATAADDR
    // [Settings] PES2008 PC DEMO
    NODATAADDR
    // PES2008
    { 
        0x1316b60, 0xd7ab78, 0xd15268,
        0xffab20, 0x1250d4c,
    },
    // [Settings] PES2008 PC
    NODATAADDR
    NODATAADDR
    // PES2008 1.10
    { 
        0x1317b80, 0xd7bb80, 0xd16258,
        0xffbb20, 0x1251d74,
    },
    NODATAADDR
    // PES2008 1.20
    {
        0x1318b80, 0xd7cc78, 0xd17240,
        0xffcb58, 0x1252d70,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
