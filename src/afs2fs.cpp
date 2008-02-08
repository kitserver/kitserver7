/* AFS2FS module */
#define UNICODE
#define THISMOD &k_kafs

// 0x1c69ab4: jmp: 1c69b6b

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afs2fs.h"
#include "afs2fs_addr.h"
#include "dllinit.h"
#include "gdb.h"
#include "pngdib.h"
#include "utf8.h"

#define lang(s) getTransl("afs2fs",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

#define EnterCriticalSection(x)
#define LeaveCriticalSection(x)

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_kafs = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// GLOBALS
bool _filesWIN32 = true;

CRITICAL_SECTION g_csFileMap;
CRITICAL_SECTION g_csOffsetMap;
CRITICAL_SECTION g_csEventMap;

hash_map<DWORD,FILE_STRUCT> g_file_map;
hash_map<DWORD,DWORD> g_offset_map;
hash_map<DWORD,FILE_STRUCT> g_event_map;

// cache
#define FILENAMELEN 64
#define MAX_ITEMS 10609

hash_map<wstring,int> g_maxItems;
hash_map<string,wchar_t*> info_cache;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void afsAtGetBinBufferSizeCallPoint();
void afsAfterGetOffsetPagesCallPoint();
void afsAtGetSizeCallPoint();
void afsAfterCreateEventCallPoint();
void afsAtGetImgSizeCallPoint();
void afsBeforeReadCallPoint();
void afsAtCloseHandleCallPoint();

KEXPORT DWORD afsAfterGetBinBufferSize(GET_BIN_SIZE_STRUCT* gbss, DWORD orgSize);
KEXPORT void afsAfterGetOffsetPages(DWORD offsetPages, DWORD afsId, DWORD binId);
KEXPORT void afsAtGetSize(DWORD afsId, DWORD binId, DWORD* pSizeBytes, DWORD* pSizePages);
KEXPORT void afsAfterCreateEvent(DWORD eventId, READ_EVENT_STRUCT* res, char* pathName);
KEXPORT void afsBeforeRead(READ_STRUCT* rs);
KEXPORT void afsAtCloseHandle(DWORD eventId);

DWORD GetTargetAddress(DWORD addr);
void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops);
void afsConfig(char* pName, const void* pValue, DWORD a);

// FUNCTION POINTERS

wchar_t* GetBinFileName(DWORD afsId, DWORD binId)
{
    if (afsId < 0 || 7 < afsId) return NULL;
    BIN_SIZE_INFO* pBST = ((BIN_SIZE_INFO**)data[BIN_SIZES_TABLE])[afsId];
    hash_map<string,wchar_t*>::iterator it = info_cache.find(pBST->relativePathName);
    if (it != info_cache.end())
        return it->second + FILENAMELEN*binId;
    return NULL;
}

int GetNumItems(wstring& folder)
{
    int result = MAX_ITEMS;
    hash_map<wstring,int>::iterator it = g_maxItems.find(folder);
    if (it == g_maxItems.end())
    {
        // get number of files inside the corresponding AFS file
        wstring afsFile(getPesInfo()->pesDir);
        FILE* f = _wfopen((afsFile + folder).c_str(),L"rb");
        if (f) {
            AFSDIRHEADER afsDirHdr;
            ZeroMemory(&afsDirHdr,sizeof(AFSDIRHEADER));
            fread(&afsDirHdr,sizeof(AFSDIRHEADER),1,f);
            if (afsDirHdr.dwSig == AFSSIG)
            {
                g_maxItems.insert(pair<wstring,int>(folder, afsDirHdr.dwNumFiles));
                result = afsDirHdr.dwNumFiles;
            }
            fclose(f);
        }
        else
        {
            // can't open for reading, then just reserve a big enough cache
            g_maxItems.insert(pair<wstring,int>(folder, MAX_ITEMS));
        }
    }
    else
        result = it->second;

    return result;
}

void InitializeFileNameCache()
{
    LOG(L"Initializing filename cache...");

	WIN32_FIND_DATA fData;
    wstring pattern(getPesInfo()->myDir);
    pattern += L"img\\*.img";

	HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
	if (hff == INVALID_HANDLE_VALUE) 
	{
		// none found.
		return;
	}
	while(true)
	{
        // check if this is a directory
        if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            WIN32_FIND_DATA fData1;
            wstring folder(L".\\img\\");
            folder += fData.cFileName;
            wstring folderpattern(getPesInfo()->myDir);
            folderpattern += folder + L"\\*";

            char* key_c = Utf8::unicodeToAnsi(folder.c_str());
            string key(key_c);
            Utf8::free(key_c);

            TRACE1S(L"Looking for %s",folderpattern.c_str());
            HANDLE hff1 = FindFirstFile(folderpattern.c_str(), &fData1);
            if (hff1 != INVALID_HANDLE_VALUE) 
            {
                while(true)
                {
                    // check if this is a file
                    if (!(fData1.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        int binId = -1;
                        wchar_t* s = wcsrchr(fData1.cFileName,'_');
                        if (s && swscanf(s+1,L"%d",&binId)==1)
                        {
                            TRACE1S1N(L"folder={%s}, bin={%d}",folder.c_str(),binId);
                            if (binId >= 0)
                            {
                                wchar_t* names = NULL;
                                hash_map<string,wchar_t*>::iterator cit = info_cache.find(key);
                                if (cit != info_cache.end()) names = cit->second;
                                else 
                                {
                                    names = (wchar_t*)HeapAlloc(
                                            GetProcessHeap(),
                                            HEAP_ZERO_MEMORY, 
                                            sizeof(wchar_t)*FILENAMELEN*GetNumItems(folder)
                                            );
                                    info_cache.insert(pair<string,wchar_t*>(key,names));
                                }

                                // put filename into cache
                                wcscpy(names + FILENAMELEN*binId, fData1.cFileName);
                            }
                        }
                    }

                    // proceed to next file
                    if (!FindNextFile(hff1, &fData1)) break;
                }
                FindClose(hff1);
            }
		}

		// proceed to next file
		if (!FindNextFile(hff, &fData)) break;
	}
	FindClose(hff);

    // print cache
    for (hash_map<wstring,int>::iterator it = g_maxItems.begin(); it != g_maxItems.end(); it++)
        LOG1S1N(L"filename cache: {%s} : %d slots", it->first.c_str(), it->second);

    LOG(L"DONE initializing filename cache.");
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

        // initialize critical sections
        InitializeCriticalSection(&g_csFileMap);
        InitializeCriticalSection(&g_csOffsetMap);
        InitializeCriticalSection(&g_csEventMap);

		copyAdresses();
		hookFunction(hk_D3D_CreateDevice, initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
        // destroy critical sections
        DeleteCriticalSection(&g_csFileMap);
        DeleteCriticalSection(&g_csOffsetMap);
        DeleteCriticalSection(&g_csEventMap);

        for (hash_map<string,wchar_t*>::iterator it = info_cache.begin(); 
                it != info_cache.end();
                it++)
            if (it->second) HeapFree(it->second, 0, GetProcessHeap());
	}
	
	return true;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

	unhookFunction(hk_D3D_CreateDevice, initModule);

    InitializeFileNameCache();

    HookCallPoint(code[C_AT_GET_BINBUFFERSIZE], afsAtGetBinBufferSizeCallPoint, 6, 1);
    HookCallPoint(code[C_AT_GET_SIZE], afsAtGetSizeCallPoint, 6, 1);
    HookCallPoint(code[C_AFTER_CREATE_EVENT], afsAfterCreateEventCallPoint, 6, 3);
    HookCallPoint(code[C_AT_GET_IMG_SIZE], afsAtGetImgSizeCallPoint, 6, 0);
    HookCallPoint(code[C_AT_CLOSE_HANDLE], afsAtCloseHandleCallPoint, 6, 3);
    HookCallPoint(code[C_AFTER_GET_OFFSET_PAGES], afsAfterGetOffsetPagesCallPoint, 6, 1);
    HookCallPoint(code[C_BEFORE_READ], afsBeforeReadCallPoint, 6, 1);

    getConfig("afs2fs", "files.win32", DT_DWORD, 1, afsConfig);
    LOG1N(L"files.win32 = %d",_filesWIN32);

	TRACE(L"Hooking done.");
    //__asm { int 3 }
    return D3D_OK;
}

void afsConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1:	// files.win32
			_filesWIN32 = *(DWORD*)pValue == 1;
			break;
	}
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

/**
 * Simple file-check routine.
 */
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size)
{
    TRACE1S(L"FileExists:: Checking file: %s", filename);
    if (_filesWIN32)
    {
        handle = CreateFile(filename,           // file to open
                           GENERIC_READ,          // open for reading
                           FILE_SHARE_READ,       // share for reading
                           NULL,                  // default security
                           OPEN_EXISTING,         // existing file only
                           FILE_ATTRIBUTE_NORMAL, // normal file
                           NULL);                 // no attr. template

        if (handle != INVALID_HANDLE_VALUE)
        {
            size = GetFileSize(handle,NULL);
            return true;
        }
    }
    else
    {
        FILE* f = _wfopen(filename,L"rb");
        if (f)
        {
            struct stat st;
            fstat(fileno(f), &st);
            handle = (HANDLE)f;
            size = st.st_size;
            return true;
        }
    }
    return false;
}

void afsAtGetBinBufferSizeCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov edx,dword ptr ss:[ebp+0x10]  // execute replaced code
        mov eax,dword ptr ds:[eax+edx*4] // ...
        push eax // org-size
        push ebp 
        call afsAfterGetBinBufferSize
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

KEXPORT DWORD afsAfterGetBinBufferSize(GET_BIN_SIZE_STRUCT* gbss, DWORD orgSize)
{
    DWORD result = orgSize;
    //LOG2N(L"afsId=%d, binId=%d", gbss->afsId, gbss->binId);

    wchar_t* file = GetBinFileName(gbss->afsId, gbss->binId);
    if (!file || file[0]=='\0')  // quick check
        return result;
    
    BIN_SIZE_INFO* pBST = ((BIN_SIZE_INFO**)data[BIN_SIZES_TABLE])[gbss->afsId];
    wchar_t* afsDir = Utf8::ansiToUnicode(pBST->relativePathName);

    // check for a file
    wchar_t filename[1024] = {0};
    swprintf(filename,L"%s%s\\%s", getPesInfo()->myDir, afsDir, file);
    Utf8::free(afsDir);

    HANDLE hfile;
    DWORD fsize = 0;
    if (OpenFileIfExists(filename, hfile, fsize))
    {
        DWORD binKey = (gbss->afsId << 16) + gbss->binId;
        EnterCriticalSection(&g_csFileMap);
        hash_map<DWORD,FILE_STRUCT>::iterator it = g_file_map.find(binKey);
        if (it == g_file_map.end())
        {
            // make new entry
            FILE_STRUCT fs;
            fs.hfile = hfile;
            fs.fsize = fsize;
            fs.orgSize = orgSize;
            fs.offset = 0;
            fs.binKey = binKey;

            g_file_map.insert(pair<DWORD,FILE_STRUCT>(binKey,fs));
        }
        else
        {
            // update existing entry (close old file handle)
            CloseHandle(it->second.hfile);
            it->second.hfile = hfile;
            it->second.fsize = fsize;
            it->second.orgSize = orgSize;
            it->second.offset = 0;
            it->second.binKey = binKey;
        }
        LeaveCriticalSection(&g_csFileMap);

        // modify BIN buffer size
        // IMPORTANT: don't decrease: only increase, because the game
        // will still load a BIN from AFS, which we will then replace later.
        DWORD newSize = (fsize + 0x7ff) & 0xfffff800;
        //LOG4N(L"afsId=%d, binId=%d, orgSize=%0x, newSize=%0x", gbss->afsId, gbss->binId, orgSize, newSize);
        result = max(orgSize, newSize);
    }
    return result;
}

void afsAtGetSizeCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov eax,dword ptr ss:[esp+0x24+0x24] // execute replaced code
        mov dword ptr ds:[eax],edx           // ...
        mov ebx,[esp+0x24+0x08]
        push ecx // pSizePages
        push eax // pSizeBytes
        push edi // binId
        push ebx // afsId
        call afsAtGetSize
        add esp,0x10     // pop parameters
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

KEXPORT void afsAtGetSize(DWORD afsId, DWORD binId, DWORD* pSizeBytes, DWORD* pSizePages)
{
    //LOG4N(L"afsAtGetSize: afsId=%d, binId=%d, sizeBytes=%08x, sizePages=%08x",
    //        afsId, binId, *pSizeBytes, *pSizePages);
    DWORD binKey = (afsId << 16) + binId;
    hash_map<DWORD,FILE_STRUCT>::iterator it = g_file_map.find(binKey);
    if (it != g_file_map.end())
    {
        // modify size
        *pSizeBytes = it->second.fsize;
        *pSizePages = (it->second.fsize + 0x7ff) / 0x800;
    }
}

DWORD GetAfsIdByPathName(char* pathName)
{
    BIN_SIZE_INFO** ppBST = (BIN_SIZE_INFO**)data[BIN_SIZES_TABLE];
    for (DWORD afsId=0; afsId<8; afsId++)
    {
        if (ppBST[afsId]==0) continue;
        if (strncmp(ppBST[afsId]->relativePathName,pathName,strlen(pathName))==0)
            return afsId;
    }
    return 0xffffffff;
}

void afsAfterGetOffsetPagesCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov ecx, [esp+0x24+0x08]
        push edi // binId
        push ecx // afsId
        push ebp // offset pages
        call afsAfterGetOffsetPages
        add esp,0x0c     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov ecx, dword ptr ds:[esi+0x114]
        retn
    }
}

KEXPORT void afsAfterGetOffsetPages(DWORD offsetPages, DWORD afsId, DWORD binId)
{
    //LOG3N(L"afsAfterGetOffsetPages:: offsetPages=%08x, afsId=%d, binId=%d",
    //        offsetPages, afsId, binId);

    EnterCriticalSection(&g_csOffsetMap);
    DWORD binKey = (offsetPages << 0x0b) + afsId;
    hash_map<DWORD,DWORD>::iterator it = g_offset_map.find(binKey);
    if (it == g_offset_map.end())
    {
        // insert new entry for future usage
        g_offset_map.insert(pair<DWORD,DWORD>(binKey,binId));
    }
    LeaveCriticalSection(&g_csOffsetMap);
}

void afsAfterCreateEventCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov edx, [esp+0x24+0x08]
        mov ecx, [esp+0x24+0x28]
        push edx  // relative img-file pathname
        push ecx  // pointer to READ_EVENT_STRUCT
        push eax  // event id
        call afsAfterCreateEvent
        add esp,0x0c     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        test eax,eax                      // execute replaced code
        mov dword ptr ds:[esi+0x27c],eax  // ...
        retn
    }
}

KEXPORT void afsAfterCreateEvent(DWORD eventId, READ_EVENT_STRUCT* res, char* pathName)
{
    // The task here is to link the eventId with afsId/binId 
    // of the file that is going to be read later.
    //TRACE1N(L"afsAfterCreateEvent:: eventId=%08x",eventId);

    DWORD afsId = GetAfsIdByPathName(pathName);
    DWORD binKey = (res->offsetPages << 0x0b) + afsId;

    EnterCriticalSection(&g_csOffsetMap);
    hash_map<DWORD,DWORD>::iterator it = g_offset_map.find(binKey);
    if (it != g_offset_map.end())
    {
        DWORD binId = it->second;

        // lookup FILE_STRUCT
        DWORD binKey1 = (afsId << 16) + binId;
        EnterCriticalSection(&g_csFileMap);
        hash_map<DWORD,FILE_STRUCT>::iterator fit = g_file_map.find(binKey1);
        if (fit != g_file_map.end())
        {
            // remember offset for later usage
            fit->second.offset = res->offsetPages << 0x0b;

            // Found: so we have a replacement file handle
            // Now associate the eventId with that struct
            EnterCriticalSection(&g_csEventMap);
            g_event_map.insert(pair<DWORD,FILE_STRUCT>(eventId,fit->second));
            LeaveCriticalSection(&g_csEventMap);

            TRACE3N(L"afsAfterCreateEvent:: eventId=%08x (afsId=%d, binId=%d)",eventId, afsId, binId);
        }
        LeaveCriticalSection(&g_csFileMap);
    }
    LeaveCriticalSection(&g_csOffsetMap);
}

void afsAtGetImgSizeCallPoint()
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
        mov eax,[edx+8]    // execute replaced code
        add eax,0x10000000 // add extra space
        mov [edx+8],eax    // ...
        mov dword ptr ds:[edi],eax // ...
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

void afsBeforeReadCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push esi  // pointer to READ_STRUCT
        call afsBeforeRead
        add esp,4  // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        sub esp,4                       // execute replaced code
        mov ecx,[esp+4]                 // (note: need to swap the values
        mov [esp],ecx                   // on top two slots on the stack)
        mov ecx,dword ptr ds:[esi+0x34] // ...
        mov [esp+4],ecx                 // ...
        jmp eax                         // ...
    }
}

KEXPORT void afsBeforeRead(READ_STRUCT* rs)
{
    EnterCriticalSection(&g_csEventMap);
    hash_map<DWORD,FILE_STRUCT>::iterator it = g_event_map.find(rs->pfrs->eventId);
    if (it != g_event_map.end())
    {
        TRACE3N(L"afsBeforeRead::(%08x) WAS: hfile=%08x, offset=%08x", 
                (DWORD)rs->pfrs->eventId, (DWORD)rs->pfrs->hfile, rs->pfrs->offset); 
        FILE_STRUCT& fs = it->second;
        rs->pfrs->hfile = fs.hfile;
        rs->pfrs->offset -= fs.offset;
        rs->pfrs->offset_again -= fs.offset;
        TRACE3N(L"afsBeforeRead::(%08x) NOW: hfile=%08x, offset=%08x", 
                (DWORD)rs->pfrs->eventId, (DWORD)rs->pfrs->hfile, rs->pfrs->offset); 
    }
    LeaveCriticalSection(&g_csEventMap);
}

void afsAtCloseHandleCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push eax // handle
        call afsAtCloseHandle
        add esp,0x04     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        push edx                          // execute replaced code
        mov edx,[esp+4]                   // (need to swap two numbers on
        mov [esp+4],eax                   // top of the stack, before doing
        mov [esp+8],edx                   // the call to CloseHandle)
        pop edx                           // ...
        call ebx                          // ...
        mov dword ptr ds:[edi+0x27c],esi  // ...
        retn
    }
}

KEXPORT void afsAtCloseHandle(DWORD eventId)
{
    // reading event is complete. Handle is being closed
    // Do necessary house-keeping tasks, such as removing the eventId
    // from the event map
    //TRACE1N(L"afsAtCloseHandle:: eventId=%08x",eventId);

    EnterCriticalSection(&g_csEventMap);
    hash_map<DWORD,FILE_STRUCT>::iterator it = g_event_map.find(eventId);
    if (it != g_event_map.end())
    {
        FILE_STRUCT& fs = it->second;
        HANDLE hfile = fs.hfile;

        // delete entry in file_map
        hash_map<DWORD,FILE_STRUCT>::iterator fit = g_file_map.find(fs.binKey);
        if (fit != g_file_map.end())
        {
            LOG2N(L"Finished read-event for afsId=%d, binId=%d",
                    (fs.binKey >> 16)&0xffff, fs.binKey&0xffff);
            g_file_map.erase(fit);
        }

        // close file handle, and delete from event map
        CloseHandle(hfile);
        g_event_map.erase(it);

    }
    LeaveCriticalSection(&g_csEventMap);
}


