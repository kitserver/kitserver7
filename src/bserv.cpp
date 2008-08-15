/* bserv module */
#define UNICODE
#define THISMOD &k_bserv

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afsio.h"
#include "bserv.h"
#include "bserv_addr.h"
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

#define lang(s) getTransl("bserv",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

//#define CREATE_FLAGS FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING
#define CREATE_FLAGS 0

#define MAX_ITERATIONS 5000

#define FIRST_BALL 2
#define LAST_BALL 19

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_bserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void bservConfig(char* pName, const void* pValue, DWORD a);
bool bservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
bool IsBallFile(DWORD binId);
void bservOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode);
void bservKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam);
void bservPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void bservReadReplayData(LPCVOID data, DWORD size);
void bservWriteReplayData(LPCVOID data, DWORD size);
void bservInitMaps();
void bservGetBallNameCallPoint1();
void bservGetBallNameCallPoint2();
void bservGetBallNameCallPoint3();
KEXPORT char* bservGetBallName(char* orgName);
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

char _ball_name[1024];
bool _ball_bins[726];
DWORD _ball_names_base = 0;

static int _myPage = -1;
static bool g_presentHooked = false;

class bserv_config_t
{
public:
    bserv_config_t() : _home_balls_enabled(true), _auto_random(false) {}
    bool _home_balls_enabled;
    bool _auto_random;
};

bserv_config_t _bserv_config;

class ball_t
{
public:
    wstring _file;
    wstring _description;
};

// main map of balls
map<wstring,ball_t> _balls;

// map of home balls
typedef map<wstring,ball_t>::iterator ball_iter_t;
map<WORD,ball_iter_t> _home_balls;

// iterators
ball_iter_t _ball_iter = _balls.end();

const wchar_t GetFirstLetter(ball_iter_t iter);

void bservInitMaps()
{
	WIN32_FIND_DATA fData;
    wstring pattern(getPesInfo()->gdbDir);
    pattern += L"GDB\\balls\\*";

	HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
    if (hff != INVALID_HANDLE_VALUE)
    {
        while(true)
        {
            // check if this is a file
            if ((fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                    && wcsicmp(fData.cFileName,L"map.txt")!=0)
            {
                ball_t ball;
                ball._file = fData.cFileName;

                if (ball._file.size() > 0x2f)
                {
                    LOG1S1N(L"ERROR: Ball filename too long for {%s}: %d characters. Must be 0-47. Skipping.", ball._file.c_str(), ball._file.size());
                }
                else
                {
                    _balls.insert(pair<wstring,ball_t>(
                                fData.cFileName, ball
                    ));
                }
            }

            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);
    }

    // read home-ball map
    hash_map<WORD,wstring> entries;
    wstring mapFile(getPesInfo()->gdbDir);
    mapFile += L"GDB\\balls\\map.txt";
    if (readMap(mapFile.c_str(), entries))
    {
        for (hash_map<WORD,wstring>::iterator it = entries.begin();
                it != entries.end();
                it++)
        {
            wstring dir(it->second);
            string_strip(dir);
            string_strip_quotes(dir);

            // find this ball in the normal map
            ball_iter_t sit = _balls.find(dir);
            if (sit != _balls.end())
                _home_balls.insert(pair<WORD,ball_iter_t>(
                            it->first, sit));
            else
                LOG1S(L"ERROR in home-balls map: ball {%s} not found. Skipping.", dir.c_str());
        }
    }

    LOG1N(L"total balls: %d", _balls.size());
    LOG1N(L"home balls assigned: %d", _home_balls.size());

    // reset the iterator
    _ball_iter = _balls.end();
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
        _myPage = addOverlayCallback(bservOverlayEvent,true);
        LOG1N(L"_myPage = %d", _myPage);

		copyAdresses();
		hookFunction(hk_D3D_CreateDevice, initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
	}
	
	return true;
}

void bservConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1: // debug
			k_bserv.debug = *(DWORD*)pValue;
			break;
        case 2: // home.enabled
            _bserv_config._home_balls_enabled = *(DWORD*)pValue == 1;
            break;
        case 3: // random.auto
            _bserv_config._auto_random = *(DWORD*)pValue == 1;
            break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

    getConfig("bserv", "debug", DT_DWORD, 1, bservConfig);
    getConfig("bserv", "home.enabled", DT_DWORD, 2, bservConfig);
    getConfig("bserv", "random.auto", DT_DWORD, 3, bservConfig);

	unhookFunction(hk_D3D_CreateDevice, initModule);
    bservInitMaps();

    // fill in BINs map
    ZeroMemory(_ball_bins, sizeof(_ball_bins));
    for (DWORD i=FIRST_BALL; i<=LAST_BALL; i++)
        _ball_bins[i] = true;


    // register callbacks
    if (_balls.size()>0)
    {
        afsioAddCallback(bservGetFileInfo);
        addKeyboardCallback(bservKeyboardEvent);
        addReadReplayDataCallback(bservReadReplayData);
        addWriteReplayDataCallback(bservWriteReplayData);

        _ball_names_base = *(DWORD*)(code[C_GET_BALL_NAME1]+3);
        HookCallPoint(code[C_GET_BALL_NAME1], 
                bservGetBallNameCallPoint1, 6, 2);
        HookCallPoint(code[C_GET_BALL_NAME2], 
                bservGetBallNameCallPoint2, 6, 2);
        HookCallPoint(code[C_GET_BALL_NAME3], 
                bservGetBallNameCallPoint3, 6, 2);
    }

    return D3D_OK;
}

bool inline IsBallFile(DWORD binId)
{
    return _ball_bins[binId];
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
bool bservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize)
{
    if (afsId != 4 || !IsBallFile(binId)) // fast check
        return false;

    if (_ball_iter != _balls.end())
    {
        wchar_t filename[1024] = {0};
        swprintf(filename,L"%sGDB\\balls\\%s", 
                getPesInfo()->gdbDir,
                _ball_iter->first.c_str());

        if (k_bserv.debug)
            LOG1N1S(L"binId=%d: Loading {%s}", binId, filename);

        return OpenFileIfExists(filename, hfile, fsize);
    }
    return false;
}

void ResetIterator()
{
    _ball_iter = _balls.end();
    GetCurrentTeams(_home,_away);

    // check for home ball
    if (_bserv_config._home_balls_enabled)
    {
        map<WORD,ball_iter_t>::iterator iter = 
            _home_balls.find(_home);
        if (iter != _home_balls.end())
            _ball_iter = iter->second;
    }

    // auto-random, unless home ball already selected
    if (_ball_iter == _balls.end() && _bserv_config._auto_random)
    {
        LARGE_INTEGER num;
        QueryPerformanceCounter(&num);
        int iterations = num.LowPart % MAX_ITERATIONS;
        ball_iter_t iter = _ball_iter;
        for (int j=0;j<iterations;j++) {
            if (iter == _balls.end()) 
                iter = _balls.begin();
            else
                iter++;
            if (iter == _balls.end()) 
                iter = _balls.begin();
        }
        _ball_iter = iter;
    }
}

void bservOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode)
{
    if (isExhibitionMode)
    {
        if (overlayOn)
        {
            hookFunction(hk_D3D_Present, bservPresent);
            setOverlayPageVisible(_myPage, true);
            TRACE(L"Showing ball selection");
            g_presentHooked = true;

            if (delta==1) 
                ResetIterator();
        }
        else
        {
            setOverlayPageVisible(_myPage, false);
            unhookFunction(hk_D3D_Present, bservPresent);
            TRACE(L"Hiding ball selection");
            g_presentHooked = false;

            if (delta==-1)
                ResetIterator();
        }
    }
    else
    {
        if (overlayOn)
        {
            hookFunction(hk_D3D_Present, bservPresent);
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
            unhookFunction(hk_D3D_Present, bservPresent);
            g_presentHooked = false;
        }
    }
}

void bservPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused)
{
    if (getOverlayPage() != _myPage) // page-check
        return;

	KDrawText(L"Ball:", 202, 7, COLOR_BLACK, 26.0f);
	KDrawText(L"Ball:", 200, 5, COLOR_CHOSEN, 26.0f);

    if (_ball_iter == _balls.end())
    {
        KDrawText(L"game choice", 262, 7, COLOR_BLACK, 26.0f);
        KDrawText(L"game choice", 260, 5, COLOR_AUTO, 26.0f);
    }
    else
    {
        wstring name(_ball_iter->first);

        // strip the extension, if present
        int extpos = name.find(L".");
        if (extpos != string::npos && extpos+4 == name.size())
            name = name.substr(0,extpos);

        KDrawText(name.c_str(), 262, 7, COLOR_BLACK, 26.0f);
        KDrawText(name.c_str(), 260, 5, COLOR_INFO, 26.0f);
    }
}

const wchar_t GetFirstLetter(ball_iter_t iter)
{
    return iter->first[0];
}

void bservKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam)
{
    if (getOverlayPage() != _myPage)
        return;

	if (code1 >= 0 && code1==HC_ACTION && lParam & 0x80000000) {
        if (wParam == 0x39) { // 9 - left 
            if (_ball_iter == _balls.begin())
                _ball_iter = _balls.end();
            else
                _ball_iter--;
        }
        else if (wParam == 0x30) { // 0 - right
            if (_ball_iter == _balls.end())
                _ball_iter = _balls.begin();
            else
                _ball_iter++;
        }
        else if (wParam == 0x37) { // 7 - reset to game choice
            _ball_iter = _balls.end();
        }
        else if (wParam == 0x38) { // 8 - random
            LARGE_INTEGER num;
            QueryPerformanceCounter(&num);
            int iterations = num.LowPart % MAX_ITERATIONS;
            ball_iter_t iter = _ball_iter;
            for (int j=0;j<iterations;j++) {
                if (iter == _balls.end()) 
                    iter = _balls.begin();
                else
                    iter++;
                if (iter == _balls.end()) 
                    iter = _balls.begin();
            }
            _ball_iter = iter;
        }
        else if (wParam == 0xbd) { // "-"
            // go to previous letter
            wchar_t currLetter = L'\0';
            ball_iter_t iter = _ball_iter;
            if (iter != _balls.end())
                currLetter = GetFirstLetter(iter);
            do {
                if (iter == _balls.begin())
                    iter = _balls.end();
                else
                    iter--;
                if (iter == _balls.end())
                    break;
            } while (currLetter == GetFirstLetter(iter));
            _ball_iter = iter;
        }
        else if (wParam == 0xbb) { // "="
            // go to next letter
            wchar_t currLetter = L'\0';
            ball_iter_t iter = _ball_iter;
            if (iter != _balls.end())
                currLetter = GetFirstLetter(iter);
            do {
                if (iter == _balls.end())
                    iter = _balls.begin();
                else
                    iter++;
                if (iter == _balls.end())
                    break;
            } while (currLetter == GetFirstLetter(iter));
            _ball_iter = iter;
        }
    }	
}

/**
 * read data callback
 */
void bservReadReplayData(LPCVOID data, DWORD size)
{
    REPLAY_DATA* replay = (REPLAY_DATA*)data;

    wstring ballKey((wchar_t*)((BYTE*)data+0x377cf0));
    LOG1S(L"read ballKey = {%s}", ballKey.c_str());

    _ball_iter = _balls.end();
    if (!ballKey.empty())
    {
        ball_iter_t it = _balls.find(ballKey);
        if (it != _balls.end())
            _ball_iter = it;
    }
}

/**
 * write data callback
 */
void bservWriteReplayData(LPCVOID data, DWORD size)
{
    REPLAY_DATA* replay = (REPLAY_DATA*)data;
    if (_ball_iter != _balls.end())
    {
        wcsncpy((wchar_t*)((BYTE*)data+0x377cf0), 
                _ball_iter->first.c_str(),
                0x30);

        LOG1S(L"written ballKey = {%s}", _ball_iter->first.c_str());
    }
}

void bservGetBallNameCallPoint1()
{
    __asm {
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        shl eax,2
        add eax,_ball_names_base
        mov eax,[eax]
        push eax
        call bservGetBallName
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

void bservGetBallNameCallPoint2()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push edx
        push esi
        push edi
        mov ecx,eax
        shl ecx,2
        add ecx,_ball_names_base
        mov ecx,[ecx]
        push ecx
        call bservGetBallName
        add esp,4     // pop parameters
        mov ecx,eax
        pop edi
        pop esi
        pop edx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void bservGetBallNameCallPoint3()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push esi
        push edi
        mov edx,ecx
        shl edx,2
        add edx,_ball_names_base
        mov edx,[edx]
        push edx
        call bservGetBallName
        add esp,4     // pop parameters
        mov edx,eax
        pop edi
        pop esi
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

KEXPORT char* bservGetBallName(char* orgName)
{
    if (_ball_iter != _balls.end())
    {
        wstring name(_ball_iter->first);

        // strip the extension, if present
        int extpos = name.find(L".");
        if (extpos != string::npos && extpos+4 == name.size())
            name = name.substr(0,extpos);

        // convert bserv dir(or name) to ascii
        Utf8::fUnicodeToAnsi(_ball_name, name.c_str());
        return _ball_name;
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

