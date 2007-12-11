#ifndef _JUCE_CRC32_
#define _JUCE_CRC32_

#include <windows.h>
#include "afsreader.h"

DWORD Sign(PACKED_BIN* buffer);
DWORD GetCRC(BYTE* data, DWORD len);

#endif
