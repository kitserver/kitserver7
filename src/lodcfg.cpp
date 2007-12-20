/* main */
/* Version 1.0 */

#include <windows.h>
#include <windef.h>
#include <string.h>
#include <stdio.h>
#include <tchar.h>
#include <commctrl.h>

#define BUFLEN 4096

#include "lodcfgui.h"
#include "lodcfg.h"
#include "detect.h"
#include "imageutil.h"
#include "configs.h"
#include "lodmixer.h"

#define UNDEFINED -1
#define WM_APP_EXECHANGE WM_APP+1
#define CFG_FILE L"config.txt"

HWND hWnd = NULL;
bool g_buttonClick = true;
BOOL g_isBeingEdited = FALSE;

char* _help = 
"Enabling aspect ratio correction will allow you to play with \n\
round ball and correct proportions on any resolution. You can \n\
either let LOD mixer calculate it automatically at run-time \n\
based on the resolution, or set it manually here.\n\
\n\
Forcing particular resolution is also possible. If you play in \n\
a windowed mode, any resolution will work. But for full-screen, \n\
only those that are REALLY supported by your videocard will be\n\
in full-screen. Unsupported resolutions will switch back to\n\
windowed mode.\n\
\n\
There are 3 levels of LOD (level-of-detail). The sliders indicate \n\
when the switching between the levels occurs. Moving sliders to \n\
the left makes the game engine to switch earlier - as a result you \n\
get less detailed player models. You can try that if your PC is \n\
struggling and the framerate is poor. If, however, you've got a \n\
pretty powerful machine and a good videocard, then try moving the \n\
sliders to the right: the game engine will switch the LODs later, \n\
and therefore give you more detail.\n\
\n\
About controller check: PES 2008 doesn't allow human players to control\n\
both teams, unless both of their selected teams are playing against \n\
each other in the match. Now you can remove that limitation. So, even\n\
for P1 vs. COM game, or P2 vs. COM - you can freely select which team\n\
you control with each controller.\n\
\n\
Dont't forget to press the [Save] button!";

LMCONFIG _lmconfig = {
    {DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_ASPECT_RATIO}, 
    {DEFAULT_LOD_SWITCH1, DEFAULT_LOD_SWITCH2},
    DEFAULT_ASPECT_RATIO_CORRECTION_ENABLED,
    DEFAULT_CONTROLLER_CHECK_ENABLED,
};

// function prototypes
void lodmixerConfig(char* pName, const void* pValue, DWORD a);
void EnableControls(BOOL flag);
void SaveConfig();
void UpdateControls(LMCONFIG& cfg);
void UpdateConfig(LMCONFIG& cfg);
void ResetLodSwitches();

void ResetLodSwitches()
{
    SendMessage(g_lodEditControl[0],WM_SETTEXT,0,(LPARAM)"0.095");
    SendMessage(g_lodEditControl[1],WM_SETTEXT,0,(LPARAM)"0.074");
    SendMessage(g_lodTrackBarControl[0],TBM_SETPOS,TRUE,(LPARAM)getTickValue(0.095));
    SendMessage(g_lodTrackBarControl[1],TBM_SETPOS,TRUE,(LPARAM)getTickValue(0.074));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_DESTROY:
			// Exit the application when the window closes
			PostQuitMessage(1);
			return true;

		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				if ((HWND)lParam == g_saveButtonControl)
				{
					// save LOD
					g_buttonClick = true;

                    char exeName[BUFLEN];
                    ZeroMemory(exeName, BUFLEN);
					SaveConfig();
				}
				else if ((HWND)lParam == g_lodCheckBox)
                {
                    bool checked = SendMessage(g_lodCheckBox,BM_GETCHECK,0,0);
                    for (int i=0; i<2; i++)
                    {
                        EnableWindow(g_lodTrackBarControl[i], checked);
                    }
                    //EnableWindow(g_lodLabel1, checked);
                    //EnableWindow(g_lodLabel2, checked);
                    if (!checked) {
                        // disable custom LOD: switch back to defaults
                        ResetLodSwitches();
                    }

                }
				else if ((HWND)lParam == g_arCheckBox)
                {
                    bool checked = SendMessage(g_arCheckBox,BM_GETCHECK,0,0);
                    EnableWindow(g_arRadio1, checked);
                    EnableWindow(g_arRadio2, checked);
                    if (checked) {
                        bool checked1 = SendMessage(g_arRadio2,BM_GETCHECK,0,0);
                        EnableWindow(g_arEditControl, checked1);
                    } else {
                        EnableWindow(g_arEditControl, false);
                    }
                }
				else if ((HWND)lParam == g_arRadio1)
                {
                    EnableWindow(g_arEditControl, false);
                }
				else if ((HWND)lParam == g_arRadio2)
                {
                    EnableWindow(g_arEditControl, true);
                }
				else if ((HWND)lParam == g_resCheckBox)
                {
                    bool checked = SendMessage(g_resCheckBox,BM_GETCHECK,0,0);
                    EnableWindow(g_resWidthControl, checked);
                    EnableWindow(g_resHeightControl, checked);
                }
				else if ((HWND)lParam == g_defButtonControl)
				{
					// reset defaults 
					g_buttonClick = true;

                    // display small help window
                    MessageBox(hWnd, _help, "LOD Mixer Help", 0);
				}
			}
			else if (HIWORD(wParam) == CBN_EDITUPDATE)
			{
				g_isBeingEdited = TRUE;
			}
			else if (HIWORD(wParam) == CBN_KILLFOCUS && g_isBeingEdited)
			{
				g_isBeingEdited = FALSE;
				HWND control = (HWND)lParam;
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				HWND control = (HWND)lParam;
			}
			break;

		case WM_HSCROLL:
            if (LOWORD(wParam) == TB_THUMBPOSITION || LOWORD(wParam) == TB_THUMBTRACK ||
                    LOWORD(wParam)==TB_LINEUP || LOWORD(wParam)==TB_LINEDOWN ||
                    LOWORD(wParam)==TB_PAGEUP || LOWORD(wParam)==TB_PAGEDOWN)
            {
                if ((HWND)lParam == g_lodTrackBarControl[0])
                {
                    int val0 = SendMessage(g_lodTrackBarControl[0],TBM_GETPOS,0,0);
                    char buf[20];
                    sprintf(buf,"%0.3f",getSwitchValue(val0));
                    SendMessage(g_lodEditControl[0],WM_SETTEXT,0,(LPARAM)buf);

                    // adjust the other slider, if necessary
                    int val1 = SendMessage(g_lodTrackBarControl[1],TBM_GETPOS,0,0);
                    if (val0 > val1)
                    {
                        SendMessage(g_lodEditControl[1],WM_SETTEXT,0,(LPARAM)buf);
                        SendMessage(g_lodTrackBarControl[1],TBM_SETPOS,TRUE,(LPARAM)val0);
                    }
                }
                else if ((HWND)lParam == g_lodTrackBarControl[1])
                {
                    int val1 = SendMessage(g_lodTrackBarControl[1],TBM_GETPOS,0,0);
                    char buf[20];
                    sprintf(buf,"%0.3f",getSwitchValue(val1));
                    SendMessage(g_lodEditControl[1],WM_SETTEXT,0,(LPARAM)buf);

                    // adjust the other slider, if necessary
                    int val0 = SendMessage(g_lodTrackBarControl[0],TBM_GETPOS,0,0);
                    if (val0 > val1)
                    {
                        SendMessage(g_lodEditControl[0],WM_SETTEXT,0,(LPARAM)buf);
                        SendMessage(g_lodTrackBarControl[0],TBM_SETPOS,TRUE,(LPARAM)val1);
                    }
                }
            }
            break;

		case WM_APP_EXECHANGE:
			if (wParam == VK_RETURN) {
				g_isBeingEdited = FALSE;
				MessageBox(hWnd, "WM_APP_EXECHANGE", "Installer Message", 0);
			}
			break;
	}
	return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

bool InitApp(HINSTANCE hInstance, LPSTR lpCmdLine)
{
	WNDCLASSEX wcx;

	// cbSize - the size of the structure.
	wcx.cbSize = sizeof(WNDCLASSEX);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = (WNDPROC)WindowProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = hInstance;
	wcx.hIcon = LoadIcon(NULL,IDI_APPLICATION);
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = "LODCFGCLS";
	wcx.hIconSm = NULL;

	// Register the class with Windows
	if(!RegisterClassEx(&wcx))
		return false;

	return true;
}

HWND BuildWindow(int nCmdShow)
{
	DWORD style, xstyle;
	HWND retval;

	style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	xstyle = WS_EX_LEFT;

	retval = CreateWindowEx(xstyle,
        "LODCFGCLS",      // class name
        LODCFG_WINDOW_TITLE, // title for our window (appears in the titlebar)
        style,
        CW_USEDEFAULT,  // initial x coordinate
        CW_USEDEFAULT,  // initial y coordinate
        WIN_WIDTH, WIN_HEIGHT,   // width and height of the window
        NULL,           // no parent window.
        NULL,           // no menu
        NULL,           // no creator
        NULL);          // no extra data

	if (retval == NULL) return NULL;  // BAD.

	ShowWindow(retval,nCmdShow);  // Show the window
	return retval; // return its handle for future use.
}

void SaveConfig()
{
    UpdateConfig(_lmconfig);
    if (writeConfig(CFG_FILE))
    {
		// show message box with success msg
		char buf[BUFLEN];
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, "\
======= SUCCESS! ========\n\
LOD Mixer configuration saved.\n\
\n");
		MessageBox(hWnd, buf, "LOD Mixer Message", 0);

    } else {
		// show message box with error msg
		char buf[BUFLEN];
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, "\
========== ERROR! ===========\n\
Problem saving LOD Mixer configuration info this file:\n\
%s.\n\
\n", CFG_FILE);

		MessageBox(hWnd, buf, "LOD Mixer Message", 0);
		return;
	}
}

void UpdateControls(LMCONFIG& cfg)
{
    // Aspect Ratio
    SendMessage(g_arCheckBox, BM_SETCHECK, BST_CHECKED, 0);
    EnableWindow(g_arRadio1, true);
    EnableWindow(g_arRadio2, true);
    if (!cfg.aspectRatioCorrectionEnabled) 
    {
        SendMessage(g_arCheckBox, BM_SETCHECK, BST_UNCHECKED, 0);
        EnableWindow(g_arRadio1, false);
        EnableWindow(g_arRadio2, false);
        EnableWindow(g_arEditControl, false);
    }
    SendMessage(g_arRadio1, BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(g_arRadio2, BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(g_arEditControl, false);
    if (cfg.screen.aspectRatio > 0.0f)
    {
        SendMessage(g_arRadio1, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_arRadio2, BM_SETCHECK, BST_CHECKED, 0);
        EnableWindow(g_arEditControl, true);
        char buf[40] = {0}; sprintf(buf,"%0.5f",cfg.screen.aspectRatio);
        SendMessage(g_arEditControl, WM_SETTEXT, 0, (LPARAM)buf);
    } 

    // Resolution
    SendMessage(g_resCheckBox, BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(g_resWidthControl, false);
    EnableWindow(g_resHeightControl, false);
    if (cfg.screen.width > 0 && cfg.screen.height > 0)
    {
        SendMessage(g_resCheckBox, BM_SETCHECK, BST_CHECKED, 0);
        EnableWindow(g_resWidthControl, true);
        char buf[40] = {0}; sprintf(buf,"%d",cfg.screen.width);
        SendMessage(g_resWidthControl, WM_SETTEXT, 0, (LPARAM)buf);
        EnableWindow(g_resHeightControl, true);
        sprintf(buf,"%d",cfg.screen.height);
        SendMessage(g_resHeightControl, WM_SETTEXT, 0, (LPARAM)buf);
    }

    // LOD
    SendMessage(g_lodCheckBox, BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(g_lodTrackBarControl[0], false);
    EnableWindow(g_lodTrackBarControl[1], false);
    if (cfg.lod.switch1 > 0.0f)  
    {
        SendMessage(g_lodCheckBox, BM_SETCHECK, BST_CHECKED, 0);

        EnableWindow(g_lodTrackBarControl[0], true);
        EnableWindow(g_lodTrackBarControl[1], true);

        float sw = max(getMaxSwitchValue(),min(getMinSwitchValue(),cfg.lod.switch1));
        char buf[40] = {0}; sprintf(buf,"%0.3f",sw);
        SendMessage(g_lodTrackBarControl[0], TBM_SETPOS, TRUE, (LPARAM)getTickValue(sw));
        SendMessage(g_lodEditControl[0], WM_SETTEXT, 0, (LPARAM)buf);
    }
    else
    {
        SendMessage(g_lodEditControl[0],WM_SETTEXT,0,(LPARAM)"0.095");
        SendMessage(g_lodTrackBarControl[0],TBM_SETPOS,TRUE,(LPARAM)getTickValue(0.095));
    }
    if (cfg.lod.switch2 > 0.0f)  
    {
        SendMessage(g_lodCheckBox, BM_SETCHECK, BST_CHECKED, 0);

        EnableWindow(g_lodTrackBarControl[0], true);
        EnableWindow(g_lodTrackBarControl[1], true);

        float sw = max(getMaxSwitchValue(),min(getMinSwitchValue(),cfg.lod.switch2));
        char buf[40] = {0}; sprintf(buf,"%0.3f",sw);
        SendMessage(g_lodTrackBarControl[1], TBM_SETPOS, TRUE, (LPARAM)getTickValue(sw));
        SendMessage(g_lodEditControl[1], WM_SETTEXT, 0, (LPARAM)buf);
    }
    else
    {
        SendMessage(g_lodEditControl[1],WM_SETTEXT,0,(LPARAM)"0.074");
        SendMessage(g_lodTrackBarControl[1],TBM_SETPOS,TRUE,(LPARAM)getTickValue(0.074));
    }

    // Controller check
    SendMessage(g_controllerCheckBox, BM_SETCHECK, BST_UNCHECKED, 0);
    if (cfg.controllerCheckEnabled)
        SendMessage(g_controllerCheckBox, BM_SETCHECK, BST_CHECKED, 0);
}

void UpdateConfig(LMCONFIG& cfg)
{
    float val;
    int ival;
    char abuf[40] = {0};
    wchar_t buf[40] = {0};

    // Aspect ratio
    bool arChecked = SendMessage(g_arCheckBox, BM_GETCHECK, 0, 0);
    _setConfig("lodmixer", "aspect-ratio.correction.enabled", 
            (arChecked)?wstring(L"1"):wstring(L"0"));
    _removeConfig("lodmixer", "screen.aspect-ratio");
    if (arChecked)
    {
        bool manual = SendMessage(g_arRadio2, BM_GETCHECK, 0, 0);
        if (manual)
        {
            ZeroMemory(abuf, sizeof(abuf));
            SendMessage(g_arEditControl, WM_GETTEXT, sizeof(abuf), (LPARAM)abuf);
            if (sscanf(abuf,"%f",&val)==1 && val>0)
            {
                swprintf(buf,L"%0.5f",val);
                _setConfig("lodmixer", "screen.aspect-ratio", wstring(buf));
            }
        }
    }

    // Custom resolution
    _removeConfig("lodmixer", "screen.width");
    _removeConfig("lodmixer", "screen.height");
    bool resChecked = SendMessage(g_resCheckBox, BM_GETCHECK, 0, 0);
    if (resChecked)
    {
        ZeroMemory(abuf, sizeof(abuf));
        SendMessage(g_resWidthControl, WM_GETTEXT, sizeof(abuf), (LPARAM)abuf);
        if (sscanf(abuf,"%d",&ival)==1 && ival>0)
        {
            swprintf(buf,L"%d",ival);
            _setConfig("lodmixer", "screen.width", wstring(buf));
        }
        ZeroMemory(abuf, sizeof(abuf));
        SendMessage(g_resHeightControl, WM_GETTEXT, sizeof(abuf), (LPARAM)abuf);
        if (sscanf(abuf,"%d",&ival)==1 && ival>0)
        {
            swprintf(buf,L"%d",ival);
            _setConfig("lodmixer", "screen.height", wstring(buf));
        }
    }

    // LOD
    bool lodChecked = SendMessage(g_lodCheckBox, BM_GETCHECK, 0, 0);
    if (lodChecked)
    {
        val = getSwitchValue(SendMessage(g_lodTrackBarControl[0], TBM_GETPOS, 0, 0));
        swprintf(buf,L"%0.3f",val);
        _setConfig("lodmixer", "lod.switch1", wstring(buf));

        val = getSwitchValue(SendMessage(g_lodTrackBarControl[1], TBM_GETPOS, 0, 0));
        swprintf(buf,L"%0.3f",val);
        _setConfig("lodmixer", "lod.switch2", wstring(buf));
    }
    else
    {
        _removeConfig("lodmixer", "lod.switch1");
        _removeConfig("lodmixer", "lod.switch2");
    }

    // Controller check
    bool controllerChecked = SendMessage(g_controllerCheckBox, BM_GETCHECK, 0, 0);
    if (controllerChecked)
    {
        _setConfig("lodmixer", "controller.check.enabled", wstring(L"1"));
    }
    else
    {
        _removeConfig("lodmixer", "controller.check.enabled");
    }
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MSG msg; int retval;

 	if(InitApp(hInstance, lpCmdLine) == false)
		return 0;

	hWnd = BuildWindow(nCmdShow);
	if(hWnd == NULL)
		return 0;

	// build GUI
	if (!BuildControls(hWnd))
		return 0;

    readConfig(CFG_FILE);
    _getConfig("lodmixer", "screen.width", DT_DWORD, 1, lodmixerConfig);
    _getConfig("lodmixer", "screen.height", DT_DWORD, 2, lodmixerConfig);
    _getConfig("lodmixer", "screen.aspect-ratio", DT_FLOAT, 3, lodmixerConfig);
    _getConfig("lodmixer", "lod.switch1", DT_FLOAT, 4, lodmixerConfig);
    _getConfig("lodmixer", "lod.switch2", DT_FLOAT, 5, lodmixerConfig);
    _getConfig("lodmixer", "aspect-ratio.correction.enabled", DT_DWORD, 6, lodmixerConfig);
    _getConfig("lodmixer", "controller.check.enabled", DT_DWORD, 7, lodmixerConfig);

    UpdateControls(_lmconfig);

	// show credits
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	strncpy(buf, CREDITS, BUFLEN-1);
	SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)buf);

	while((retval = GetMessage(&msg,NULL,0,0)) != 0)
	{
		if(retval == -1)
			return 0;	// an error occured while getting a message

		if (!IsDialogMessage(hWnd, &msg)) // need to call this to make WS_TABSTOP work
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

void lodmixerConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1:	// width
			_lmconfig.screen.width = *(DWORD*)pValue;
			break;
		case 2: // height
			_lmconfig.screen.height = *(DWORD*)pValue;
			break;
		case 3: // aspect ratio
			_lmconfig.screen.aspectRatio = *(float*)pValue;
			break;
		case 4: // LOD 
			_lmconfig.lod.switch1 = *(float*)pValue;
			break;
		case 5: // LOD
			_lmconfig.lod.switch2 = *(float*)pValue;
			break;
		case 6: // LOD
			_lmconfig.aspectRatioCorrectionEnabled = *(DWORD*)pValue != 0;
			break;
		case 7: // Controller check
			_lmconfig.controllerCheckEnabled = *(DWORD*)pValue != 0;
			break;
	}
}

