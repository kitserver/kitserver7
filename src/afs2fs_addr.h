// ADDRESSES for afs2fs.cpp
BYTE allowedGames[] = {
    //gvPES2008demo,
    gvPES2008,
    gvPES2008v110,
    gvPES2008v120,
};

#define CODELEN 2
enum {
    C_AFTER_GET_BINBUFFERSIZE, C_PROCESS_BIN,
};

#define NOCODEADDR {0,0},
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    {
        0, 0,
    },
    // [Settings] PES2008 PC DEMO
    NOCODEADDR
    // PES2008
    {
        0xa882fb, 0xa8851c,
    },
    // [Settings] PES2008 PC
    NOCODEADDR
    NOCODEADDR
    // PES2008 1.10
    {
        0xa87efa, 0xa88115,
    },
    NOCODEADDR
    // PES2008 1.20
    {
        0xa8a5a0, 0xa8a7ba,
    },
};

#define DATALEN 1 
enum {
    BIN_SIZES_TABLE,
};

#define NODATAADDR {0},
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
    NODATAADDR
    // [Settings] PES2008 PC DEMO
    NODATAADDR
    // PES2008
    { 
        0x1316b60,
    },
    // [Settings] PES2008 PC
    NODATAADDR
    NODATAADDR
    // PES2008 1.10
    { 
        0x1317b80,
    },
    NODATAADDR
    // PES2008 1.20
    {
        0x1318b80,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
