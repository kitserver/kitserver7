// ADDRESSES for fserv.cpp
BYTE allowedGames[] = {
    //gvPES2008demo,
    //gvPES2008,
    //gvPES2008v110,
    gvPES2008v120,
};

#define CODELEN 2
enum { 
    C_CHECK_FACE_AND_HAIR_ID, C_COPY_DATA,
};

#define NOCODEADDR {0,0}
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR,
    // [Settings] PES2008 PC DEMO
    NOCODEADDR,
    // PES2008
    NOCODEADDR,
    // [Settings] PES2008 PC
    NOCODEADDR,
    NOCODEADDR,
    // PES2008 1.10
    NOCODEADDR,
    NOCODEADDR,
    // PES2008 1.20
    { 
        0xb734cf, 0xc99b28,
    },
};

#define DATALEN 2 
enum {
    EDIT_DATA_PTR, BIN_SIZES_TABLE,
};

#define NODATAADDR {0,0},
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
    NODATAADDR
    // [Settings] PES2008 PC DEMO
    NODATAADDR
    // PES2008
    { 
        0, 0,
    },
    // [Settings] PES2008 PC
    NODATAADDR
    NODATAADDR
    // PES2008 1.10
    { 
        0, 0,
    },
    NODATAADDR
    // PES2008 1.20
    {
        0x1252d68, 0x1318b80,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
