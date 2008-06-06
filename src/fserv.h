// fserv.h

#define MODID 151
#define NAMELONG L"FSERV Module 7.1.0.0"
#define NAMESHORT L"FSERV"
#define DEFAULT_DEBUG 0

typedef struct _PLAYER_INFO
{
    DWORD id;
    BYTE unknown1[0x6a];
    BYTE unknown2;
    BYTE faceHairMask;
    BYTE unknown[0x34];
    BYTE country;
    BYTE age;
    WORD padding;
} PLAYER_INFO;
