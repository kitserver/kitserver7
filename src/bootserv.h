// bootserv.h

#define MODID 160
#define NAMELONG L"BOOTSERV Module 7.3.0.0"
#define NAMESHORT L"BOOTSERV"
#define DEFAULT_DEBUG 0

typedef struct _PLAYER_INFO
{
    DWORD id;
    char name[0x2e];
    BYTE unknown1[0x3c];
    BYTE unknown2;
    BYTE faceHairMask;
    BYTE unknown3[0x34];
    BYTE country;
    BYTE unknown4;
    WORD padding;
} PLAYER_INFO;

