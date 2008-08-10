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
    bootserv_config_t() : _enable_online(false) {}
};

bootserv_config_t _bootserv_config;

// GLOBALS
hash_map<DWORD,WORD> _boot_slots;
wstring* _fast_bin_table[MAX_BOOTS+100];

bool _struct_replaced = false;
int _num_slots = 8094;
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
void bootservCopyPlayerData(PLAYER_INFO* players, int place, bool writeList);

void bootservAtGetBootIdCallPointA();
void bootservAtGetBootIdCallPointB();
void bootservAtGetBootIdCallPointC();
DWORD GetIdByPlayerIndex(DWORD idx);
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

    // register callbacks
    afsioAddCallback(bootservGetFileInfo);
    addCopyPlayerDataCallback(bootservCopyPlayerData);

    // initialize boots map
    InitMaps();

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

    LOG1N(L"Total GDB boots: %d", slots.size());
    if (slots.size() > 0)
        _num_slots = FIRST_EXTRA_BOOT_SLOT + slots.size();
    LOG1N(L"_num_slots = %d", _num_slots);
}

void bootservCopyPlayerData(PLAYER_INFO* players, int place, bool writeList)
{
    if (place==2)
    {
        DWORD menuMode = *(DWORD*)data[MENU_MODE_IDX];
        if (menuMode==NETWORK_MODE && !_bootserv_config._enable_online)
            return;

        if (!_struct_replaced)
            _struct_replaced = afsioExtendSlots_cv0(_num_slots);
    }

    multimap<string,DWORD> mm;
    for (WORD i=0; i<MAX_PLAYERS; i++)
    {
        if (players[i].id == 0)
            continue;
        //if (players[i].padding!=0)
        //    LOG2N(L"id=%d, padding=%d",players[i].id,players[i].padding);

        hash_map<DWORD,WORD>::iterator it = 
            _boot_slots.find(players[i].id);
        if (it != _boot_slots.end())
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

    LOG1N(L"loading boot BIN: %d", binId);
    if (binId >= FIRST_EXTRA_BOOT_SLOT && binId < 20000)
    {
        wstring* pws = _fast_bin_table[binId - FIRST_EXTRA_BOOT_SLOT];
        if (pws) 
        {
            wchar_t filename[1024] = {0};
            swprintf(filename,L"%sGDB\\boots\\%s", getPesInfo()->gdbDir,
                    pws->c_str());
            return OpenFileIfExists(filename, hfile, fsize);
        }
    }
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
            // lookup player
            DWORD id = GetIdByPlayerIndex(*pPlayerIndex);
            hash_map<DWORD,WORD>::iterator sit = _boot_slots.find(id);
            if (sit != _boot_slots.end())
            {
                return sit->second;
            }
        }
    }

    if (boot >= 9)
        return FIRST_EXTRA_BOOT_SLOT + (boot-9);
    return FIRST_BOOT_SLOT + boot;
}

KEXPORT DWORD bootservGetBootId2(DWORD boot)
{
    if (k_bootserv.debug)
        LOG1N(L"GetBootId2: _last_playerIndex = %d", _last_playerIndex);

    WORD idx = _last_playerIndex;
    _last_playerIndex = 0; // reset

    if (idx!=0)
    {
        // lookup player
        DWORD id = GetIdByPlayerIndex(idx);
        hash_map<DWORD,WORD>::iterator sit = _boot_slots.find(id);
        if (sit != _boot_slots.end())
        {
            return sit->second;
        }
    }

    if (boot >= 9)
        return FIRST_EXTRA_BOOT_SLOT + (boot-9);
    return FIRST_BOOT_SLOT + boot;
}

DWORD GetIdByPlayerIndex(DWORD idx)
{
    if (idx >= MAX_PLAYERS)
        return 0xffffffff;

    PLAYER_INFO* players = (PLAYER_INFO*)(*(DWORD**)data[EDIT_DATA_PTR] + 1);
    return players[idx].id;
}

void bootservAtProcessBootIdCallPoint()
{
    __asm {
        push eax
        mov edx,dword ptr ds:[esi+0x2c]
        shr edx,5
        mov ax,word ptr ds:[esi+0x3a]
        movzx eax,ax
        mov _last_playerIndex, eax
        pop eax
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

