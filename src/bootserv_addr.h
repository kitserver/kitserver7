// ADDRESSES for bootserv.cpp
BYTE allowedGames[] = {
    //gvPES2008demo,
    gvPES2008,
    gvPES2008v110,
    gvPES2008v120,
};

#define CODELEN 11
enum { 
    C_COPY_DATA, C_COPY_DATA2,
    C_GET_BOOT_ID1, C_GET_BOOT_ID2,
    C_GET_BOOT_ID3, C_GET_BOOT_ID4,
    C_GET_BOOT_ID5, C_GET_BOOT_ID6,
    C_ADVANCE_BOOT_COUNTER,
    C_PROCESS_BOOT_ID, C_ENTRANCE_BOOTS,
};

#define NOCODEADDR {0,0,0,0,0,0,0,0,0,0,0}
DWORD codeArray[][CODELEN] = { 
    // PES2008 DEMO
    NOCODEADDR,
    // [Settings] PES2008 PC DEMO
    NOCODEADDR,
    // PES2008
    {
        0xc96248, 0x41433f,
        0x41dbe1, 0x46b9d1, 
        0xb6e460, 0xb6e486,
        0xb6e54a, 0xb6e714,
        0xb6e9cc,
        0xb6f02a, 0x7aac96,
    },
    // [Settings] PES2008 PC
    NOCODEADDR,
    NOCODEADDR,
    // PES2008 1.10
    {
        0xc97bd8, 0x413e8f,
        0x41dbe1, 0x46c5b1,
        0xb6e280, 0xb6e2a6,
        0xb6e36a, 0xb6e534,
        0xb6e7ec,
        0xb6ee4a, 0x7ab4d6,
    },
    NOCODEADDR,
    // PES2008 1.20
    { 
        0xc99b28, 0x414caf,
        0x41dc41, 0x46c801,
        0xb72960, 0xb72986,
        0xb72a4a, 0xb72c14,
        0xb72ecc,
        0xb7352a, 0x7acf56,
    },
};

#define DATALEN 3
enum {
    EDIT_DATA_PTR, MENU_MODE_IDX, NEXT_MATCH_DATA_PTR,
};

#define NODATAADDR {0,0,0},
DWORD dataArray[][DATALEN] = {
    // PES2008 DEMO
    NODATAADDR
    // [Settings] PES2008 PC DEMO
    NODATAADDR
    // PES2008
    { 
        0x1250d44, 0x1250fc8, 0x1250d4c,
    },
    // [Settings] PES2008 PC
    NODATAADDR
    NODATAADDR
    // PES2008 1.10
    { 
        0x1251d6c, 0x1251ff0, 0x1251d74,
    },
    NODATAADDR
    // PES2008 1.20
    {
        0x1252d68, 0x1252ff0, 0x1252d70,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];
