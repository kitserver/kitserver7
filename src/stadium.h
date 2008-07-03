// stadium.h

#define MODID 155
#define NAMELONG L"STADIUM Module 7.3.0.0"
#define NAMESHORT L"STADIUM"
#define DEFAULT_DEBUG 0

#include "teaminfo.h"

typedef struct _STADIUM_INFO
{
    char* name;
    char* region;
    char* country;
    char* city;
    BYTE unknown[0x10];
} STADIUM_INFO;

