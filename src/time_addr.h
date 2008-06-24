// ADDRESSES for time module
BYTE allowedGames[] = {
    //gvPES2008demo,
    gvPES2008,
    gvPES2008v110,
    gvPES2008v120,
};

#define CODELEN 3
enum { 
    C_SET_EXHIB_TIME, C_SET_CUP_ENDURANCE, C_SET_CUP_TIME,
};

#define NOCODEADDR {0,0}
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR,
    // [Settings] PES2008 PC DEMO
    NOCODEADDR,
    // PES2008
    {
        0xaa6ed2, 0x8ff332, 0x90498d,
    },
    // [Settings] PES2008 PC
    NOCODEADDR,
    NOCODEADDR,
    // PES2008 1.10
    {
        0xaa7f72, 0x900262, 0x90475d,
    },
    NOCODEADDR,
    // PES2008 1.20
    { 
        0xaaa552, 0x905722, 0x9025cd,
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
