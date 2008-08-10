// ADDRESSES for fserv.cpp
BYTE allowedGames[] = {
    //gvPES2008demo,
    gvPES2008,
    gvPES2008v110,
    gvPES2008v120,
};

#define CODELEN 3
enum { 
    C_CHECK_FACE_AND_HAIR_ID, C_COPY_DATA, C_COPY_DATA2,
};

#define NOCODEADDR {0,0,0}
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR,
    // [Settings] PES2008 PC DEMO
    NOCODEADDR,
    // PES2008
    {
        0xb6efcf, 0xc96248, 0x41433f,
    },
    // [Settings] PES2008 PC
    NOCODEADDR,
    NOCODEADDR,
    // PES2008 1.10
    {
        0xb6edef, 0xc97bd8, 0x413e8f,
    },
    NOCODEADDR,
    // PES2008 1.20
    { 
        0xb734cf, 0xc99b28, 0x414caf,
    },
};

#define DATALEN 2 
enum {
    EDIT_DATA_PTR, MENU_MODE_IDX,
};

#define NODATAADDR {0,0},
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
    NODATAADDR
    // [Settings] PES2008 PC DEMO
    NODATAADDR
    // PES2008
    { 
        0x1250d44, 0x1250fc8,
    },
    // [Settings] PES2008 PC
    NODATAADDR
    NODATAADDR
    // PES2008 1.10
    { 
        0x1251d6c, 0x1251ff0,
    },
    NODATAADDR
    // PES2008 1.20
    {
        0x1252d68, 0x1252ff0,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
