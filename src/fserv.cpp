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
#define MAX_BIN_ID 65535

#define UNIQUE_FACE 0x10
#define UNIQUE_HAIR 0x40

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_fserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// GLOBALS
hash_map<DWORD,wstring> _player_face;
hash_map<DWORD,wstring> _player_hair;
hash_map<DWORD,WORD> _player_face_slot;
hash_map<DWORD,WORD> _player_hair_slot;

wstring* _fast_bin_table[MAX_BIN_ID-FIRST_FACE_SLOT+1];

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
KEXPORT void fservAtCopyEditData(PLAYER_INFO* players, DWORD size);

DWORD GetTargetAddress(DWORD addr);
void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops);
void fservConfig(char* pName, const void* pValue, DWORD a);
bool fservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size);
bool ReplaceBinSizes();
void InitMaps();


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

    HookCallPoint(code[C_CHECK_FACE_AND_HAIR_ID], 
            fservAtFaceHairCallPoint, 6, 20);
    HookCallPoint(code[C_COPY_DATA], 
            fservAtCopyEditDataCallPoint, 6, 1);

    // register callback
    afsioAddCallback(fservGetFileInfo);

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

            LOG1N(L"{%d}:",it->first);
            LOG2S(L"{%s}/{%s}",face.c_str(),hair.c_str());

            if (!face.empty())
                _player_face.insert(pair<DWORD,wstring>(it->first,face));
            if (!hair.empty())
                _player_hair.insert(pair<DWORD,wstring>(it->first,hair));
        }
    }

    // read slot-map if available
    map<DWORD,DWORD> slots;
    DWORD nextSlotPair = FIRST_FACE_SLOT/2;

    wstring spath(getPesInfo()->myDir);
    spath += L"\\fserv.slotmap";
    FILE* f = _wfopen(spath.c_str(),L"rb");
    if (f) 
    {
        while (true)
        {
            DWORD playerId;
            DWORD slotId;
            if (fread(&slotId,sizeof(slotId),1,f)!=1)
                break;
            if (fread(&playerId,sizeof(playerId),1,f)!=1)
                break;

            slots.insert(pair<DWORD,DWORD>(slotId,playerId));
            nextSlotPair = max(nextSlotPair,slotId/2+1);

            hash_map<DWORD,wstring>::iterator it;
            switch (slotId % 2)
            {
                case 0: // face
                    it = _player_face.find(playerId);
                    if (it != _player_face.end())
                    {
                        _fast_bin_table[slotId - FIRST_FACE_SLOT] = &it->second;
                        _player_face_slot.insert(pair<DWORD,WORD>(playerId,slotId));
                    }
                    break;
                case 1: // hair
                    it = _player_hair.find(playerId);
                    if (it != _player_hair.end())
                    {
                        _fast_bin_table[slotId - FIRST_FACE_SLOT] = &it->second;
                        _player_hair_slot.insert(pair<DWORD,WORD>(playerId,slotId));
                    }
                    break;
            }
        }
        fclose(f);
    }

    // assign new slots, if necessary
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

        _fast_bin_table[slotId - FIRST_FACE_SLOT] = &it->second;
        _player_face_slot.insert(pair<DWORD,WORD>(it->first,slotId));
        slots.insert(pair<DWORD,DWORD>(slotId,it->first));
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

        _fast_bin_table[slotId - FIRST_FACE_SLOT] = &it->second;
        _player_hair_slot.insert(pair<DWORD,WORD>(it->first,slotId));
        slots.insert(pair<DWORD,DWORD>(slotId,it->first));
    }

    // update slot-map
    f = _wfopen(spath.c_str(), L"wb");
    if (!f) 
    {
        LOG(L"ERROR: Unable to update face/hair slot-map.");
        return;
    }
    for (map<DWORD,DWORD>::iterator it = slots.begin();
            it != slots.end();
            it++)
    {
        LOG2N(L"Face/Hair slot-map: (slot) %d --> (player) %d",
                it->first,it->second);

        fwrite(&it->first,sizeof(DWORD),1,f);
        fwrite(&it->second,sizeof(DWORD),1,f);
    }
    fclose(f);

    // initialize total number of BINs
    _num_slots = nextSlotPair*2;
    LOG1N(L"_num_slots = %d",_num_slots);
}

void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops)
{
    DWORD target = (DWORD)func + codeShift;
	if (addr && target)
	{
	    BYTE* bptr = (BYTE*)addr;
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 16, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(addr + 1);
	        ptr[0] = target - (DWORD)(addr + 5);
            // padding with NOPs
            for (int i=0; i<numNops; i++) bptr[5+i] = 0x90;
	        TRACE2N(L"Function (%08x) HOOKED at address (%08x)", target, addr);
	    }
	}
}

DWORD GetTargetAddress(DWORD addr)
{
	if (addr)
	{
	    BYTE* bptr = (BYTE*)addr;
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 8, newProtection, &protection)) 
        {
            // get original target
            DWORD* ptr = (DWORD*)(addr + 1);
            return (DWORD)(ptr[0] + addr + 5);
	    }
	}
    return 0;
}

bool ReplaceBinSizes()
{
    // extend BIN-sizes table
    BIN_SIZE_INFO** tabArray = (BIN_SIZE_INFO**)data[BIN_SIZES_TABLE];
    if (!tabArray)
        return false;
    BIN_SIZE_INFO* table = tabArray[0];
    if (!table)
        return false;

    int newSize = sizeof(DWORD)*(_num_slots)+0x120;
    BIN_SIZE_INFO* newTable = (BIN_SIZE_INFO*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, newSize);
    memcpy(newTable, table, table->structSize);
    for (int i=table->numItems; i<_num_slots; i++)
        newTable->sizes[i] = 0x800; // back-fill with 1 page defaults

    newTable->structSize = newSize;
    newTable->numItems = _num_slots;
    newTable->numItems2 = _num_slots;
    tabArray[0] = newTable; // point to new structure
    return true;
}

KEXPORT void fservAtCopyEditData(PLAYER_INFO* players, DWORD size)
{
    if (size != 0x12a9cc)
        return;  // not edit-data

    multimap<string,DWORD> mm;
    for (int i=0; i<5460; i++)
    {
        if (players[i].id == 0)
            continue;
        //if (players[i].padding!=0)
        //    LOG2N(L"id=%d, padding=%d",players[i].id,players[i].padding);

        hash_map<DWORD,WORD>::iterator it = 
            _player_face_slot.find(players[i].id);
        if (it != _player_face_slot.end())
        {
            players[i].padding = it->second/2; // place slot.
            LOG2N(L"player #%d assigned slot #%d",
                    players[i].id,it->second);
            // adjust flag
            players[i].faceHairMask |= UNIQUE_FACE;
        }
        it = _player_hair_slot.find(players[i].id);
        if (it != _player_hair_slot.end())
        {
            players[i].padding = it->second/2; // place slot.
            LOG2N(L"player #%d assigned slot #%d",
                    players[i].id,it->second);
            // adjust flag
            players[i].faceHairMask |= UNIQUE_HAIR;
        }

        if (players[i].name[0]!='\0')
        {
            string name(players[i].name);
            mm.insert(pair<string,DWORD>(name,players[i].id));
        }
                    
    }

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

    LOG(L"fservAtCopyEditData() done: players modified.");
}

void fservAtCopyEditDataCallPoint()
{
    __asm {
        mov ecx,dword ptr ds:[esi+0xc] // execute replaced code
        mov edx,dword ptr ds:[esi+8] // ...
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ecx // param: size
        push eax // param: src
        call fservAtCopyEditData
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

KEXPORT void fservAtFaceHair(DWORD dest, DWORD src)
{
    if (!_struct_replaced)
        _struct_replaced = ReplaceBinSizes();

    BYTE faceHairMask = *(BYTE*)(src+3);
    WORD slotPair = *(WORD*)(src+0x3a);

    WORD* from = (WORD*)(src+6); // face
    WORD* to = (WORD*)(dest+0xe);
    *to = *from & 0x7ff;
    if (slotPair>=FIRST_FACE_SLOT/2 && (faceHairMask & UNIQUE_FACE))
    {
        if (_fast_bin_table[slotPair*2 - FIRST_FACE_SLOT])
            *to = slotPair*2-1408;
    }

    from = (WORD*)(src+4); // hair
    to = (WORD*)(dest+4);
    *to = *from & 0x7ff; 
    if (slotPair>=FIRST_FACE_SLOT/2 && (faceHairMask & UNIQUE_HAIR))
    {
        if (_fast_bin_table[slotPair*2+1 - FIRST_FACE_SLOT])
            *to = slotPair*2+1-4449;
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
    if (afsId != 0 || binId < FIRST_FACE_SLOT)
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

