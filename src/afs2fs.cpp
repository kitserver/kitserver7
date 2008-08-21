/* AFS2FS module */
#define UNICODE
#define THISMOD &k_afs

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afsreader.h"
#include "afsio.h"
#include "afs2fs.h"
#include "afs2fs_addr.h"
#include "dllinit.h"
#include "gdb.h"
#include "pngdib.h"
#include "utf8.h"
#include "names.h"

#define lang(s) getTransl("afs2fs",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

//#define CREATE_FLAGS FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING
#define CREATE_FLAGS 0

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_afs = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// cache
#define DEFAULT_FILENAMELEN 64
#define MAX_ITEMS 10609
#define MAX_FOLDERS 8

typedef struct _FAST_INFO_CACHE_STRUCT
{
    bool initialized; 
    int numItems;
    wchar_t* names;
} FAST_INFO_CACHE_STRUCT;

hash_map<wstring,int> g_maxItems;
hash_map<string,wchar_t*> _info_cache;
FAST_INFO_CACHE_STRUCT _fast_info_cache[MAX_FOLDERS];
int _fileNameLen = DEFAULT_FILENAMELEN;
song_map_t* _songs = NULL;
ball_map_t* _balls = NULL;

#define MAX_IMGDIR_LEN 4096
wchar_t _imgDir[MAX_IMGDIR_LEN];

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void afsConfig(char* pName, const void* pValue, DWORD a);
void afsReadSongsMap();
bool afsGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);

// FUNCTION POINTERS

wchar_t* GetBinFileName(DWORD afsId, DWORD binId)
{
    if (afsId < 0 || MAX_FOLDERS-1 < afsId) return NULL; // safety check
    if (!_fast_info_cache[afsId].initialized)
    {
        BIN_SIZE_INFO* pBST = ((BIN_SIZE_INFO**)data[BIN_SIZES_TABLE])[afsId];
        if (pBST) 
        {
            hash_map<string,wchar_t*>::iterator it = _info_cache.find(pBST->relativePathName);
            if (it != _info_cache.end())
                _fast_info_cache[afsId].names = it->second;
        }
        if (k_afs.debug)
            LOG1N(L"initialized _fast_info_cache entry for afsId=%d",afsId);
        _fast_info_cache[afsId].numItems = pBST->numItems;
        _fast_info_cache[afsId].initialized = true;
    }

    wchar_t* base = _fast_info_cache[afsId].names;
    return (base && binId<_fast_info_cache[afsId].numItems) ? 
        base + _fileNameLen*binId : NULL;
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
    LOG1S(L"img.dir = {%s}", _imgDir);

	WIN32_FIND_DATA fData;
    wstring pattern(_imgDir);
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
            wstring folderpattern(_imgDir);
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
                                hash_map<string,wchar_t*>::iterator cit = _info_cache.find(key);
                                if (cit != _info_cache.end()) names = cit->second;
                                else 
                                {
                                    names = (wchar_t*)HeapAlloc(
                                            GetProcessHeap(),
                                            HEAP_ZERO_MEMORY, 
                                            sizeof(wchar_t)*_fileNameLen*GetNumItems(folder)
                                            );
                                    _info_cache.insert(pair<string,wchar_t*>(key,names));
                                }

                                // put filename into cache
                                wcsncpy(names + _fileNameLen*binId, fData1.cFileName, _fileNameLen-1);

                                // print message, if filename is too long
                                if (wcslen(fData1.cFileName) >= _fileNameLen)
                                {
                                    LOG2S(L"ERROR: filename too long: \"%s\" (in folder: %s)", 
                                            fData1.cFileName, folder.c_str());
                                    LOG2N(L"ERROR: length = %d chars. Maximum allowed length: %d chars.", 
                                            wcslen(fData1.cFileName), _fileNameLen-1);
                                }
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

		copyAdresses();
		hookFunction(hk_D3D_CreateDevice, initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
        for (hash_map<string,wchar_t*>::iterator it = _info_cache.begin(); 
                it != _info_cache.end();
                it++)
            if (it->second) HeapFree(GetProcessHeap(), 0, it->second);

        delete _songs;
	}
	
	return true;
}

void afsConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1: // debug
			k_afs.debug = *(DWORD*)pValue;
			break;
		case 2: // filename.length
			_fileNameLen = *(DWORD*)pValue;
			break;
        case 3: // img.dir
            wcsncpy(_imgDir, (wchar_t*)pValue, MAX_IMGDIR_LEN-1);
            break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

    ZeroMemory(_imgDir, sizeof(_imgDir));
    wcsncpy(_imgDir, getPesInfo()->myDir, MAX_IMGDIR_LEN-1);

    getConfig("afs2fs", "debug", DT_DWORD, 1, afsConfig);
    getConfig("afs2fs", "filename.length", DT_DWORD, 2, afsConfig);
    getConfig("afs2fs", "img.dir", DT_STRING, 3, afsConfig);

	unhookFunction(hk_D3D_CreateDevice, initModule);

    // register callback
    afsioAddCallback(afsGetFileInfo);

    InitializeFileNameCache();
    ZeroMemory(_fast_info_cache,sizeof(_fast_info_cache));

	TRACE(L"Hooking done.");

    wstring songMapFile(getPesInfo()->myDir);
    songMapFile += L"\\names\\songs.txt";
    _songs = new song_map_t(songMapFile);
    // support older location too:
    if (_songs->_songMap.size()==0)
    {
        delete _songs;
        songMapFile = getPesInfo()->myDir;
        songMapFile += L"\\songs.txt";
        _songs = new song_map_t(songMapFile);
    }

    LOG1N(L"Songs-map read (size=%d)",_songs->_songMap.size());

    // apply songs info
    BYTE* bptr = (BYTE*)data[SONGS_INFO_TABLE];
    DWORD protection = 0;
    DWORD newProtection = PAGE_READWRITE;
    if (VirtualProtect(bptr, 59*sizeof(SONG_STRUCT), newProtection, &protection)) 
    {
        SONG_STRUCT* ss = (SONG_STRUCT*)data[SONGS_INFO_TABLE];
        for (int i=0; i<59; i++)
        {
            hash_map<WORD,SONG_STRUCT>::iterator it = _songs->_songMap.find(ss[i].binId);
            if (it != _songs->_songMap.end())
            {
                ss[i].title = it->second.title;
                ss[i].author = it->second.author;
                LOG1N(L"Set title/author info for song with binId=%d",ss[i].binId);
            }
        }
    }

    wstring ballMapFile(getPesInfo()->myDir);
    ballMapFile += L"\\names\\balls.txt";
    _balls = new ball_map_t(ballMapFile);

    LOG1N(L"Balls-map read (size=%d)",_balls->_ballMap.size());

    // apply balls info
    bptr = (BYTE*)data[BALLS_INFO_TABLE];
    protection = 0;
    newProtection = PAGE_READWRITE;
    if (VirtualProtect(bptr, 12*sizeof(char**), newProtection, &protection)) 
    {
        char** names = (char**)data[BALLS_INFO_TABLE];
        for (int i=1; i<=12; i++)
        {
            hash_map<WORD,BALL_STRUCT>::iterator it = _balls->_ballMap.find(i);
            if (it != _balls->_ballMap.end())
            {
                names[i-1] = it->second.name;
                LOG1N(L"Set name for ball #%d",i);
            }
        }
    }

    //__asm { int 3 }          // uncomment this for debugging as needed
    return D3D_OK;
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
bool afsGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize)
{
    wchar_t* file = GetBinFileName(afsId, binId);
    if (!file || file[0]=='\0')  // quick check
        return false;

    BIN_SIZE_INFO* pBST = ((BIN_SIZE_INFO**)data[BIN_SIZES_TABLE])[afsId];
    wchar_t* afsDir = Utf8::ansiToUnicode(pBST->relativePathName);

    // check for a file
    wchar_t filename[1024] = {0};
    swprintf(filename,L"%s%s\\%s", _imgDir, afsDir, file);
    Utf8::free(afsDir);

    return OpenFileIfExists(filename, hfile, fsize);
}

