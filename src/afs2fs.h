// afs2fs.h

#include "afsreader.h"

#define MODID 100
#define NAMELONG L"AFS2FS Module 7.1.1.0"
#define NAMESHORT L"AFS2FS"
#define DEFAULT_DEBUG 0

#pragma pack(4)

typedef struct _BIN_SIZE_INFO
{
    BYTE unknown1[0x10];
    char relativePathName[0x108];
    DWORD unknown2;
    DWORD sizes[1];
} BIN_SIZE_INFO;

typedef struct _GET_BIN_SIZE_STRUCT
{
    BYTE unknown1[0x0c];
    DWORD afsId;
    DWORD binId;
} GET_BIN_SIZE_STRUCT;

typedef struct _FILE_READ_STRUCT
{
    HANDLE hfile;
    DWORD unknown1[2];
    DWORD offset;
    DWORD unknown2;
    DWORD eventId;
    DWORD offset_again;
    DWORD unknown3;
    BYTE* destBuffer;
    DWORD bytesToRead;
} FILE_READ_STRUCT; 

typedef struct _READ_STRUCT
{
    BYTE unknown1[0x20];
    DWORD threadId;
    BYTE unknown2[0x10];
    FILE_READ_STRUCT* pfrs;
    DWORD count;
} READ_STRUCT; 

typedef struct _READ_EVENT_STRUCT
{
    DWORD unknown1[3];
    DWORD offsetPages;
    DWORD sizeBytes;
    DWORD unknown2;
    DWORD sizePages;
    DWORD unknown3;
    DWORD unknown4[13];
    DWORD binSizesTableAddr;
} READ_EVENT_STRUCT;

typedef struct _FILE_STRUCT
{
    HANDLE hfile;
    DWORD fsize;
    DWORD offset;
    DWORD orgSize;
    DWORD binKey;
} FILE_STRUCT;

