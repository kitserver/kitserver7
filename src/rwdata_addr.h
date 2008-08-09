// ADDRESSES for rwdata module
BYTE allowedGames[] = {
    //gvPES2008demo,
    gvPES2008,
    gvPES2008v110,
    gvPES2008v120,
};

#define CODELEN 4
enum { 
    C_CREATEFILEW_R, C_CREATEFILEW_W,
    C_CALC_STRING_SIZE, C_SET_DIRNAME,
};

#define NOCODEADDR {0,0,0,0}
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR,
    // [Settings] PES2008 PC DEMO
    NOCODEADDR,
    // PES2008
    {
        0xa841d7, 0xa83d4b,
        0xa846df, 0xa84702,
    },
    // [Settings] PES2008 PC
    NOCODEADDR,
    NOCODEADDR,
    // PES2008 1.10
    {
        0xa83a17, 0xa8358b,
        0xa83f1f, 0xa83f42,
    },
    NOCODEADDR,
    // PES2008 1.20
    { 
        0xa85e17, 0xa8598b,
        0xa8631f, 0xa86342,
    },
};

#define DATALEN 1 
enum {
    DUMMY
};

#define NODATAADDR {0}
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
    NODATAADDR,
    // [Settings] PES2008 PC DEMO
    NODATAADDR,
    // PES2008
    NODATAADDR,
    // [Settings] PES2008 PC
    NODATAADDR,
    NODATAADDR,
    // PES2008 1.10
    NODATAADDR,
    NODATAADDR,
    // PES2008 1.20
    NODATAADDR,
};

DWORD code[CODELEN];
DWORD data[DATALEN];
