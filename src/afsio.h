// afsio.h
#ifndef _AFSIO_H
#define _AFSIO_H

#pragma pack(4)

// CALLBACK PROTOTYPES
////////////////////////////////////////////////////////////////////////////////////

typedef bool (*CLBK_GET_FILE_INFO)(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);

// FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////

KEXPORT bool afsioAddCallback(const CLBK_GET_FILE_INFO callback);
KEXPORT bool afsioRemoveCallback(const CLBK_GET_FILE_INFO callback);

KEXPORT bool afsioExtendSlots_cv0(int num_slots);

// STRUCTURES
////////////////////////////////////////////////////////////////////////////////////

typedef struct _BIN_SIZE_INFO
{
    DWORD unknown1;
    DWORD structSize;
    DWORD numItems;
    WORD numItems2;
    WORD unknown2;
    char relativePathName[0x108];
    DWORD unknown3;
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
    DWORD binKey;
} FILE_STRUCT;

#endif

