#define UNICODE

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>

extern const wchar_t* _getTransl(char* section, char* key);
#define lang(s) _getTransl("setup",s)

#define BUFLEN 4096

#include "lodcfgui.h"

HWND g_lodListControl[5];          // lod lists
HWND g_crowdCheckBox;             // crowd
HWND g_JapanCheckBox;             // Japan check
HWND g_arCheckBox;            // aspect ratio correction check
HWND g_controllerCheckBox;            // controller check

HWND g_weatherListControl;         // weather (default,fine,rainy,random)
HWND g_timeListControl;            // time of the day (default,day,night)
HWND g_seasonListControl;          // season (default,summer,winter)
HWND g_stadiumEffectsListControl;  // stadium effects (default,0/1)
HWND g_numberOfSubs;               // number of substitutions allowed
HWND g_homeCrowdListControl;       // home crowd attendance (default,0-3)
HWND g_awayCrowdListControl;       // away crowd attendance (default,0-3)
HWND g_crowdStanceListControl;      // crowd stance (default,1-3)

HWND g_resCheckBox;
HWND g_resWidthControl;
HWND g_resHeightControl;
HWND g_arRadio1;
HWND g_arRadio2;
HWND g_arEditControl;
HWND g_lodCheckBox;
HWND g_defLodControl;
HWND g_lodLabel1;
HWND g_lodLabel2;
HWND g_lodTrackBarControl[2];
HWND g_lodEditControl[2];

HWND g_statusTextControl;          // displays status messages
HWND g_saveButtonControl;        // save settings button
HWND g_defButtonControl;          // default settings button

//#define LDSW_MIN 10    // 0.00005
//#define LDSW_MAX 1530  // 200.0
#define LDSW_MIN 470  // 0.005
#define LDSW_MAX 1461 // 100.0

float getSwitchValue(int tickValue)
{
    return exp((LDSW_MAX + LDSW_MIN - tickValue-1000)/100.0);
}

float getMinSwitchValue()
{
    return exp((LDSW_MAX - 1000)/100.0);
}

float getMaxSwitchValue()
{
    return exp((LDSW_MIN - 1000)/100.0);
}

float round(float val)
{
    float lo = floorf(val);
    float hi = ceilf(val);
    return (fabs(val-lo) < fabs(val-hi)) ? lo : hi;
}

int getTickValue(float switchValue)
{
    int val = LDSW_MAX + LDSW_MIN - (round(log(switchValue)*100)+1000);
    if (val<LDSW_MIN) return LDSW_MIN;
    if (val>LDSW_MAX) return LDSW_MAX;
    return val;
}

// CreateTrackbar - creates and initializes a trackbar. 
HWND WINAPI CreateTrackbar( 
    HWND hwndDlg,  // handle of dialog box (parent window) 
    UINT x, UINT y,
    UINT w, UINT h,
    UINT iMin,     // minimum value in trackbar range 
    UINT iMax,     // maximum value in trackbar range 
    UINT iSelMin,  // minimum value in trackbar selection 
    UINT iSelMax)  // maximum value in trackbar selection 
{ 

    InitCommonControls(); // loads common control's DLL 

    HWND hwndTrack = CreateWindowEx( 
        0,                             // no extended styles 
        TRACKBAR_CLASS,                // class name 
        L"Trackbar Control",            // title (caption) 
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | /*TBS_ENABLESELRANGE |*/ TBS_HORZ | TBS_TOP | TBS_FIXEDLENGTH, 
        //TBS_AUTOTICKS | TBS_ENABLESELRANGE,  // style 
        x, y,                        // position 
        w, h,                       // size 
        hwndDlg,                       // parent window 
        NULL,//ID_TRACKBAR,             // control identifier 
        NULL,                       // instance 
        NULL                           // no WM_CREATE parameter 
        ); 

    SendMessage(hwndTrack, TBM_SETRANGE, 
        (WPARAM) TRUE,                   // redraw flag 
        (LPARAM) MAKELONG(iMin, iMax));  // min. & max. positions 
    SendMessage(hwndTrack, TBM_SETPAGESIZE, 
        0, (LPARAM) 4);                  // new page size 

    SendMessage(hwndTrack, TBM_SETSEL, 
        (WPARAM) FALSE,                  // redraw flag 
        (LPARAM) MAKELONG(iSelMin, iSelMax)); 
    SendMessage(hwndTrack, TBM_SETPOS, 
        (WPARAM) TRUE,                   // redraw flag 
        (LPARAM) iSelMin); 

    SendMessage(hwndTrack, TBM_SETTHUMBLENGTH, (WPARAM)25, (LPARAM)0);
    //SetFocus(hwndTrack); 

    return hwndTrack; 
} 


/**
 * build all controls
 */
bool BuildControls(HWND parent)
{
	HGDIOBJ hObj;
	DWORD style, xstyle;
	int x, y, spacer;
	int boxW, boxH, statW, statH, borW, borH, butW, butH, editW, editH;

	// Get a handle to the stock font object
	hObj = GetStockObject(DEFAULT_GUI_FONT);

	spacer = 6; 
	x = spacer, y = spacer;
	butH = 22;
	butW = 60;

	// use default extended style
	xstyle = WS_EX_LEFT;
	style = WS_CHILD | WS_VISIBLE;

	// TOP SECTION: Aspect ratio

	borW = WIN_WIDTH - spacer*3;
	statW = 40;
	boxW = borW - statW - spacer*3; boxH = 22; statH = 16;
	borH = spacer*3 + boxH*2;

	statW = borW - spacer*4;

    int i;
	style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX;

    x = spacer;
	HWND staticCrowdBorderTopControl = CreateWindowEx(
			xstyle, L"Static", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW, butH*2+spacer*3,
			parent, NULL, NULL, NULL);

    y += spacer;
    x = spacer*2;
	g_arCheckBox = CreateWindowEx(
			xstyle, L"button", L"Enable aspect ratio correction", style,
			x, y, 250, butH,
			parent, NULL, NULL, NULL);

    editW = 66;
    editH = statH + 6;
    x += spacer*2 + 250;

	style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    butW = 90;
	g_arRadio1 = CreateWindowEx(
			xstyle, L"button", L"Automatic", style,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);
    SendMessage(g_arRadio1, WM_SETFONT, (WPARAM)hObj, true);
    //EnableWindow(g_arRadio1, FALSE);

    y += spacer + butH;
    butW = 78;
	g_arRadio2 = CreateWindowEx(
			xstyle, L"button", L"Manual", style,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);
    SendMessage(g_arRadio2, WM_SETFONT, (WPARAM)hObj, true);
    //EnableWindow(g_arRadio2, FALSE);
   
    x += spacer*2 + butW;
	style = WS_CHILD | WS_VISIBLE | ES_LEFT;
	g_arEditControl = CreateWindowEx(
			WS_EX_CLIENTEDGE, L"edit", L"", style,
			x, y-2, editW, editH,
			parent, NULL, NULL, NULL);
    SendMessage(g_arEditControl, WM_SETFONT, (WPARAM)hObj, true);
    EnableWindow(g_arEditControl, FALSE);

    ////////////// Custom resolution ////////////////////////////////////

    style = WS_CHILD | WS_VISIBLE;
    y += spacer*3 + butH;

    x = spacer;
	HWND staticWeatherBorderTopControl = CreateWindowEx(
			xstyle, L"Static", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW, boxH+spacer*2,
			parent, NULL, NULL, NULL);

    x = spacer*2;
    y += spacer;
	g_resCheckBox = CreateWindowEx(
			xstyle, L"button", L"Enforce custom resolution", style | BS_AUTOCHECKBOX,
			x, y, 250, butH,
			parent, NULL, NULL, NULL);
    SendMessage(g_resCheckBox, WM_SETFONT, (WPARAM)hObj, true);

    editW = 66;
    editH = statH + 6;

    x += spacer*2 + 250;

	style = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_NUMBER;
	g_resWidthControl = CreateWindowEx(
			WS_EX_CLIENTEDGE, L"edit", L"", style | WS_TABSTOP,
			x, y, editW, editH,
			parent, NULL, NULL, NULL);
    SendMessage(g_resWidthControl, WM_SETFONT, (WPARAM)hObj, true);
    EnableWindow(g_resWidthControl, FALSE);

    statW = 12;
    x += editW + spacer;
    HWND heightLabel = CreateWindowEx(
            xstyle, L"Static", L" x ", style,
            x, y+2, statW, statH, 
            parent, NULL, NULL, NULL);
    SendMessage(heightLabel, WM_SETFONT, (WPARAM)hObj, true);

    x += statW + spacer;
	style = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_NUMBER;
	g_resHeightControl = CreateWindowEx(
			WS_EX_CLIENTEDGE, L"edit", L"", style | WS_TABSTOP,
			x, y, editW, editH,
			parent, NULL, NULL, NULL);
    SendMessage(g_resHeightControl, WM_SETFONT, (WPARAM)hObj, true);
    EnableWindow(g_resHeightControl, FALSE);

    // LOD
    /////////////////////////////////////////////////////////////
    
	style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX;
	x = spacer*2;
	y += boxH + spacer*4;

    statW = 100;
    editW = 50;
    editH = statH;
    UINT tw = borW - spacer*6 - statW - editW;
    UINT th = 28;
    borH = th*2 + spacer*6 + butH;

    x = spacer;
	HWND staticBorderTopControl3 = CreateWindowEx(
			xstyle, L"Static", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW, borH,
			parent, NULL, NULL, NULL);

    y += spacer;
    x = spacer*2;
	g_lodCheckBox = CreateWindowEx(
			xstyle, L"button", L"Custom LOD configuration", style,
			x, y, 260, butH,
			parent, NULL, NULL, NULL);

	y += boxH + spacer;
    x = spacer*2;

	g_lodLabel1 = CreateWindowEx(
			xstyle, L"Static", L"LOD switch 0/1", WS_CHILD | WS_VISIBLE,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    x += statW + spacer;
    g_lodTrackBarControl[0] = CreateTrackbar(parent,x,y,tw,th,LDSW_MIN,LDSW_MAX,
            getTickValue(0.095),getTickValue(0.095));
    x += tw + spacer;
	g_lodEditControl[0] = CreateWindowEx(
			xstyle, L"static", L"0.095", WS_CHILD | WS_VISIBLE,
			x, y, editW, editH,
			parent, NULL, NULL, NULL);
    y += th + spacer;

    x = spacer*2;
	g_lodLabel2 = CreateWindowEx(
			xstyle, L"Static", L"LOD switch 1/2", WS_CHILD | WS_VISIBLE,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    x += statW + spacer;
    g_lodTrackBarControl[1] = CreateTrackbar(parent,x,y,tw,th,LDSW_MIN,LDSW_MAX,
            getTickValue(0.074),getTickValue(0.074));
    x += tw + spacer;
	g_lodEditControl[1] = CreateWindowEx(
			xstyle, L"static", L"0.074", WS_CHILD | WS_VISIBLE,
			x, y, editW, editH,
			parent, NULL, NULL, NULL);
    y += th + spacer;

    for (i=0; i<2; i++)
        SendMessage(g_lodTrackBarControl[i], 
                TBM_SETTICFREQ, (WPARAM)((int)(LDSW_MAX-LDSW_MIN)/55), 0);

	style = WS_CHILD | WS_VISIBLE;

    y += spacer*4;

    // controller check
    x = spacer;
	HWND staticBorderTopControl4 = CreateWindowEx(
			xstyle, L"Static", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW, boxH+spacer*2,
			parent, NULL, NULL, NULL);

    y += spacer;
    x = spacer*2;
	g_controllerCheckBox = CreateWindowEx(
			xstyle, L"button", L"Disable controller selection check", 
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			x, y, 260, butH,
			parent, NULL, NULL, NULL);

	y += boxH + spacer*2;
    x = spacer*2;

	// BOTTOM sections: buttons
	
	x = borW - butW;
	g_saveButtonControl = CreateWindowEx(
			xstyle, L"Button", L"Save", style,
			x, y, butW + spacer, butH,
			parent, NULL, NULL, NULL);

    butW = 60;
	x -= butW + spacer;

	g_defButtonControl = CreateWindowEx(
			xstyle, L"Button", L"Help", style,
			x - spacer, y, butW + spacer, butH,
			parent, NULL, NULL, NULL);

	x = spacer;
	statW = WIN_WIDTH - spacer*4 - 160;

	g_statusTextControl = CreateWindowEx(
			xstyle, L"Static", L"", style,
			x, y+6, statW, statH,
			parent, NULL, NULL, NULL);

    for (i=0; i<5; i++) {
        SendMessage(g_lodListControl[i], WM_SETFONT, (WPARAM)hObj, true);
		EnableWindow(g_lodListControl[i], FALSE);
    }
    EnableWindow(g_crowdCheckBox, FALSE);
    SendMessage(g_weatherListControl, WM_SETFONT, (WPARAM)hObj, true);
    EnableWindow(g_weatherListControl, FALSE);

	SendMessage(g_statusTextControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_saveButtonControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_defButtonControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_crowdCheckBox, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_arCheckBox, WM_SETFONT, (WPARAM)hObj, true);

	SendMessage(g_lodCheckBox, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_controllerCheckBox, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_defLodControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_lodLabel1, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_lodLabel2, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_lodEditControl[0], WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_lodEditControl[1], WM_SETFONT, (WPARAM)hObj, true);

    //EnableWindow(g_lodLabel1, FALSE);
    //EnableWindow(g_lodLabel2, FALSE);
    for (i=0; i<2; i++) {
        EnableWindow(g_lodTrackBarControl[i], FALSE);
        EnableWindow(g_lodEditControl[i], TRUE);
    }

	//EnableWindow(g_saveButtonControl, FALSE);
	//EnableWindow(g_defButtonControl, FALSE);

	return true;
}

