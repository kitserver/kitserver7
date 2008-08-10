/* <fserv>
 *
 */
#define UNICODE
#define THISMOD &k_fserv

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afsio.h"
#include "fserv.h"
#include "fserv_addr.h"
#include "dllinit.h"
#include "pngdib.h"
#include "configs.h"
#include "configs.hpp"
#include "utf8.h"
#include "replay.h"
#include "player.h"

#define lang(s) getTransl("fserv",s)

#include <map>
#include <list>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

//#define CREATE_FLAGS FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING
#define CREATE_FLAGS 0

#define FIRST_FACE_SLOT 20000
#define NUM_SLOTS 65536

#define UNIQUE_FACE 0x10
#define SCAN_FACE   0x20
#define UNIQUE_HAIR 0x40
#define CLEAR_UNIQUE_FACE 0xef
#define CLEAR_SCAN_FACE   0xdf
#define CLEAR_UNIQUE_HAIR 0xbf

#define NETWORK_MODE 4
#define MAX_PLAYERS 5460

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_fserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

class fserv_config_t
{
public:
    bool _enable_online;
    fserv_config_t() : _enable_online(false) {}
};

fserv_config_t _fserv_config;

// GLOBALS
hash_map<DWORD,wstring> _player_face;
hash_map<DWORD,wstring> _player_hair;
hash_map<DWORD,WORD> _player_face_slot;
hash_map<DWORD,WORD> _player_hair_slot;
list<DWORD> _non_unique_face;
list<DWORD> _non_unique_hair;
list<DWORD> _scan_face;

wstring* _fast_bin_table[NUM_SLOTS-FIRST_FACE_SLOT];

bool _struct_replaced = false;
int _num_slots = 8094;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void fservAtFaceHairCallPoint();
void fservAtCopyEditDataCallPoint();
KEXPORT void fservAtFaceHair(DWORD dest, DWORD src);

void fservConfig(char* pName, const void* pValue, DWORD a);
bool fservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size);
void InitMaps();
void fservCopyPlayerData(PLAYER_INFO* players, int place, bool writeList);

void fservWriteEditData(LPCVOID data, DWORD size);
void fservReadReplayData(LPCVOID data, DWORD size);
void fservWriteReplayData(LPCVOID data, DWORD size);

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
	}
	
	return true;
}

void fservConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1: // debug
			k_fserv.debug = *(DWORD*)pValue;
			break;
        case 2: // disable-online
            _fserv_config._enable_online = *(DWORD*)pValue == 1;
            break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

	unhookFunction(hk_D3D_CreateDevice, initModule);

    LOG(L"Initializing Faceserver");

    getConfig("fserv", "debug", DT_DWORD, 1, fservConfig);
    getConfig("fserv", "online.enabled", DT_DWORD, 2, fservConfig);

    HookCallPoint(code[C_CHECK_FACE_AND_HAIR_ID], 
            fservAtFaceHairCallPoint, 6, 20);

    // register callbacks
    afsioAddCallback(fservGetFileInfo);
    addCopyPlayerDataCallback(fservCopyPlayerData);

    addReadReplayDataCallback(fservReadReplayData);
    addWriteReplayDataCallback(fservWriteReplayData);
    addWriteEditDataCallback(fservWriteEditData);

    // initialize face/hair map
    InitMaps();

	TRACE(L"Hooking done.");
    return D3D_OK;
}

void InitMaps()
{
    ZeroMemory(_fast_bin_table, sizeof(_fast_bin_table));

    // process face/hair map file
    hash_map<DWORD,wstring> mapFile;
    wstring mpath(getPesInfo()->gdbDir);
    mpath += L"GDB\\faces\\map.txt";
    if (!readMap(mpath.c_str(), mapFile))
    {
        LOG1S(L"Couldn't open face/hair-map for reading: {%s}",mpath.c_str());
    }
    else
    {
        for (hash_map<DWORD,wstring>::iterator it = mapFile.begin(); it != mapFile.end(); it++)
        {
            wstring& line = it->second;
            int comma = line.find(',');

            wstring face(line.substr(0,comma));
            string_strip(face);
            if (!face.empty())
                string_strip_quotes(face);

            wstring hair;
            if (comma != string::npos) // hair can be omitted
            {
                hair = line.substr(comma+1);
                string_strip(hair);
                if (!hair.empty())
                    string_strip_quotes(hair);
            }

            //LOG1N(L"{%d}:",it->first);
            //LOG2S(L"{%s}/{%s}",face.c_str(),hair.c_str());

            if (!face.empty())
            {
                // check that the file exists, so that we don't crash
                // later, when it's attempted to replace a face.
                wstring filename(getPesInfo()->gdbDir);
                filename += L"GDB\\faces\\" + face;
                HANDLE handle;
                DWORD size;
                if (OpenFileIfExists(filename.c_str(), handle, size))
                {
                    CloseHandle(handle);
                    _player_face.insert(pair<DWORD,wstring>(it->first,face));
                }
                else
                    LOG1N1S(L"ERROR in faceserver map for ID = %d: FAILED to open face BIN \"%s\". Skipping", it->first, face.c_str());
            }
            if (!hair.empty())
            {
                // check that the file exists, so that we don't crash
                // later, when it's attempted to replace a hair.
                wstring filename(getPesInfo()->gdbDir);
                filename += L"GDB\\faces\\" + hair;
                HANDLE handle;
                DWORD size;
                if (OpenFileIfExists(filename.c_str(), handle, size))
                {
                    CloseHandle(handle);
                    _player_hair.insert(pair<DWORD,wstring>(it->first,hair));
                }
                else
                    LOG1N1S(L"ERROR in faceserver map for ID = %d: FAILED to open hair BIN \"%s\". Skipping", it->first, hair.c_str());
            }
        }
    }

    DWORD nextSlotPair = FIRST_FACE_SLOT/2;

    // assign slots
    for (hash_map<DWORD,wstring>::iterator it = _player_face.begin();
            it != _player_face.end();
            it++)
    {
        hash_map<DWORD,WORD>::iterator sit = _player_face_slot.find(it->first);
        if (sit != _player_face_slot.end())
            continue; // already has slot
        sit = _player_hair_slot.find(it->first);
        WORD slotId;
        if (sit != _player_hair_slot.end())
            slotId = sit->second-1; // slot already known
        else
        {
            slotId = nextSlotPair*2; // assign new slot
            nextSlotPair++;
        }

        // check whether we ran out of slots
        if (nextSlotPair*2 >= NUM_SLOTS)
        {
            LOG1N(L"No more slots for faces: ID = %d skipped.", it->first);
            continue;
        }

        _fast_bin_table[slotId - FIRST_FACE_SLOT] = &it->second;
        _player_face_slot.insert(pair<DWORD,WORD>(it->first,slotId));
    }
    for (hash_map<DWORD,wstring>::iterator it = _player_hair.begin();
            it != _player_hair.end();
            it++)
    {
        hash_map<DWORD,WORD>::iterator sit = _player_hair_slot.find(it->first);
        if (sit != _player_hair_slot.end())
            continue; // already has slot
        sit = _player_face_slot.find(it->first);
        WORD slotId;
        if (sit != _player_face_slot.end())
            slotId = sit->second+1; // slot already known
        else
        {
            slotId = nextSlotPair*2+1; // assign new slot
            nextSlotPair++;
        }

        // check whether we ran out of slots
        if (nextSlotPair*2 >= NUM_SLOTS)
        {
            LOG1N(L"No more slots for hairs: ID = %d skipped.", it->first);
            continue;
        }

        _fast_bin_table[slotId - FIRST_FACE_SLOT] = &it->second;
        _player_hair_slot.insert(pair<DWORD,WORD>(it->first,slotId));
    }

    // initialize total number of BINs
    _num_slots = nextSlotPair*2;
    LOG1N(L"_num_slots = %d",_num_slots);
}

void fservCopyPlayerData(PLAYER_INFO* players, int place, bool writeList)
{
    if (place==2)
    {
        DWORD menuMode = *(DWORD*)data[MENU_MODE_IDX];
        if (menuMode==NETWORK_MODE && !_fserv_config._enable_online)
            return;
    }

    multimap<string,DWORD> mm;
    for (WORD i=0; i<MAX_PLAYERS; i++)
    {
        if (players[i].id == 0)
            continue;
        //if (players[i].padding!=0)
        //    LOG2N(L"id=%d, padding=%d",players[i].id,players[i].padding);

        hash_map<DWORD,WORD>::iterator it = 
            _player_face_slot.find(players[i].id);
        if (it != _player_face_slot.end())
        {
            players[i].padding = i; // player index
            
            //LOG2N(L"player #%d assigned slot (face) #%d",
            //        players[i].id,it->second);
            // if not unique face, remember that for later restoring
            if (!(players[i].faceHairMask & UNIQUE_FACE))
                _non_unique_face.push_back(i);
            if (players[i].faceHairMask & SCAN_FACE)
                _scan_face.push_back(i);
            // adjust flags
            players[i].faceHairMask |= UNIQUE_FACE;
            players[i].faceHairMask &= CLEAR_SCAN_FACE;
        }
        it = _player_hair_slot.find(players[i].id);
        if (it != _player_hair_slot.end())
        {
            players[i].padding = i; // player index

            //LOG2N(L"player #%d assigned slot (hair) #%d",
            //        players[i].id,it->second);
            // if not unique hair, remember that for later restoring
            if (!(players[i].faceHairMask & UNIQUE_HAIR))
                _non_unique_hair.push_back(i);
            // adjust flag
            players[i].faceHairMask |= UNIQUE_HAIR;
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

    LOG(L"fservCopyPlayerData() done: players updated.");
}

void fservAtFaceHairCallPoint()
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
        push esi // param: src
        push ecx // param: dest
        call fservAtFaceHair
        add esp,0x08     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void GetSlotsByPlayerIndex(DWORD idx, DWORD& faceSlot, DWORD& hairSlot)
{
    if (idx >= MAX_PLAYERS)
        return;

    PLAYER_INFO* players = (PLAYER_INFO*)(*(DWORD**)data[EDIT_DATA_PTR] + 1);
    hash_map<DWORD,WORD>::iterator sit;
    sit = _player_face_slot.find(players[idx].id);
    if (sit != _player_face_slot.end())
        faceSlot = sit->second;
    sit = _player_hair_slot.find(players[idx].id);
    if (sit != _player_hair_slot.end())
        hairSlot = sit->second;
}

KEXPORT void fservAtFaceHair(DWORD dest, DWORD src)
{
    if (!_struct_replaced)
        _struct_replaced = afsioExtendSlots_cv0(_num_slots);

    BYTE faceHairMask = *(BYTE*)(src+3);
    WORD playerIdx = *(WORD*)(src+0x3a);
    DWORD faceSlot = 0, hairSlot = 0;
    GetSlotsByPlayerIndex(playerIdx, faceSlot, hairSlot);

    WORD* from = (WORD*)(src+6); // face
    WORD* to = (WORD*)(dest+0xe);
    *to = *from & 0x7ff;
    if ((faceHairMask & UNIQUE_FACE) &&
            faceSlot >= FIRST_FACE_SLOT && faceSlot < NUM_SLOTS)
    {
        if (_fast_bin_table[faceSlot - FIRST_FACE_SLOT])
            *to = faceSlot-1408;
    }

    from = (WORD*)(src+4); // hair
    to = (WORD*)(dest+4);
    *to = *from & 0x7ff; 
    if ((faceHairMask & UNIQUE_HAIR) &&
            hairSlot >= FIRST_FACE_SLOT && hairSlot < NUM_SLOTS)
    {
        if (_fast_bin_table[hairSlot - FIRST_FACE_SLOT])
            *to = hairSlot-4449;
    }
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
bool fservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize)
{
    if (afsId != 0 || binId < FIRST_FACE_SLOT || binId >= NUM_SLOTS)
        return false;

    wchar_t filename[1024] = {0};
    wstring* pws = _fast_bin_table[binId - FIRST_FACE_SLOT];
    if (pws) 
    {
        swprintf(filename,L"%sGDB\\faces\\%s", getPesInfo()->gdbDir,
                pws->c_str());
        return OpenFileIfExists(filename, hfile, fsize);
    }
    return false;
}

/**
 * write data callback
 */
void fservWriteEditData(LPCVOID data, DWORD size)
{
    LOG(L"Restoring face/hair settings");

    // restore face/hair settings
    PLAYER_INFO* players = (PLAYER_INFO*)((BYTE*)data + 0x120);
    for (list<DWORD>::iterator it = _non_unique_face.begin();
            it != _non_unique_face.end();
            it++)
    {
        //LOG2N(L"Restoring face-type flags for %d (id=%d)", 
        //        *it, players[*it].id);
        players[*it].faceHairMask &= CLEAR_UNIQUE_FACE;
    }
    for (list<DWORD>::iterator it = _non_unique_hair.begin();
            it != _non_unique_hair.end();
            it++)
    {
        //LOG2N(L"Restoring hair-type flags for %d (id=%d)",
        //        *it, players[*it].id);
        players[*it].faceHairMask &= CLEAR_UNIQUE_HAIR;
    }
    for (list<DWORD>::iterator it = _scan_face.begin();
            it != _scan_face.end();
            it++)
    {
        //LOG2N(L"Restoring scan-type flags for %d (id=%d)",
        //        *it, players[*it].id);
        players[*it].faceHairMask |= SCAN_FACE;
    }
}

/**
 * write data callback
 */
void fservWriteReplayData(LPCVOID data, DWORD size)
{
    LOG(L"Restoring face/hair settings");

    // restore face/hair settings
    REPLAY_PLAYER_INFO* players = (REPLAY_PLAYER_INFO*)((BYTE*)data + 0x1c0);
    for (int i=0; i<22; i++)
    {
        wchar_t* name = Utf8::ansiToUnicode(players[i].name);
        LOG1S1N(L"player {%s}, padding=%04x", name, players[i].padding);
        Utf8::free(name);

        WORD idx = players[i].padding;
        if (idx != 0)
        {
            for (list<DWORD>::iterator it = _non_unique_face.begin();
                    it != _non_unique_face.end();
                    it++)
                if (*it == idx)
                {
                    players[i].faceHairMask &= CLEAR_UNIQUE_FACE;
                    break;
                }
            for (list<DWORD>::iterator it = _non_unique_hair.begin();
                    it != _non_unique_hair.end();
                    it++)
                if (*it == idx)
                {
                    players[i].faceHairMask &= CLEAR_UNIQUE_HAIR;
                    break;
                }
            for (list<DWORD>::iterator it = _scan_face.begin();
                    it != _scan_face.end();
                    it++)
                if (*it == idx)
                {
                    players[i].faceHairMask |= SCAN_FACE;
                    break;
                }
        }
    }
}

/**
 * read data callback
 */
void fservReadReplayData(LPCVOID data, DWORD size)
{
    LOG(L"Setting face/hair settings");

    // set face/hair settings
    REPLAY_PLAYER_INFO* players = (REPLAY_PLAYER_INFO*)((BYTE*)data + 0x1c0);
    for (int i=0; i<22; i++)
    {
        wchar_t* name = Utf8::ansiToUnicode(players[i].name);
        LOG1S1N(L"player {%s}, padding=%04x", name, players[i].padding);
        Utf8::free(name);

        WORD idx = players[i].padding;
        if (idx != 0)
        {
            DWORD faceSlot = 0, hairSlot = 0;
            GetSlotsByPlayerIndex(idx, faceSlot, hairSlot);
            if (faceSlot >= FIRST_FACE_SLOT && faceSlot < NUM_SLOTS)
            {
                players[i].faceHairMask |= UNIQUE_FACE;
                players[i].faceHairMask &= CLEAR_SCAN_FACE;
            }
            if (hairSlot >= FIRST_FACE_SLOT && hairSlot < NUM_SLOTS)
                players[i].faceHairMask |= UNIQUE_HAIR;
        }
    }
}

