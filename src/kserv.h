// kserv.h

#include "afsreader.h"

#define MODID 100
#define NAMELONG L"Kitserver Module 7.1.0.6"
#define NAMESHORT L"KSERV"
#define DEFAULT_DEBUG 0

#pragma pack(4)

typedef struct _PNG_CHUNK_HEADER {
    DWORD dwSize;
    DWORD dwName;
} PNG_CHUNK_HEADER;

typedef struct _TEXTURE_ENTRY_HEADER
{
    BYTE sig[4]; // "WE00"
    DWORD unknown1;
    WORD width;
    WORD height;
    DWORD unknown2;
} TEXTURE_ENTRY_HEADER;

typedef struct _PALETTE_ENTRY
{
    BYTE b;
    BYTE g;
    BYTE r;
    BYTE a;
} PALETTE_ENTRY;

typedef struct _TEXTURE_ENTRY
{
    TEXTURE_ENTRY_HEADER header;
    PALETTE_ENTRY palette[256];
    BYTE data[1];
} TEXTURE_ENTRY;

typedef struct _BIN_INFO
{
    DWORD id;
    AFSITEMINFO itemInfo;
} BIN_INFO;

typedef DWORD KCOLOR;

typedef struct _KIT_INFO
{
    DWORD unknown0[2];
    KCOLOR mainColor;
    KCOLOR editShirtColors[4];
    KCOLOR shortsFirstColor;
    KCOLOR editKitColors[9];
    DWORD unknown1;
    KCOLOR editNumberAndFontColors1;
    DWORD unknown2;
    KCOLOR editNumberAndFontColors2;
    DWORD unknown3;
    KCOLOR editNumberAndFontColors3;
    DWORD unknown4;
    KCOLOR editNumberAndFontColors4;
    BYTE collar;
    BYTE editKitStyles[9];
    BYTE fontStyle;
    BYTE fontPosition;
    BYTE unknown5;
    BYTE fontShape;
    BYTE unknown7;
    BYTE frontNumberPosition;
    BYTE shortsNumberPosition;
    BYTE unknown8[2];
    BYTE flagPosition;
    DWORD unknown9;
    BYTE model;
    BYTE unknown10;
    WORD kitLink;
} KIT_INFO;

typedef struct _TEAM_KIT_INFO
{
    KIT_INFO ga;
    KIT_INFO pa;
    KIT_INFO gb;
    KIT_INFO pb;
} TEAM_KIT_INFO;

typedef struct _TEAM_MATCH_DATA_INFO
{
    BYTE unknown1[4];
    WORD teamIdSpecial;
    WORD teamId;
    BYTE unknown2[0x2d3c];
    TEAM_KIT_INFO tki;
} TEAM_MATCH_DATA_INFO;

typedef struct _NEXT_MATCH_DATA_INFO
{
    TEAM_MATCH_DATA_INFO* home;
    TEAM_MATCH_DATA_INFO* away;
} NEXT_MATCH_DATA_INFO;
