// afs2fs.h

#include "afsreader.h"

#define MODID 100
#define NAMELONG L"AFS2FS Module 7.1.0.0"
#define NAMESHORT L"AFS2FS"
#define DEFAULT_DEBUG 0

#pragma pack(4)

typedef struct _READ_BIN_STRUCT
{
    DWORD afsId;
    DWORD binId;
    HANDLE hfile;
    DWORD fsize;
} READ_BIN_STRUCT;

typedef struct _BIN_SIZE_INFO
{
    BYTE unknown1[0x10];
    char relativePathName[0x108];
    DWORD unknown2;
    DWORD sizes[1];
} BIN_SIZE_INFO;

