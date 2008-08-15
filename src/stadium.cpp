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
#include "configs.h"
#include "replay.h"

#define COLOR_BLACK D3DCOLOR_RGBA(0,0,0,128)
#define COLOR_AUTO D3DCOLOR_RGBA(135,135,135,255)
#define COLOR_AUTO2 D3DCOLOR_RGBA(170,170,170,255)
#define COLOR_CHOSEN D3DCOLOR_RGBA(210,210,210,255)
#define COLOR_INFO D3DCOLOR_RGBA(0xb0,0xff,0xb0,0xff)

#define lang(s) getTransl("stadium",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

//#define CREATE_FLAGS FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING
#define CREATE_FLAGS 0

#define MAX_ITERATIONS 5000

enum {
    SPECTATORS_SUMMER, SPECTATORS_WINTER, FLAGS,
    ADBOARDS_1, ADBOARDS_2, GOALS,
    DAY_SUMMER_0, DAY_SUMMER_1,
    DAY_SUMMER_2, DAY_SUMMER_3,
    DAY_SUMMER_MESH, DAY_SUMMER_5, DAY_SUMMER_TURF,
    DAY_WINTER_0, DAY_WINTER_1,
    DAY_WINTER_2, DAY_WINTER_3,
    DAY_WINTER_MESH, DAY_WINTER_5, DAY_WINTER_TURF,
    EVENING_SUMMER_0, EVENING_SUMMER_1,
    EVENING_SUMMER_2, EVENING_SUMMER_3,
    EVENING_SUMMER_MESH, EVENING_SUMMER_5, EVENING_SUMMER_TURF, 
    EVENING_WINTER_0, EVENING_WINTER_1,
    EVENING_WINTER_2, EVENING_WINTER_3,
    EVENING_WINTER_MESH, EVENING_WINTER_5, EVENING_WINTER_TURF,
    NIGHT_SUMMER_0, NIGHT_SUMMER_1,
    NIGHT_SUMMER_2, NIGHT_SUMMER_3,
    NIGHT_SUMMER_MESH, NIGHT_SUMMER_5, NIGHT_SUMMER_TURF,
    NIGHT_WINTER_0, NIGHT_WINTER_1,
    NIGHT_WINTER_2, NIGHT_WINTER_3,
    NIGHT_WINTER_MESH, NIGHT_WINTER_5, NIGHT_WINTER_TURF,
};

wstring _stad_names[] = {
    L"spec_summer.bin", L"spec_winter.bin", L"stad_flags.bin",
    L"stad_adboards1.bin", L"stad_adboards2.bin", L"stad_goals.bin",
    L"", L"",
    L"", L"",
    L"day_summer_mesh.bin", L"", L"day_summer_turf.bin",
    L"", L"",
    L"", L"",
    L"day_winter_mesh.bin", L"", L"day_winter_turf.bin",
    L"", L"",
    L"", L"",
    L"evening_summer_mesh.bin", L"", L"evening_summer_turf.bin",
    L"", L"",
    L"", L"",
    L"evening_winter_mesh.bin", L"", L"evening_winter_turf.bin",
    L"", L"",
    L"", L"",
    L"night_summer_mesh.bin", L"", L"night_summer_turf.bin",
    L"", L"",
    L"", L"",
    L"night_winter_mesh.bin", L"", L"night_winter_turf.bin",
};

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_stad = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

#define NUM_STADIUMS 16
#define NUM_STAD_FILES 48

#define STAD_FIRST  41
#define STAD_LAST   712
#define STAD_SPAN   42

#define STAD_SPEC_SUMMER 35
#define STAD_SPEC_WINTER 36
#define STAD_ADBOARDS_1  38
#define STAD_ADBOARDS_2  39
#define STAD_GOALS       40
#define STAD_FLAGS       713

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void stadInfo(char* pName, const void* pValue, DWORD a);
void stadConfig(char* pName, const void* pValue, DWORD a);
bool stadGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
bool IsStadiumFile(DWORD binId);
int GetBinType(DWORD binId);
void stadOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode);
void stadKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam);
void stadPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void stadReadReplayData(LPCVOID data, DWORD size);
void stadWriteReplayData(LPCVOID data, DWORD size);
void stadInitMaps();
void stadGetStadiumNameCallPoint();
KEXPORT char* stadGetStadiumName(char* orgName);
void ResetIterator();
void GetCurrentTeams(WORD& home, WORD& away);

static void string_strip(wstring& s)
{
    static const wchar_t* empties = L" \t\n\r";
    int e = s.find_last_not_of(empties);
    s.erase(e + 1);
    int b = s.find_first_not_of(empties);
    s.erase(0,b);
}

static void string_strip_quotes(wstring& s)
{
    static const wchar_t* empties = L" \t\n\r";
    if (s[s.length()-1]=='"')
        s.erase(s.length()-1);
    if (s[0]=='"')
        s.erase(0,1);
}

// GLOBALS
WORD _home = 0xffff;
WORD _away = 0xffff;

char _stadium_name[1024];
bool _stadium_bins[726];

static int _myPage = -1;
static bool g_presentHooked = false;

class stadium_config_t
{
public:
    stadium_config_t() : _home_stadiums_enabled(true) {}
    bool _home_stadiums_enabled;
};

stadium_config_t _stadium_config;

class stadium_t
{
public:
    wstring _files[NUM_STAD_FILES];
    wstring _dir;
    wstring _name;
    int _capacity;
    int _built;
    bool _infoLoaded;

    stadium_t() : _capacity(0), _built(0), _infoLoaded(false) {}
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
                            if (0 <= t && t < NUM_STAD_FILES)
                                _files[t] = fData.cFileName;
                        }
                    }

                    // Format B: check for standard name
                    if (binId == -1)
                    {
                        wstring name(fData.cFileName);
                        for (int i=0; i<NUM_STAD_FILES; i++)
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

        // we'll parse the info file later, when needed
        //this->_parseInfoFile();

        LOG1S(L"initialized stadium: %s", _dir.c_str());
        return true;
    }

    void _parseInfoFile()
    {
        wstring infoFile(getPesInfo()->gdbDir);
        infoFile += L"GDB\\stadiums\\" + _dir + L"\\info.txt";

        //LOG1S(L"readConfig(%s)...", infoFile.c_str());
        if (readConfig(infoFile.c_str()))
        {
            LOG1S(L"info.txt successfully loaded for {%s}", _dir.c_str());
            _getConfig("", "name", DT_STRING, (DWORD)this, stadInfo);
            _getConfig("", "built", DT_DWORD, (DWORD)this, stadInfo);
            _getConfig("", "capacity", DT_DWORD, (DWORD)this, stadInfo);
        }
    }

    void loadInfo()
    {
        if (!_infoLoaded)
            _parseInfoFile();
        _infoLoaded = true;
    }
};

// main map of stadiums
map<wstring,stadium_t> _stadiums;

// map of home stadiums
typedef map<wstring,stadium_t>::iterator stadium_iter_t;
map<WORD,stadium_iter_t> _home_stadiums;

// iterators
stadium_iter_t _stadium_iter = _stadiums.end();

const wchar_t GetFirstLetter(stadium_iter_t iter);

void stadInitMaps()
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

    // read home-stadium map
    hash_map<WORD,wstring> entries;
    wstring mapFile(getPesInfo()->gdbDir);
    mapFile += L"GDB\\stadiums\\map.txt";
    if (readMap(mapFile.c_str(), entries))
    {
        for (hash_map<WORD,wstring>::iterator it = entries.begin();
                it != entries.end();
                it++)
        {
            wstring dir(it->second);
            string_strip(dir);
            string_strip_quotes(dir);

            // find this stadium in the normal map
            stadium_iter_t sit = _stadiums.find(dir);
            if (sit != _stadiums.end())
                _home_stadiums.insert(pair<WORD,stadium_iter_t>(
                            it->first, sit));
            else
                LOG1S(L"ERROR in home-stadiums map: stadium {%s} not found. Skipping.", dir.c_str());
        }
    }

    LOG1N(L"total stadiums: %d", _stadiums.size());
    LOG1N(L"home stadiums assigned: %d", _home_stadiums.size());

    // reset the iterator
    _stadium_iter = _stadiums.end();
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
        case 2: // home.enabled
            _stadium_config._home_stadiums_enabled = *(DWORD*)pValue == 1;
            break;
	}
	return;
}

void stadInfo(char* pName, const void* pValue, DWORD a)
{
    stadium_t* stadium = (stadium_t*)a;
    if (strcmp(pName,"[]name")==0)
        stadium->_name = (wchar_t*)pValue;
    else if (strcmp(pName,"[]built")==0)
        stadium->_built = *(DWORD*)pValue;
    else if (strcmp(pName,"[]capacity")==0)
        stadium->_capacity = *(DWORD*)pValue;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

    getConfig("stadium", "debug", DT_DWORD, 1, stadConfig);
    getConfig("stadium", "home.enabled", DT_DWORD, 2, stadConfig);

	unhookFunction(hk_D3D_CreateDevice, initModule);
    stadInitMaps();

    // fill in BINs map
    ZeroMemory(_stadium_bins, sizeof(_stadium_bins));
    for (DWORD i=0; i<726; i++)
        if (GetBinType(i)!=-1)
            _stadium_bins[i] = true;


    // register callbacks
    afsioAddCallback(stadGetFileInfo);
    addKeyboardCallback(stadKeyboardEvent);
    addReadReplayDataCallback(stadReadReplayData);
    addWriteReplayDataCallback(stadWriteReplayData);

    HookCallPoint(code[C_GET_STADIUM_NAME], 
            stadGetStadiumNameCallPoint, 6, 0, true);

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
        case STAD_SPEC_SUMMER:
            return SPECTATORS_SUMMER;
        case STAD_SPEC_WINTER:
            return SPECTATORS_WINTER;
        case STAD_ADBOARDS_1:
            return ADBOARDS_1;
        case STAD_ADBOARDS_2:
            return ADBOARDS_2;
        case STAD_GOALS:
            return GOALS;
        case STAD_FLAGS:
            return FLAGS;
    }

    if (binId < STAD_FIRST || binId > STAD_LAST)
        return -1;  // not a stadium file

    switch ((binId - STAD_FIRST) % STAD_SPAN)
    {
        case 0:  return DAY_SUMMER_0;
        case 1:  return DAY_SUMMER_1;
        case 2:  return DAY_SUMMER_2;
        case 3:  return DAY_SUMMER_3;
        case 4:  return DAY_SUMMER_MESH;
        case 5:  return DAY_SUMMER_5;
        case 6:  return DAY_SUMMER_TURF; 

        case 7:  return DAY_WINTER_0;
        case 8:  return DAY_WINTER_1;
        case 9:  return DAY_WINTER_2;
        case 10: return DAY_WINTER_3;
        case 11: return DAY_WINTER_MESH;
        case 12: return DAY_WINTER_5; 
        case 13: return DAY_WINTER_TURF; 

        case 14: return EVENING_SUMMER_0;
        case 15: return EVENING_SUMMER_1;
        case 16: return EVENING_SUMMER_2;
        case 17: return EVENING_SUMMER_3;
        case 18: return EVENING_SUMMER_MESH;
        case 19: return EVENING_SUMMER_5;
        case 20: return EVENING_SUMMER_TURF; 

        case 21: return EVENING_WINTER_0;
        case 22: return EVENING_WINTER_1;
        case 23: return EVENING_WINTER_2;
        case 24: return EVENING_WINTER_3;
        case 25: return EVENING_WINTER_MESH;
        case 26: return EVENING_WINTER_5;
        case 27: return EVENING_WINTER_TURF; 

        case 28: return NIGHT_SUMMER_0;
        case 29: return NIGHT_SUMMER_1;
        case 30: return NIGHT_SUMMER_2;
        case 31: return NIGHT_SUMMER_3;
        case 32: return NIGHT_SUMMER_MESH;
        case 33: return NIGHT_SUMMER_5;
        case 34: return NIGHT_SUMMER_TURF; 

        case 35: return NIGHT_WINTER_0;
        case 36: return NIGHT_WINTER_1;
        case 37: return NIGHT_WINTER_2;
        case 38: return NIGHT_WINTER_3;
        case 39: return NIGHT_WINTER_MESH;
        case 40: return NIGHT_WINTER_5;
        case 41: return NIGHT_WINTER_TURF; 
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

    if (_stadium_iter != _stadiums.end())
    {
        wstring& fname = _stadium_iter->second._files[GetBinType(binId)];
        if (!fname.empty())
        {
            wchar_t filename[1024] = {0};
            swprintf(filename,L"%sGDB\\stadiums\\%s\\%s", 
                    getPesInfo()->gdbDir,
                    _stadium_iter->first.c_str(),
                    fname.c_str());

            if (k_stad.debug)
                LOG1N1S(L"binId=%d: Loading {%s}", binId, filename);

            return OpenFileIfExists(filename, hfile, fsize);
        }
    }
    return false;
}

void ResetIterator()
{
    _stadium_iter = _stadiums.end();
    GetCurrentTeams(_home,_away);

    // check for home stadium
    if (_stadium_config._home_stadiums_enabled)
    {
        map<WORD,stadium_iter_t>::iterator iter = 
            _home_stadiums.find(_home);
        if (iter != _home_stadiums.end())
            _stadium_iter = iter->second;
    }
}

void stadOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode)
{
    if (isExhibitionMode)
    {
        if (overlayOn)
        {
            hookFunction(hk_D3D_Present, stadPresent);
            setOverlayPageVisible(_myPage, true);
            TRACE(L"Showing stadium selection");
            g_presentHooked = true;

            if (delta==1) 
                ResetIterator();
        }
        else
        {
            setOverlayPageVisible(_myPage, false);
            unhookFunction(hk_D3D_Present, stadPresent);
            TRACE(L"Hiding stadium selection");
            g_presentHooked = false;

            if (delta==-1)
                ResetIterator();
        }
    }
    else
    {
        if (overlayOn)
        {
            hookFunction(hk_D3D_Present, stadPresent);
            setOverlayPageVisible(_myPage, true);
            g_presentHooked = true;

            WORD home,away;
            GetCurrentTeams(home,away);
            if (home != _home || away != _away)
                ResetIterator();
            _home = home;
            _away = away;
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

	KDrawText(L"Stadium:", 202, 7, COLOR_BLACK, 26.0f);
	KDrawText(L"Stadium:", 200, 5, COLOR_CHOSEN, 26.0f);

    if (_stadium_iter == _stadiums.end())
    {
        KDrawText(L"game choice", 302, 7, COLOR_BLACK, 26.0f);
        KDrawText(L"game choice", 300, 5, COLOR_AUTO, 26.0f);
    }
    else
    {
        // use directory name
        KDrawText(_stadium_iter->first.c_str(), 302, 7, COLOR_BLACK, 26.0f);
        KDrawText(_stadium_iter->first.c_str(), 300, 5, COLOR_INFO, 26.0f);

        _stadium_iter->second.loadInfo();
        if (_stadium_iter->second._built)
        {
            wchar_t buf[20];
            swprintf(buf,L"built: %d",_stadium_iter->second._built);
            KDrawText(buf, 302, 35, COLOR_BLACK, 26.0f);
            KDrawText(buf, 300, 33, COLOR_AUTO2, 26.0f);
        }
        if (_stadium_iter->second._capacity)
        {
            wchar_t buf[20];
            swprintf(buf,L"capacity: %d",_stadium_iter->second._capacity);
            KDrawText(buf, 442, 35, COLOR_BLACK, 26.0f);
            KDrawText(buf, 440, 33, COLOR_AUTO2, 26.0f);
        }
    }
}

const wchar_t GetFirstLetter(stadium_iter_t iter)
{
    return iter->first[0];
}

void stadKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam)
{
    if (getOverlayPage() != _myPage)
        return;

	if (code1 >= 0 && code1==HC_ACTION && lParam & 0x80000000) {
        if (wParam == 0x39) { // 9 - left 
            if (_stadium_iter == _stadiums.begin())
                _stadium_iter = _stadiums.end();
            else
                _stadium_iter--;
        }
        else if (wParam == 0x30) { // 0 - right
            if (_stadium_iter == _stadiums.end())
                _stadium_iter = _stadiums.begin();
            else
                _stadium_iter++;
        }
        else if (wParam == 0x37) { // 7 - reset to game choice
            _stadium_iter = _stadiums.end();
        }
        else if (wParam == 0x38) { // 8 - random
            LARGE_INTEGER num;
            QueryPerformanceCounter(&num);
            int iterations = num.LowPart % MAX_ITERATIONS;
            stadium_iter_t iter = _stadium_iter;
            for (int j=0;j<iterations;j++) {
                if (iter == _stadiums.end()) 
                    iter = _stadiums.begin();
                else
                    iter++;
                if (iter == _stadiums.end()) 
                    iter = _stadiums.begin();
            }
            _stadium_iter = iter;
        }
        else if (wParam == 0xbd) { // "-"
            // go to previous letter
            wchar_t currLetter = L'\0';
            stadium_iter_t iter = _stadium_iter;
            if (iter != _stadiums.end())
                currLetter = GetFirstLetter(iter);
            do {
                if (iter == _stadiums.begin())
                    iter = _stadiums.end();
                else
                    iter--;
                if (iter == _stadiums.end())
                    break;
            } while (currLetter == GetFirstLetter(iter));
            _stadium_iter = iter;
        }
        else if (wParam == 0xbb) { // "="
            // go to next letter
            wchar_t currLetter = L'\0';
            stadium_iter_t iter = _stadium_iter;
            if (iter != _stadiums.end())
                currLetter = GetFirstLetter(iter);
            do {
                if (iter == _stadiums.end())
                    iter = _stadiums.begin();
                else
                    iter++;
                if (iter == _stadiums.end())
                    break;
            } while (currLetter == GetFirstLetter(iter));
            _stadium_iter = iter;
        }
    }	
}

/**
 * read data callback
 */
void stadReadReplayData(LPCVOID data, DWORD size)
{
    REPLAY_DATA* replay = (REPLAY_DATA*)data;

    //wstring stadKey((wchar_t*)((BYTE*)data+0x377cf0));
    wstring stadKey;
    char* start;
    int capacity = 0;
    for (int i=0; i<22; i++)
    {
        int taken1 = strlen(replay->payload.players[i].name);
        capacity = (0x2e-taken1-4)/2;
        if (capacity > 0)
        {
            start = replay->payload.players[i].name + taken1 + 2;
            wstring frag((wchar_t*)start);
            stadKey += frag;
        }
        int taken2 = strlen(replay->payload.players[i].nameOnShirt);
        capacity = (0x10-taken2-4)/2;
        if (capacity > 0)
        {
            start = replay->payload.players[i].nameOnShirt + taken2 + 2;
            wstring frag((wchar_t*)start);
            stadKey += frag;
        }
    }
    LOG1S(L"read stadKey = {%s}", stadKey.c_str());

    _stadium_iter = _stadiums.end();
    if (!stadKey.empty())
    {
        stadium_iter_t it = _stadiums.find(stadKey);
        if (it != _stadiums.end())
            _stadium_iter = it;
    }
}

/**
 * write data callback
 */
void stadWriteReplayData(LPCVOID data, DWORD size)
{
    REPLAY_DATA* replay = (REPLAY_DATA*)data;
    if (_stadium_iter != _stadiums.end())
    {
        int wchars = 0, capacity = 0;
        for (int i=0; i<22; i++)
        {
            int taken1 = strlen(replay->payload.players[i].name);
            capacity = (0x2e-taken1-4)/2;
            if (capacity > 0)
                wchars += capacity;

            int taken2 = strlen(replay->payload.players[i].nameOnShirt);
            capacity = (0x10-taken1-4)/2;
            if (capacity > 0)
                wchars += capacity;
        }

        if (_stadium_iter->first.size() <= wchars)
        {
            //wcsncpy((wchar_t*)((BYTE*)data+0x377cf0), 
            //        _stadium_iter->first.c_str(),
            //        0x30);

            char* start;
            int j = 0, k = 0;
            for (int i=0; i<22; i++)
            {
                if (j >= _stadium_iter->first.size())
                    break;

                int taken1 = strlen(replay->payload.players[i].name);
                start = replay->payload.players[i].name + taken1 + 2;
                capacity = (0x2e-taken1-4)/2;
                if (capacity > 0)
                {
                    wstring frag = _stadium_iter->first.substr(j,capacity);
                    wcsncpy((wchar_t*)start, frag.c_str(), capacity);
                    j += capacity;
                }

                if (j >= _stadium_iter->first.size())
                    break;

                int taken2 = strlen(replay->payload.players[i].nameOnShirt);
                start = replay->payload.players[i].nameOnShirt + taken2 + 2;
                capacity = (0x10-taken2-4)/2;
                if (capacity > 0)
                {
                    wstring frag = _stadium_iter->first.substr(j,capacity);
                    wcsncpy((wchar_t*)start, frag.c_str(), capacity);
                    j += capacity;
                }               
            }
            LOG1S(L"written stadKey = {%s}", _stadium_iter->first.c_str());
        }
        else
            LOG2N(L"WARNING: Not enough capacity to write STADIUM info in the replay data. Stadium key size: %d chars (Capacity: %d)", _stadium_iter->first.size(), wchars);
    }
}

void stadGetStadiumNameCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push eax // parameter: stadium name ptr
        call stadGetStadiumName
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop ebp
        popfd
        retn
    }
}

KEXPORT char* stadGetStadiumName(char* orgName)
{
    if (_stadium_iter != _stadiums.end())
    {
        _stadium_iter->second.loadInfo();

        // convert stadium dir(or name) to ascii
        if (_stadium_iter->second._name.empty())
            Utf8::fUnicodeToAnsi(_stadium_name, 
                    (wchar_t*)_stadium_iter->second._dir.c_str());
        else
            Utf8::fUnicodeToAnsi(_stadium_name, 
                    (wchar_t*)_stadium_iter->second._name.c_str());

        return _stadium_name;
    }
    return orgName;
}

void GetCurrentTeams(WORD& home, WORD& away)
{
    NEXT_MATCH_DATA_INFO* pNM = 
        *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
    if (pNM && pNM->home) home = pNM->home->teamId;
    if (pNM && pNM->away) away = pNM->away->teamId;
}

