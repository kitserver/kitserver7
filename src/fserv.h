// fserv.h

#define MODID 151
#define NAMELONG L"FSERV Module 7.3.0.0"
#define NAMESHORT L"FSERV"
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

typedef struct _REPLAY_PLAYER_INFO
{
    WORD unknown1;
    BYTE unknown2;
    BYTE faceHairMask;
    BYTE unknown3[0x34];
    BYTE country;
    BYTE unknown4;
    WORD padding;
    BYTE unknown5[3];
    char name[0x2e];
    char nameOnShirt[0x13];
} REPLAY_PLAYER_INFO;

typedef struct _REPLAY_DATA_PAYLOAD
{
    BYTE miscInfo[0xa0];
    REPLAY_PLAYER_INFO players[22];
} REPLAY_DATA_PAYLOAD;

typedef struct _REPLAY_DATA
{
    BYTE header[0x100];
    DWORD fileType;
    DWORD size;
    DWORD checksum;
    DWORD unknown;
    REPLAY_DATA_PAYLOAD payload;
} REPLAY_DATA;

