// kserv.h

#include "afsreader.h"
#include "teaminfo.h"

#define MODID 100
#define NAMELONG L"Kitserver Module 7.3.1.5"
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

