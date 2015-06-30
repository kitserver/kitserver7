// player.h

#ifndef _PLAYER_H
#define _PLAYER_H

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

#endif
