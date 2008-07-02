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

#define NUM_STADIUMS 16

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


// GLOBALS
char _stadium_name[1024];

bool _stadium_bins[726];

static int _myPage = -1;
static bool g_presentHooked = false;

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
        wstring infoFile(getPesInfo()->gdbDir);
        infoFile += L"GDB\\stadiums\\" + _dir + L"\\info.txt";

        //LOG1S(L"readConfig(%s)...", infoFile.c_str());
        if (readConfig(infoFile.c_str()))
        {
            //LOG(L"readConfig() SUCCEEDED.");
            _getConfig("", "name", DT_STRING, (DWORD)this, stadInfo);
            _getConfig("", "built", DT_DWORD, (DWORD)this, stadInfo);
            _getConfig("", "capacity", DT_DWORD, (DWORD)this, stadInfo);
        }
    }
};

// main map of stadiums
map<wstring,stadium_t> _stadiums;

// map of home stadiums
typedef map<wstring,stadium_t>::iterator stadium_iter_t;
map<WORD,stadium_iter_t> _home_stadiums;

// iterators
stadium_iter_t _stadium_iter = _stadiums.end();

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
        if (_stadium_iter->second._name.empty())
        {
            // use directory name
            KDrawText(_stadium_iter->first.c_str(), 302, 7, COLOR_BLACK, 26.0f);
            KDrawText(_stadium_iter->first.c_str(), 300, 5, COLOR_INFO, 26.0f);
        }
        else
        {
            // use name from info.txt
            KDrawText(_stadium_iter->second._name.c_str(), 302, 7, COLOR_BLACK, 26.0f);
            KDrawText(_stadium_iter->second._name.c_str(), 300, 5, COLOR_INFO, 26.0f);
        }

        if (_stadium_iter->second._built)
        {
            wchar_t buf[20];
            swprintf(buf,L"built: %d",_stadium_iter->second._built);
            KDrawText(buf, 202, 35, COLOR_BLACK, 24.0f);
            KDrawText(buf, 200, 33, COLOR_AUTO2, 24.0f);
        }
        if (_stadium_iter->second._capacity)
        {
            wchar_t buf[20];
            swprintf(buf,L"capacity: %d",_stadium_iter->second._capacity);
            KDrawText(buf, 352, 35, COLOR_BLACK, 26.0f);
            KDrawText(buf, 350, 33, COLOR_AUTO2, 26.0f);
        }
    }
}

void stadKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam)
{
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

