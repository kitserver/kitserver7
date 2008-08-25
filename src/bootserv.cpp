/* <bootserv>
 *
 */
#define UNICODE
#define THISMOD &k_bootserv

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afsio.h"
#include "bootserv.h"
#include "bootserv_addr.h"
#include "dllinit.h"
#include "pngdib.h"
#include "configs.h"
#include "configs.hpp"
#include "utf8.h"
#include "replay.h"
#include "player.h"
#include "teaminfo.h"

#define lang(s) getTransl("bootserv",s)

#include <map>
#include <list>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

//#define CREATE_FLAGS FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING
#define CREATE_FLAGS 0

#define FIRST_BOOT_SLOT 7493
#define LAST_BOOT_SLOT 7508

#define FIRST_EXTRA_BOOT_SLOT 9000
#define FIRST_RANDOM_BOOT_SLOT 14500
#define LAST_EXTRA_BOOT_SLOT 19999
#define MAX_PLAYERS 5460
#define MAX_BOOTS MAX_PLAYERS

#define NETWORK_MODE 4

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_bootserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

class bootserv_config_t
{
public:
    bool _enable_online;
    bool _random_boots;
    wstring _random_skip;
    bootserv_config_t() : 
        _enable_online(false),
        _random_boots(false)
    {}
};

bootserv_config_t _bootserv_config;

// GLOBALS
hash_map<DWORD,WORD> _boot_slots;
WORD _player_boot_slots[MAX_PLAYERS];
wstring* _fast_bin_table[LAST_EXTRA_BOOT_SLOT - FIRST_EXTRA_BOOT_SLOT + 1];

bool _struct_replaced = false;
bool _fast_lookup_initialized = false;
int _num_slots = 8094;
int _num_random_boots = 0;
bool _random_mapped = false;
DWORD _last_playerIndex = 0;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void bootservConfig(char* pName, const void* pValue, DWORD a);
bool bootservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size);
void InitMaps();
void EnumerateBoots(wstring dir, int& count);
void bootservCopyPlayerData(PLAYER_INFO* players, int place, bool writeList);
void bootservWriteEditData(LPCVOID data, DWORD size);
void bootservOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode);
void ReRandomizeBoots();
void GetCurrentTeams(WORD& home, WORD& away);

void bootservAtGetBootIdCallPointA();
void bootservAtGetBootIdCallPointB();
void bootservAtGetBootIdCallPointC();
DWORD GetIdByPlayerIndex(DWORD idx);
WORD GetIndexByPlayerId(DWORD id);
void bootservAtProcessBootIdCallPoint();
void bootservEntranceBootsCallPoint();
KEXPORT void bootservEntranceBoots(BYTE* pData);

KEXPORT DWORD bootservGetBootId1(DWORD boot, BYTE* pPartialPlayerInfo);
KEXPORT DWORD bootservGetBootId2(DWORD boot);

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


/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;

		RegisterKModule(THISMOD);
        ZeroMemory(_fast_bin_table, sizeof(_fast_bin_table));
        ZeroMemory(_player_boot_slots, sizeof(_player_boot_slots));
        LOG1N(L"sizeof(_fast_bin_table) = %d", sizeof(_fast_bin_table));

		if (!checkGameVersion()) {
			LOG(L"Sorry, your game version isn't supported!");
			return false;
		}

        CHECK_KLOAD(MAKELONG(4,7));

		copyAdresses();
		hookFunction(hk_D3D_CreateDevice, initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
        for (int i=0; i<MAX_BOOTS; i++)
            if (_fast_bin_table[i])
                delete _fast_bin_table[i];
	}
	
	return true;
}

void bootservConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1: // debug
			k_bootserv.debug = *(DWORD*)pValue;
			break;
        case 2: // disable-online
            _bootserv_config._enable_online = *(DWORD*)pValue == 1;
            break;
        case 3: // random boots
            _bootserv_config._random_boots = *(DWORD*)pValue == 1;
            break;
        case 4: // random.skip
            _bootserv_config._random_skip = (wchar_t*)pValue;
            if (!_bootserv_config._random_skip.empty())
            {
                _bootserv_config._random_skip += L"\\";

                // normalize the path
                wstring skipPath(getPesInfo()->gdbDir);
                skipPath += L"GDB\\boots\\";
                skipPath += _bootserv_config._random_skip;

                wchar_t fullname[MAX_PATH];
                GetFullPathName(skipPath.c_str(), MAX_PATH, fullname, 0);
                _bootserv_config._random_skip = fullname;
            }
            break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

	unhookFunction(hk_D3D_CreateDevice, initModule);

    LOG(L"Initializing Bootserver");

    getConfig("bootserv", "debug", DT_DWORD, 1, bootservConfig);
    getConfig("bootserv", "online.enabled", DT_DWORD, 2, bootservConfig);
    getConfig("bootserv", "random.enabled", DT_DWORD, 3, bootservConfig);
    getConfig("bootserv", "random.skip", DT_STRING, 4, bootservConfig);

    HookCallPoint(code[C_GET_BOOT_ID1] + 4, 
            bootservAtGetBootIdCallPointA, 6, 14);
    HookCallPoint(code[C_GET_BOOT_ID2] + 4, 
            bootservAtGetBootIdCallPointA, 6, 14);
    HookCallPoint(code[C_GET_BOOT_ID3] + 3, 
            bootservAtGetBootIdCallPointB, 6, 13);
    HookCallPoint(code[C_GET_BOOT_ID4] + 3, 
            bootservAtGetBootIdCallPointC, 6, 19);
    HookCallPoint(code[C_GET_BOOT_ID5] + 4, 
            bootservAtGetBootIdCallPointA, 6, 15);
    HookCallPoint(code[C_GET_BOOT_ID6] + 4, 
            bootservAtGetBootIdCallPointA, 6, 14);

    HookCallPoint(code[C_PROCESS_BOOT_ID],
            bootservAtProcessBootIdCallPoint, 3, 1);
    HookCallPoint(code[C_ENTRANCE_BOOTS],
            bootservEntranceBootsCallPoint, 6, 1);

    // initialize boots map
    InitMaps();

    // register callbacks
    afsioAddCallback(bootservGetFileInfo);
    addCopyPlayerDataCallback(bootservCopyPlayerData);
    addWriteEditDataCallback(bootservWriteEditData);

    // Random boots on each match doesn't yet work well,
    // because we need to make sure the boots aren't re-mapped
    // in the middle of boot loading operation!
    //
    //if (_bootserv_config._random_boots)
    //    addOverlayCallback(bootservOverlayEvent,false);

	TRACE(L"Hooking done.");
    return D3D_OK;
}

void InitMaps()
{
    hash_map<wstring,WORD> slots;

    // process boots map file
    hash_map<DWORD,wstring> mapFile;
    wstring mpath(getPesInfo()->gdbDir);
    mpath += L"GDB\\boots\\map.txt";
    if (!readMap(mpath.c_str(), mapFile))
    {
        LOG1S(L"Couldn't open boots map for reading: {%s}",mpath.c_str());
    }
    else
    {
        int slot = 0;
        for (hash_map<DWORD,wstring>::iterator it = mapFile.begin(); it != mapFile.end(); it++)
        {
            wstring boot(it->second);
            string_strip(boot);
            if (!boot.empty())
                string_strip_quotes(boot);

            if (!boot.empty())
            {
                // check that the file exists, so that we don't crash
                // later, when it's attempted to replace a boot.
                wstring filename(getPesInfo()->gdbDir);
                filename += L"GDB\\boots\\" + boot;
                HANDLE handle;
                DWORD size;
                if (OpenFileIfExists(filename.c_str(), handle, size))
                {
                    CloseHandle(handle);
                    if (slot >= MAX_BOOTS)
                    {
                        LOG2N(L"ERROR in bootserver map: Too many boots: %d (MAX supported = %d). Aborting map processing", slot, MAX_BOOTS);
                        break;
                    }

                    hash_map<wstring,WORD>::iterator wit = slots.find(boot);
                    if (wit != slots.end())
                    {
                        // boot already has an assigned slot
                        _boot_slots.insert(pair<DWORD,WORD>(
                                    it->first, wit->second));
                    }
                    else
                    {
                        _boot_slots.insert(pair<DWORD,WORD>(
                                    it->first, FIRST_EXTRA_BOOT_SLOT + slot));
                        slots.insert(pair<wstring,WORD>(
                                    boot, FIRST_EXTRA_BOOT_SLOT + slot));
                        slot++;
                    }
                }
                else
                    LOG1N1S(L"ERROR in bootserver map for ID = %d: FAILED to open boot BIN \"%s\". Skipping", it->first, boot.c_str());
            }
        }
    }

    if (_bootserv_config._random_boots)
    {
        // enumerate all boots and add them to the slots list
        wstring dir(getPesInfo()->gdbDir);
        dir += L"GDB\\boots\\";

        // normalize the path
        wchar_t fullpath[MAX_PATH];
        GetFullPathName(dir.c_str(), MAX_PATH, fullpath, 0);
        dir = fullpath;

        int count;
        LOG(L"Enumerating all boots in GDB...");
        EnumerateBoots(dir, count);
        _num_random_boots = count;
        LOG1N(L"_num_random_boots = %d", _num_random_boots);
    }

    // initialize fast bin lookup table
    for (hash_map<wstring,WORD>::iterator sit = slots.begin();
            sit != slots.end();
            sit++)
    {
        _fast_bin_table[sit->second - FIRST_EXTRA_BOOT_SLOT] = 
            new wstring(sit->first);
        if (k_bootserv.debug)
            LOG1N1S(L"slot %d <-- boot {%s}", sit->second, sit->first.c_str());
    }

    LOG1N(L"Total assigned GDB boots: %d", slots.size());
    LOG1N(L"Total random GDB boots: %d", _num_random_boots);

    if (slots.size() > 0)
        _num_slots = FIRST_EXTRA_BOOT_SLOT + slots.size();
    if (_num_random_boots > 0)
        _num_slots = FIRST_RANDOM_BOOT_SLOT + _num_random_boots;
    LOG1N(L"_num_slots = %d", _num_slots);
}

void EnumerateBoots(wstring dir, int& count)
{
    // check for skip-path
    if (wcsicmp(dir.c_str(),_bootserv_config._random_skip.c_str())==0)
    {
        LOG1S(L"skipping {%s}: boots from this folder can ONLY BE ASSIGNED MANUALLY (using map.txt)", dir.c_str());
        return;
    }

	WIN32_FIND_DATA fData;
    wstring pattern(dir);
    pattern += L"*";

	HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
	if (hff == INVALID_HANDLE_VALUE) 
	{
		// none found.
		return;
	}
	while(true)
	{
        // bounds check
        if (count >= MAX_BOOTS)
        {
            LOG1N(L"ERROR in bootserver: Too many boots (MAX supported = %d). Random boot enumeration stopped.", MAX_BOOTS);
            break;
        }

        // check if this is a directory
        if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
                && wcscmp(fData.cFileName,L".")!=0 
                && wcscmp(fData.cFileName,L"..")!=0)
        {
            wstring nestedDir(dir);
            nestedDir += fData.cFileName;
            nestedDir += L"\\";
            EnumerateBoots(nestedDir, count);
        }
        else if ((fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            // check for ".bin" extension. This way we have
            // a little bit of protection against other types
            // of files in boots folder.
            int flen = wcslen(fData.cFileName);
            if (wcsicmp(fData.cFileName+flen-4, L".bin")==0)
            {
                int idx = FIRST_RANDOM_BOOT_SLOT + count;

                wstring bootFile(dir);
                bootFile += L"\\";
                bootFile += fData.cFileName;
                if (k_bootserv.debug)
                    LOG1N1S(L"random boot: %d <-- {%s}", idx, bootFile.c_str());

                _fast_bin_table[idx - FIRST_EXTRA_BOOT_SLOT] =
                    new wstring(bootFile);
                count++;
            }
		}

		// proceed to next file
		if (!FindNextFile(hff, &fData)) break;
	}
	FindClose(hff);
}

void bootservCopyPlayerData(PLAYER_INFO* players, int place, bool writeList)
{
    if (!_fast_lookup_initialized)
    {
        for (int i=0; i<MAX_PLAYERS; i++)
        {
            hash_map<DWORD,WORD>::iterator it = _boot_slots.find(players[i].id);
            if (it != _boot_slots.end())
                _player_boot_slots[i] = it->second;
        }
        _fast_lookup_initialized = true;
    }

    if (place==2)
    {
        DWORD menuMode = *(DWORD*)data[MENU_MODE_IDX];
        if (menuMode==NETWORK_MODE && !_bootserv_config._enable_online)
            return;

        if (!_struct_replaced)
            _struct_replaced = afsioExtendSlots_cv0(_num_slots);
    }


    if (_bootserv_config._random_boots && !_random_mapped)
    {
        // randomly map all available boots
        if (_num_random_boots > 0)
        {
            LARGE_INTEGER num;
            QueryPerformanceCounter(&num);
            srand(num.LowPart);

            for (WORD i=0; i<MAX_PLAYERS; i++)
            {
                if (players[i].id == 0)
                    continue;
                if (_player_boot_slots[i]!=0)
                    continue;

                int idx = rand() % _num_random_boots;

                //LOG1N(L"random idx = %d", idx);
                //_boot_slots.insert(pair<DWORD,WORD>(
                //            players[i].id, FIRST_RANDOM_BOOT_SLOT + idx
                //));
                _player_boot_slots[i] = FIRST_RANDOM_BOOT_SLOT + idx;
            }
            LOG(L"random boots mapped.");
        }
        _random_mapped = true;
    }

    multimap<string,DWORD> mm;
    for (WORD i=0; i<MAX_PLAYERS; i++)
    {
        if (players[i].id == 0)
            continue;
        //if (players[i].padding!=0)
        //    LOG2N(L"id=%d, padding=%d",players[i].id,players[i].padding);

        //hash_map<DWORD,WORD>::iterator it = 
        //    _boot_slots.find(players[i].id);
        //if (it != _boot_slots.end())
        if (_player_boot_slots[i]!=0)
        {
            players[i].padding = i; // player index
        }
        if (writeList && players[i].name[0]!='\0')
        {
            string name(players[i].name);
            mm.insert(pair<string,DWORD>(name,players[i].id));
        }
    }

    if (writeList)
    {
        // write out playerlist.txt
        wstring plist(getPesInfo()->myDir);
        plist += L"\\playerlist.txt";
        FILE* f = _wfopen(plist.c_str(),L"wt");
        if (f)
        {
            for (multimap<string,DWORD>::iterator it = mm.begin();
                    it != mm.end();
                    it++)
                fprintf(f,"%7d : %s\n",it->second,it->first.c_str());
            fclose(f);
        }
    }

    LOG(L"bootservCopyPlayerData() done: players updated.");
}

/**
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
bool bootservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize)
{
    if (afsId != 0 || (binId < FIRST_BOOT_SLOT || binId > LAST_BOOT_SLOT) && binId < FIRST_EXTRA_BOOT_SLOT)
        return false;

    if (binId >= FIRST_RANDOM_BOOT_SLOT && binId < LAST_EXTRA_BOOT_SLOT)
    {
        LOG1N(L"loading boot BIN (random boot): %d", binId);
        wstring* pws = _fast_bin_table[binId - FIRST_EXTRA_BOOT_SLOT];
        if (pws) 
        {
            if (k_bootserv.debug)
                LOG1N1S(L"%d --> {%s}", binId, pws->c_str());
            return OpenFileIfExists(pws->c_str(), hfile, fsize);
        }
    }

    if (binId >= FIRST_EXTRA_BOOT_SLOT && binId < _num_slots)
    {
        LOG1N(L"loading boot BIN: %d", binId);
        wstring* pws = _fast_bin_table[binId - FIRST_EXTRA_BOOT_SLOT];
        if (pws) 
        {
            wchar_t filename[1024] = {0};
            swprintf(filename,L"%sGDB\\boots\\%s", getPesInfo()->gdbDir,
                    pws->c_str());

            if (k_bootserv.debug)
                LOG1N1S(L"%d --> {%s}", binId, filename);
            return OpenFileIfExists(filename, hfile, fsize);
        }
    }

    LOG1N(L"loading boot BIN (standard): %d", binId);
    return false;
}

void bootservAtGetBootIdCallPointA()
{
    __asm {
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        and eax,0x7f
        push esi  // param: to lookup player index
        push eax  // param: boot index
        call bootservGetBootId1
        add esp,0x08     // pop parameters
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

void bootservAtGetBootIdCallPointB()
{
    __asm {
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        and eax,0x7f
        push eax  // param: boot index
        call bootservGetBootId2
        add esp,4 // pop parameters
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

void bootservAtGetBootIdCallPointC()
{
    __asm {
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        and eax,0x7f
        push eax  // param: boot index
        call bootservGetBootId2
        add esp,4 // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop ebp
        popfd
        mov [ecx+0x10],eax // execute replaced code
        retn
    }
}

KEXPORT DWORD bootservGetBootId1(DWORD boot, BYTE* pPartialPlayerInfo)
{
    if (pPartialPlayerInfo)
    {
        WORD* pPlayerIndex = (WORD*)(pPartialPlayerInfo + 0x3a);
        if (!IsBadReadPtr(pPlayerIndex,2))
        {
            WORD idx = *pPlayerIndex;
            if (idx>0 && idx<MAX_PLAYERS)
            {
                // lookup player
                /*
                DWORD id = GetIdByPlayerIndex(idx);
                hash_map<DWORD,WORD>::iterator sit = _boot_slots.find(id);
                if (sit != _boot_slots.end())
                {
                    return sit->second;
                }
                */
                WORD slot = _player_boot_slots[idx];
                if (slot!=0)
                    return slot;
            }
        }
    }

    if (boot >= 9)
        return FIRST_BOOT_SLOT;
    return FIRST_BOOT_SLOT + boot;
}

KEXPORT DWORD bootservGetBootId2(DWORD boot)
{
    if (k_bootserv.debug)
        LOG1N(L"GetBootId2: _last_playerIndex = %d", _last_playerIndex);

    WORD idx = _last_playerIndex;
    _last_playerIndex = 0; // reset

    if (idx>0 && idx<MAX_PLAYERS)
    {
        // lookup player
        /*
        DWORD id = GetIdByPlayerIndex(idx);
        hash_map<DWORD,WORD>::iterator sit = _boot_slots.find(id);
        if (sit != _boot_slots.end())
        {
            return sit->second;
        }
        */
        WORD slot = _player_boot_slots[idx];
        if (slot!=0)
            return slot;
    }

    if (boot >= 9)
        return FIRST_BOOT_SLOT;
    return FIRST_BOOT_SLOT + boot;
}

DWORD GetIdByPlayerIndex(DWORD idx)
{
    if (idx >= MAX_PLAYERS)
        return 0xffffffff;

    PLAYER_INFO* players = (PLAYER_INFO*)(*(DWORD**)data[EDIT_DATA_PTR] + 1);
    return players[idx].id;
}

WORD GetIndexByPlayerId(DWORD id)
{
    PLAYER_INFO* players = (PLAYER_INFO*)(*(DWORD**)data[EDIT_DATA_PTR] + 1);
    for (int idx=0; idx<MAX_PLAYERS; idx++)
        if (players[idx].id == id)
            return idx;
    return 0;
}

void bootservAtProcessBootIdCallPoint()
{
    __asm {
        push eax
        mov edx,dword ptr ds:[esi+0x2c]
        shr edx,5
        mov _last_playerIndex, 0
        mov eax,dword ptr ds:[esi+0x38]
        cmp eax,0x01010101
        jz ex
        mov ax,word ptr ds:[esi+0x3a]
        movzx eax,ax
        mov _last_playerIndex, eax
    ex: pop eax
        retn
    }
}

void bootservEntranceBootsCallPoint()
{
    __asm {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov esi, dword ptr ds:[edi+0x534]
        push esi
        call bootservEntranceBoots
        add esp,4 // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov ecx, dword ptr ds:[edi+0x534]
        retn
    }
}

KEXPORT void bootservEntranceBoots(BYTE* pData)
{
    WORD* pPlayerIndex = (WORD*)(pData + 0xe4);
    if (!IsBadReadPtr(pPlayerIndex,2))
    {
        if (*pPlayerIndex >= MAX_PLAYERS)
            return;

        // verify correct location: player IDs should match
        DWORD id1 = GetIdByPlayerIndex(*pPlayerIndex);
        DWORD* pId2 = (DWORD*)(pData + 0x3e);
        if (IsBadReadPtr(pId2,4) || id1 != *pId2)
            return;
        
        if (k_bootserv.debug)
        {
            LOG1N(L"entrance: playerIndex = %d", *pPlayerIndex);
            LOG1N(L"data: %08x", (DWORD)pData);
            LOG1N(L"player.id = %d", *pId2);
        }

        if (id1!=0)
            _last_playerIndex = *pPlayerIndex;
    }
}

/**
 * write data callback
 */
void bootservWriteEditData(LPCVOID data, DWORD size)
{
    LOG(L"Restoring player settings");

    PLAYER_INFO* players = (PLAYER_INFO*)((BYTE*)data + 0x120);
    for (int i=0; i<MAX_PLAYERS; i++)
        players[i].padding = 0;
}

void bootservOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode)
{
    if (overlayOn)
    {
        WORD home,away;
        GetCurrentTeams(home,away);
        if (home != 0xffff && away != 0xffff)
            ReRandomizeBoots();
    }
}

void ReRandomizeBoots()
{
    LARGE_INTEGER num;
    QueryPerformanceCounter(&num);
    srand(num.LowPart);

    for (WORD i=0; i<MAX_PLAYERS; i++)
    {
        if (_player_boot_slots[i] < FIRST_RANDOM_BOOT_SLOT)
            continue;

        int idx = rand() % _num_random_boots;
        _player_boot_slots[i] = FIRST_RANDOM_BOOT_SLOT + idx;
    }
    LOG(L"random boots re-mapped.");
}

void GetCurrentTeams(WORD& home, WORD& away)
{
    home = 0xffff; away = 0xffff;
    NEXT_MATCH_DATA_INFO* pNM = 
        *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
    if (pNM && pNM->home) home = pNM->home->teamId;
    if (pNM && pNM->away) away = pNM->away->teamId;
}

