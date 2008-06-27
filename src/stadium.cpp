/* stadium module */
#define UNICODE
#define THISMOD &k_stad

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afsio.h"
#include "stadium.h"
#include "stadium_addr.h"
#include "dllinit.h"
#include "gdb.h"
#include "pngdib.h"
#include "utf8.h"

#define lang(s) getTransl("stadium",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

//#define CREATE_FLAGS FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING
#define CREATE_FLAGS 0

enum {
    PLACKARDS, ADBOARDS_1, ADBOARDS_2, GOALS,
    DAY_SUMMER_MESH, DAY_SUMMER_TURF,
    DAY_WINTER_MESH, DAY_WINTER_TURF,
    EVENING_SUMMER_MESH, EVENING_SUMMER_TURF, 
    EVENING_WINTER_MESH, EVENING_WINTER_TURF,
    NIGHT_SUMMER_MESH, NIGHT_SUMMER_TURF,
    NIGHT_WINTER_MESH, NIGHT_WINTER_TURF,
};

wstring _stad_names[] = {
    L"stad_plackards.bin", L"stad_adboards1.bin", 
    L"stad_adboards2.bin", L"stad_goals.bin",
    L"day_summer_mesh.bin", L"day_summer_turf.bin",
    L"day_winter_mesh.bin", L"day_winter_turf.bin",
    L"evening_summer_mesh.bin", L"evening_summer_turf.bin",
    L"evening_winter_mesh.bin", L"evening_winter_turf.bin",
    L"night_summer_mesh.bin", L"night_summer_turf.bin",
    L"night_winter_mesh.bin", L"night_winter_turf.bin",
};

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_stad = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

#define STAD_FIRST  45
#define STAD_LAST   712
#define STAD_SPAN   42

#define STAD_PLACKARDS  37
#define STAD_ADBOARDS_1 38
#define STAD_ADBOARDS_2 39
#define STAD_GOALS      40

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void stadConfig(char* pName, const void* pValue, DWORD a);
bool stadGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
bool IsStadiumFile(DWORD binId);
int GetBinType(DWORD binId);
void stadOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode);
void stadKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam);
void stadPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);


// GLOBALS

bool _stadium_bins[726];

static int _myPage = -1;
static bool g_presentHooked = false;
static bool g_beginShowKitSelection = false;

class stadium_t
{
public:
    wstring _files[16];
    wstring _dir;
    wstring _name;
    int _capacity;
    int _built;

    stadium_t() : _capacity(0), _built(0) {}
    bool init(const wstring& dir) 
    {
        _dir = dir;

        // read the file names;
        WIN32_FIND_DATA fData;
        wstring pattern(getPesInfo()->gdbDir);
        pattern += L"GDB\\stadiums\\" + dir + L"\\*";

        HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
        if (hff != INVALID_HANDLE_VALUE) 
        {
            while (true)
            {
                // check if this is a file
                if (!(fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) 
                {
                    // Format A: check for BIN number
                    int binId = -1;
                    wchar_t* s = wcsrchr(fData.cFileName,'_');
                    if (s && swscanf(s+1,L"%d",&binId)==1)
                    {
                        TRACE1S1N(L"folder={%s}, bin={%d}",dir.c_str(),binId);
                        if (binId >= 0)
                        {
                            int t = GetBinType(binId);
                            if (0 <= t && t < 16)
                                _files[t] = fData.cFileName;
                        }
                    }

                    // Format B: check for standard name
                    if (binId == -1)
                    {
                        wstring name(fData.cFileName);
                        for (int i=0; i<16; i++)
                        {
                            if (_stad_names[i] == name)
                            {
                                _files[i] = fData.cFileName;
                                break;
                            }
                        }

                    } // binId
                }

                // proceed to next file
                if (!FindNextFile(hff, &fData)) break;
            }
            FindClose(hff);
        }

        // parse information file, if provided
        this->_parseInfoFile();

        LOG1S(L"initialized stadium: %s", _dir.c_str());
        return true;
    }

    void _parseInfoFile()
    {
    }
};

// main map of stadiums
map<wstring,stadium_t> _stadiums;

// map of home stadiums
typedef map<wstring,stadium_t>::iterator stadium_iter_t;
map<WORD,stadium_iter_t> _home_stadiums;


static void InitMaps()
{
	WIN32_FIND_DATA fData;
    wstring pattern(getPesInfo()->gdbDir);
    pattern += L"GDB\\stadiums\\*";

	HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
    if (hff != INVALID_HANDLE_VALUE)
    {
        while(true)
        {
            // check if this is a directory
            if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY 
                    && wcscmp(fData.cFileName,L".")!=0 
                    && wcscmp(fData.cFileName,L"..")!=0)
            {
                stadium_t stadium;
                stadium.init(fData.cFileName);
                _stadiums.insert(pair<wstring,stadium_t>(
                            fData.cFileName, stadium
                ));
            }

            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);
    }

    LOG1N(L"total stadiums: %d", _stadiums.size());
    LOG1N(L"home stadiums assigned: %d", _home_stadiums.size());
}


/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;

		RegisterKModule(THISMOD);
		
		if (!checkGameVersion()) {
			LOG(L"Sorry, your game version isn't supported!");
			return false;
		}

        CHECK_KLOAD(MAKELONG(3,7));

        // we need to do this early so that the overlay pages
        // appear in the same order as DLLs in config.txt
        _myPage = addOverlayCallback(stadOverlayEvent,true);
        LOG1N(L"_myPage = %d", _myPage);

		copyAdresses();
		hookFunction(hk_D3D_CreateDevice, initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
	}
	
	return true;
}

void stadConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1: // debug
			k_stad.debug = *(DWORD*)pValue;
			break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

    getConfig("stadium", "debug", DT_DWORD, 1, stadConfig);

	unhookFunction(hk_D3D_CreateDevice, initModule);

    // fill in BINs map
    ZeroMemory(_stadium_bins, sizeof(_stadium_bins));
    for (DWORD i=0; i<726; i++)
        if (GetBinType(i)!=-1)
            _stadium_bins[i] = true;

    // register callbacks
    afsioAddCallback(stadGetFileInfo);
    addKeyboardCallback(stadKeyboardEvent);

	TRACE(L"Hooking done.");

    InitMaps();
    return D3D_OK;
}

bool inline IsStadiumFile(DWORD binId)
{
    return _stadium_bins[binId];
}

int GetBinType(DWORD binId)
{
    switch (binId)
    {
        case STAD_PLACKARDS:
            return PLACKARDS;
        case STAD_ADBOARDS_1:
            return ADBOARDS_1;
        case STAD_ADBOARDS_2:
            return ADBOARDS_2;
        case STAD_GOALS:
            return GOALS;
    }

    if (binId < STAD_FIRST || binId > STAD_LAST)
        return -1;  // not a stadium file

    switch ((binId - STAD_FIRST) % STAD_SPAN)
    {
        case 0: 
            return DAY_SUMMER_MESH;
        case 2:
            return DAY_SUMMER_TURF; 
        case 7: 
            return DAY_WINTER_MESH;
        case 9:
            return DAY_WINTER_TURF; 
        case 14: 
            return EVENING_SUMMER_MESH;
        case 16:
            return EVENING_SUMMER_TURF; 
        case 21: 
            return EVENING_WINTER_MESH;
        case 23:
            return EVENING_WINTER_TURF; 
        case 28: 
            return NIGHT_SUMMER_MESH;
        case 30:
            return NIGHT_SUMMER_TURF; 
        case 35: 
            return NIGHT_WINTER_MESH;
        case 37:
            return NIGHT_WINTER_TURF; 
    }

    return -1; // not a stadium file
}

/**
 * Simple file-check routine.
 */
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size)
{
    TRACE1S(L"OpenFileIfExists:: %s", filename);
    handle = CreateFile(filename,           // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL | CREATE_FLAGS, // normal file
                       NULL);                 // no attr. template

    if (handle != INVALID_HANDLE_VALUE)
    {
        size = GetFileSize(handle,NULL);
        return true;
    }
    return false;
}

/**
 * AFSIO callback
 */
bool stadGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize)
{
    if (afsId != 4 || !IsStadiumFile(binId)) // fast check
        return false;

    LOG1N(L"binId = %d", binId);

    wchar_t filename[1024] = {0};
    swprintf(filename,L"%sGDB\\stadiums\\Wembley\\wembley_%d.bin", 
            getPesInfo()->gdbDir,
            (binId==40) ? 40 : (213+((binId-STAD_FIRST)%STAD_SPAN)));

    return OpenFileIfExists(filename, hfile, fsize);
}

void stadOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode)
{
    if (isExhibitionMode)
    {
        if (overlayOn)
        {
            hookFunction(hk_D3D_Present, stadPresent);
            setOverlayPageVisible(_myPage, true);
            TRACE(L"Showing kit selection");
            g_presentHooked = true;
            g_beginShowKitSelection = true;

            //if (delta==1) 
            //    ResetIterators();
        }
        else
        {
            setOverlayPageVisible(_myPage, false);
            unhookFunction(hk_D3D_Present, stadPresent);
            TRACE(L"Hiding kit selection");
            g_presentHooked = false;

            //if (delta==1 && menuMode == 0x0a)
            //    ResetIterators();
        }
    }
    else
    {
        if (overlayOn)
        {
            hookFunction(hk_D3D_Present, stadPresent);
            setOverlayPageVisible(_myPage, true);
            g_presentHooked = true;
            g_beginShowKitSelection = true;
        }
        else 
        {
            setOverlayPageVisible(_myPage, false);
            unhookFunction(hk_D3D_Present, stadPresent);
            g_presentHooked = false;
        }
    }
}

void stadPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused)
{
    if (getOverlayPage() != _myPage) // page-check
        return;

    if (g_beginShowKitSelection)
    {
        g_beginShowKitSelection = false;
    }

	KDrawText(L"stadPresent", 0, 0, D3DCOLOR_RGBA(0xff,0xff,0xff,0xff), 20.0f);

    /*
    // display "home"/"away" helper for Master League games
    if (pNextMatch->home->teamIdSpecial != pNextMatch->home->teamId)
    {
        KDrawText(L"Home", 7, 35, COLOR_BLACK, 26.0f);
        KDrawText(L"Home", 5, 33, COLOR_INFO, 26.0f);
    }
    else if (pNextMatch->away->teamIdSpecial != pNextMatch->away->teamId)
    {
        KDrawText(L"Away", 7, 35, COLOR_BLACK, 26.0f);
        KDrawText(L"Away", 5, 33, COLOR_INFO, 26.0f);
    }

    // display current kit selection

    // Home PL
    if (g_iterHomePL == g_iterHomePL_end)
    {
        KDrawText(L"P: auto", 200, 7, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 202, 5, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 202, 7, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 200, 5, COLOR_AUTO, 30.0f);
    }
    else
    {
        wchar_t wbuf[512] = {0};
        swprintf(wbuf, L"P: %s", g_iterHomePL->first.c_str());
        KDrawText(wbuf, 200, 7, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 202, 5, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 202, 7, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 200, 5, COLOR_CHOSEN, 30.0f);
        gdb->loadConfig(g_iterHomePL->second);
        if (g_iterHomePL->second.attDefined & MAIN_COLOR)
        {
            RGBAColor& c = g_iterHomePL->second.mainColor;
            KDrawText(L"\x2580", 180, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 178, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterHomePL->second.foldername == L"")
        {
            // non-GDB kit => get main color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->home->teamId);
            if (pNM->home->teamIdSpecial != pNM->home->teamId)
                tki = &pNM->home->tki;

            KCOLOR kc = (g_iterHomePL->first == L"pa") ? tki->pa.mainColor : tki->pb.mainColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2580", 180, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 178, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        if (g_iterHomePL->second.attDefined & SHORTS_MAIN_COLOR)
        {
            RGBAColor& c = g_iterHomePL->second.shortsFirstColor;
            KDrawText(L"\x2584", 180, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 178, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterHomePL->second.foldername == L"")
        {
            // non-GDB kit => get shorts color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->home->teamId);
            if (pNM->home->teamIdSpecial != pNM->home->teamId)
                tki = &pNM->home->tki;

            KCOLOR kc = (g_iterHomePL->first == L"pa") ? tki->pa.shortsFirstColor : tki->pb.shortsFirstColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2584", 180, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 178, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
    }
    */
}

void stadKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam)
{
    /*
	if (code1 >= 0 && code1==HC_ACTION && lParam & 0x80000000) {
        if (wParam == 0x31) { // home PL
            if (g_iterHomePL == g_iterHomePL_end)
                g_iterHomePL = g_iterHomePL_begin;
            else
                g_iterHomePL++;
        }
        else if (wParam == 0x32) { // away PL
            if (g_iterAwayPL == g_iterAwayPL_end)
                g_iterAwayPL = g_iterAwayPL_begin;
            else
                g_iterAwayPL++;
        }
        else if (wParam == 0x33) { // home GK
            if (g_iterHomeGK == g_iterHomeGK_end)
                g_iterHomeGK = g_iterHomeGK_begin;
            else
                g_iterHomeGK++;
        }
        else if (wParam == 0x34) { // away GK
            if (g_iterAwayGK == g_iterAwayGK_end)
                g_iterAwayGK = g_iterAwayGK_begin;
            else
                g_iterAwayGK++;
        }
    }	
    */
}

