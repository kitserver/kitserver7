// win32gui.h

#ifndef _JUCE_WIN32GUI
#define _JUCE_WIN32GUI

#include <windows.h>

#define WIN_WIDTH 540 
#define WIN_HEIGHT 365

extern HWND g_lodListControl[5];          // lod lists
extern HWND g_crowdCheckBox;              // crowd
extern HWND g_JapanCheckBox;              // Japan check

extern HWND g_weatherListControl;         // weather (default,fine,rainy,random)
extern HWND g_timeListControl;            // time of the day (default,day,night)
extern HWND g_seasonListControl;          // season (default,summer,winter)
extern HWND g_stadiumEffectsListControl;  // stadium effects (default,0/1)
extern HWND g_numberOfSubs;               // number of substitutions
extern HWND g_homeCrowdListControl;       // home crowd attendance (default,0-3)
extern HWND g_awayCrowdListControl;       // away crowd attendance (default,0-3)
extern HWND g_crowdStanceListControl;      // crowd stance (default,1-3)

extern HWND g_resCheckBox;
extern HWND g_resWidthControl;
extern HWND g_resHeightControl;
extern HWND g_arCheckBox;
extern HWND g_arRadio1;
extern HWND g_arRadio2;
extern HWND g_arEditControl;
extern HWND g_lodCheckBox;
extern HWND g_controllerCheckBox;
extern HWND g_defLodControl;
extern HWND g_lodLabel1;
extern HWND g_lodLabel2;
extern HWND g_lodTrackBarControl[2];
extern HWND g_lodEditControl[2];

extern HWND g_statusTextControl;          // displays status messages
extern HWND g_saveButtonControl;          // save settings button
extern HWND g_defButtonControl;          // default settings button

// functions
bool BuildControls(HWND parent);
int getTickValue(float switchValue);
float getSwitchValue(int tickValue);
float getMaxSwitchValue();
float getMinSwitchValue();

#endif
