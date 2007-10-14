// detect.h

#ifndef _JUCE_DETECT_H
#define _JUCE_DETECT_H

#include "windows.h"
#include <stdio.h>

// Returns the game version id
int GetGameVersion(void);
int GetGameVersion(wchar_t* filename);
bool isGame(int gameVersion);

#endif

