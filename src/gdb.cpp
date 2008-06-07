#define UNICODE
#define THISMOD &k_kserv

#include <stdio.h>
#include <string>
#include "gdb.h"
#include "configs.h"
#include "configs.hpp"
#include "utf8.h"
#include "log.h"

extern KMOD k_kserv;

#ifdef MYDLL_RELEASE_BUILD
#define GDB_DEBUG(f,x)
#define GDB_DEBUG_OPEN(f,dir)
#define GDB_DEBUG_CLOSE(f)

#else
#define GDB_DEBUG(f,x) if (f) {\
    swprintf x;\
    char a_str[BUFLEN];\
    ZeroMemory(a_str, BUFLEN);\
    Utf8::fUnicodeToUtf8((BYTE*)a_str,slog);\
    fputs(a_str,f);\
}
#define GDB_DEBUG_OPEN(f,dir) {\
    f = _wfopen((dir + L"GDB.debug.log").c_str(),L"wt");\
}
#define GDB_DEBUG_CLOSE(f) if (f) fclose(f)
FILE* wlog;
wchar_t slog[WBUFLEN];
#endif

#define PLAYERS 0
#define GOALKEEPERS 1

enum {
    ATT_MODEL,
    ATT_COLLAR,
    ATT_SHIRT_NUMBER_LOCATION,
    ATT_SHORTS_NUMBER_LOCATION,
    ATT_NAME_LOCATION,
    ATT_NAME_SHAPE,
    ATT_LOGO_LOCATION,
    ATT_MAIN_COLOR,
    ATT_SHORTS_COLOR,
    ATT_DESCRIPTION,
};

class kattr_data
{
public:
    Kit& kit;
    int attr;

    kattr_data(Kit& k, int a) : kit(k), attr(a) {}
};

// functions
//////////////////////////////////////////

static bool ParseColor(const wchar_t* str, RGBAColor* color);
static void kitConfig(char* pName, const void* pValue, DWORD a);
static bool equals(const void* a, const void* b);
static void string_strip(wstring& s);

static bool equals(const void* a, const void* b)
{
    wstring sa((wchar_t*)a);
    wstring sb((wchar_t*)b);
    string_strip(sa);
    string_strip(sb);
    return sa == sb;
}

static void string_strip(wstring& s)
{
    static const wchar_t* empties = L" \t\n\r";
    int e = s.find_last_not_of(empties);
    s.erase(e + 1);
    int b = s.find_first_not_of(empties);
    s.erase(0,b);
}

/**
 * Allocate and initialize the GDB structure, read the "map.txt" file
 * but don't look for kit folders themselves.
 */
void GDB::load()
{
	GDB_DEBUG_OPEN(wlog,this->dir);
	GDB_DEBUG(wlog,(slog,L"Loading GDB...\n"));

    // process kit map file
    hash_map<WORD,wstring> mapFile;
    if (!readMap((this->dir + L"GDB\\uni\\map.txt").c_str(), mapFile))
    {
        GDB_DEBUG(wlog,(slog,L"Unable to find uni-map: %s\n",mapFile));
        LOG1S(L"Couldn't open uni-map for reading: {%s}",(this->dir + L"GDB\\uni\\map.txt").c_str());
    }

    for (hash_map<WORD,wstring>::iterator it = mapFile.begin(); it != mapFile.end(); it++)
    {
        KitCollection kitCol(it->second);

        // strip off leading and trailing spaces
        string_strip(kitCol.foldername);

        // strip off quotes, if present
        if (kitCol.foldername[0]=='"' || kitCol.foldername[0]=='\'')
            kitCol.foldername.erase(0,1);
        int last = kitCol.foldername.length()-1;
        if (kitCol.foldername[last]=='"' || kitCol.foldername[last]=='\'')
            kitCol.foldername.erase(last);

        GDB_DEBUG(wlog,(slog,L"teamId = {%d}, foldername = {%s}\n", 
                    it->first, kitCol.foldername.c_str()));

        // store in the "uni" map
        this->uni.insert(pair<WORD,KitCollection>(it->first,kitCol));

        // find kits for this team
        this->findKitsForTeam(it->first);
    }

    // create two dummy kit collections: for use with AFS kits
    Kit dummyKit;
    dummyKit.configLoaded = true;

    this->dummyHome.players.insert(pair<wstring,Kit>(L"pa",dummyKit));
    this->dummyHome.players.insert(pair<wstring,Kit>(L"pb",dummyKit));
    this->dummyHome.goalkeepers.insert(pair<wstring,Kit>(L"ga",dummyKit));
    this->dummyHome.goalkeepers.insert(pair<wstring,Kit>(L"gb",dummyKit));
    this->dummyHome.loaded = true;

    this->dummyAway.players.insert(pair<wstring,Kit>(L"pa",dummyKit));
    this->dummyAway.players.insert(pair<wstring,Kit>(L"pb",dummyKit));
    this->dummyAway.goalkeepers.insert(pair<wstring,Kit>(L"ga",dummyKit));
    this->dummyAway.goalkeepers.insert(pair<wstring,Kit>(L"gb",dummyKit));
    this->dummyAway.loaded = true;

	GDB_DEBUG(wlog,(slog,L"Loading GDB complete.\n"));
    GDB_DEBUG_CLOSE(wlog);
}

/**
 * Enumerate all kits in this team folder.
 * and for each one parse the "config.txt" file.
 */
void GDB::fillKitCollection(KitCollection& col, int kitType)
{
	WIN32_FIND_DATA fData;
    wstring pattern(this->dir);

	if (kitType == PLAYERS) {
		pattern += L"GDB\\uni\\" + col.foldername + L"\\p*";
    } else if (kitType == GOALKEEPERS) {
		pattern += L"GDB\\uni\\" + col.foldername + L"\\g*";
    }

	GDB_DEBUG(wlog,(slog, L"pattern = {%s}\n",pattern));

	HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
	if (hff == INVALID_HANDLE_VALUE) 
	{
		// none found.
		return;
	}
	while(true)
	{
        GDB_DEBUG(wlog,(slog,L"found: {%s}\n",fData.cFileName));
        // check if this is a directory
        if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            Kit kit;
            kit.foldername = L"GDB\\uni\\" + col.foldername + L"\\" + wstring(fData.cFileName);

            // read and parse the config.txt
			kit.attDefined = 0;

            wstring key(fData.cFileName);
            if (this->readConfigs)
                this->loadConfig(kit);

            // insert kit object into KitCollection map
            if (kitType == PLAYERS)
                col.players.insert(pair<wstring,Kit>(key,kit));
            else if (kitType == GOALKEEPERS)
                col.goalkeepers.insert(pair<wstring,Kit>(key,kit));
		}

		// proceed to next file
		if (!FindNextFile(hff, &fData)) break;
	}

	FindClose(hff);
}

/**
 * Enumerate all kits in this team folder.
 * and for each one parse the "config.txt" file.
 */
void GDB::findKitsForTeam(WORD teamId)
{
    hash_map<WORD,KitCollection>::iterator it = this->uni.find(teamId);
    if (it != this->uni.end() && !it->second.loaded)
    {
        // players
        this->fillKitCollection(it->second, PLAYERS);
        // goalkeepers
        this->fillKitCollection(it->second, GOALKEEPERS);
        // mark kit collection as loaded
        it->second.loaded = true;
    }
}

/**
 * Read and parse the config.txt for the given kit.
 */
void GDB::loadConfig(Kit& kit)
{
    if (kit.configLoaded) return;

    //LOG1S(L"path: {%s}",(this->dir + kit.foldername + L"\\config.txt").c_str());
    if (readConfig((this->dir + kit.foldername + L"\\config.txt").c_str()))
    {
        _getConfig("", "model", DT_DWORD, (DWORD)&kattr_data(kit,ATT_MODEL), kitConfig);
        _getConfig("", "collar", DT_STRING, (DWORD)&kattr_data(kit,ATT_COLLAR), kitConfig);
        _getConfig("", "shirt.number.location", DT_STRING, (DWORD)&kattr_data(kit,ATT_SHIRT_NUMBER_LOCATION), kitConfig);
        _getConfig("", "shorts.number.location", DT_STRING, (DWORD)&kattr_data(kit,ATT_SHORTS_NUMBER_LOCATION), kitConfig);
        _getConfig("", "name.location", DT_STRING, (DWORD)&kattr_data(kit,ATT_NAME_LOCATION), kitConfig);
        _getConfig("", "name.shape", DT_STRING, (DWORD)&kattr_data(kit,ATT_NAME_SHAPE), kitConfig);
        _getConfig("", "logo.location", DT_STRING, (DWORD)&kattr_data(kit,ATT_LOGO_LOCATION), kitConfig);
        _getConfig("", "main.color", DT_STRING, (DWORD)&kattr_data(kit,ATT_MAIN_COLOR), kitConfig);
        _getConfig("", "radar.color", DT_STRING, (DWORD)&kattr_data(kit,ATT_MAIN_COLOR), kitConfig); // for backward compatibility
        _getConfig("", "shorts.color", DT_STRING, (DWORD)&kattr_data(kit,ATT_SHORTS_COLOR), kitConfig);
        _getConfig("", "description", DT_STRING, (DWORD)&kattr_data(kit,ATT_DESCRIPTION), kitConfig);

        LOG1S(L"config.txt loaded for {%s}",kit.foldername.c_str());
    }
    else
    {
        GDB_DEBUG(wlog, (slog, L"Unable to find config.txt for %s\n", kit.foldername.c_str()));
        LOG1S1N(L"ERROR: unable to read config.txt for {%s}. Error code = %d",kit.foldername.c_str(), errno);
    }

    kit.configLoaded = true;
}

/**
 * Callback function for reading of config.txt
 */
static void kitConfig(char* pName, const void* pValue, DWORD a)
{
    kattr_data* kd = (kattr_data*)a;
    if (!kd) return;

    switch (kd->attr)
    {
        case ATT_MODEL:
            kd->kit.model = *(DWORD*)pValue;
            kd->kit.attDefined |= MODEL;
            GDB_DEBUG(wlog,(slog,L"model = %d\n",kd->kit.model));
            break;

        case ATT_COLLAR:
            if (equals(pValue, L"yes")) kd->kit.collar = 0;
            else if (equals(pValue, L"no")) kd->kit.collar = 1;
            else if (equals(pValue, L"0")) kd->kit.collar = 0;
            else if (equals(pValue, L"1")) kd->kit.collar = 1;
            else if (equals(pValue, L"2")) kd->kit.collar = 2;
            else if (equals(pValue, L"3")) kd->kit.collar = 3;
            else kd->kit.collar = 1;
            kd->kit.attDefined |= COLLAR;
            GDB_DEBUG(wlog,(slog,L"collar = %d\n",kd->kit.collar));
            break;

        case ATT_SHIRT_NUMBER_LOCATION:
            if (equals(pValue, L"off"))
                kd->kit.shirtNumberLocation = 0;
            else if (equals(pValue, L"center"))
                kd->kit.shirtNumberLocation = 1;
            else if (equals(pValue, L"topright"))
                kd->kit.shirtNumberLocation = 2;
            kd->kit.attDefined |= SHIRT_NUMBER_LOCATION;
            GDB_DEBUG(wlog,(slog,L"shirtNumberLocation = %d\n",kd->kit.shirtNumberLocation));
            break;

        case ATT_SHORTS_NUMBER_LOCATION:
            if (equals(pValue,L"off"))
                kd->kit.shortsNumberLocation = 0;
            else if (equals(pValue,L"left"))
                kd->kit.shortsNumberLocation = 1;
            else if (equals(pValue,L"right"))
                kd->kit.shortsNumberLocation = 2;
            kd->kit.attDefined |= SHORTS_NUMBER_LOCATION;
            GDB_DEBUG(wlog,(slog,L"shortsNumberLocation = %d\n",kd->kit.shortsNumberLocation));
            break;

        case ATT_NAME_LOCATION:
            if (equals(pValue, L"off"))
                kd->kit.nameLocation = 0;
            else if (equals(pValue, L"top"))
                kd->kit.nameLocation = 1;
            else if (equals(pValue, L"bottom"))
                kd->kit.nameLocation = 2;
            kd->kit.attDefined |= NAME_LOCATION;
            GDB_DEBUG(wlog,(slog,L"nameLocation = %d\n",kd->kit.nameLocation));
            break;

        case ATT_NAME_SHAPE:
            if (equals(pValue, L"type1"))
                kd->kit.nameShape = 0;
            else if (equals(pValue, L"type2"))
                kd->kit.nameShape = 1;
            else if (equals(pValue, L"type3"))
                kd->kit.nameShape = 2;
            else if (equals(pValue, L"type4"))
                kd->kit.nameShape = 3;
            kd->kit.attDefined |= NAME_SHAPE;
            GDB_DEBUG(wlog,(slog,L"nameShape = %d\n",kd->kit.nameShape));
            break;

        case ATT_LOGO_LOCATION:
            if (equals(pValue, L"off"))
                kd->kit.logoLocation = 0;
            else if (equals(pValue, L"top"))
                kd->kit.logoLocation = 1;
            else if (equals(pValue, L"bottom"))
                kd->kit.logoLocation = 2;
            kd->kit.attDefined |= LOGO_LOCATION;
            GDB_DEBUG(wlog,(slog,L"logoLocation = %d\n",kd->kit.logoLocation));
            break;

        case ATT_MAIN_COLOR:
            if (ParseColor((wchar_t*)pValue, &kd->kit.mainColor))
                kd->kit.attDefined |= MAIN_COLOR;
            GDB_DEBUG(wlog,(slog,L"mainColor = %02x%02x%02x%02x\n",
                        kd->kit.mainColor.r,
                        kd->kit.mainColor.g,
                        kd->kit.mainColor.b,
                        kd->kit.mainColor.a
                        ));
            break;

        case ATT_SHORTS_COLOR:
            if (ParseColor((wchar_t*)pValue, &kd->kit.shortsFirstColor))
                kd->kit.attDefined |= SHORTS_MAIN_COLOR;
            GDB_DEBUG(wlog,(slog,L"shortsFirstColor = %02x%02x%02x%02x\n",
                        kd->kit.shortsFirstColor.r,
                        kd->kit.shortsFirstColor.g,
                        kd->kit.shortsFirstColor.b,
                        kd->kit.shortsFirstColor.a
                        ));
            break;

        case ATT_DESCRIPTION:
            kd->kit.description = (wchar_t*)pValue;
            GDB_DEBUG(wlog,(slog,L"description = {%s}\n",kd->kit.description));
            break;
    }
}

/**
 * parses a RRGGBB(AA) string into RGBAColor structure
 */
bool ParseColor(const wchar_t* str, RGBAColor* color)
{
	int len = lstrlen(str);
	if (!(len == 6 || len == 8)) 
		return false;

	int num = 0;
	if (swscanf(str,L"%x",&num)!=1) return false;

	if (len == 6) {
		// assume alpha as fully opaque.
		color->r = (BYTE)((num >> 16) & 0xff);
		color->g = (BYTE)((num >> 8) & 0xff);
		color->b = (BYTE)(num & 0xff);
		color->a = 0xff; // set alpha to opaque
	}
	else {
		color->r = (BYTE)((num >> 24) & 0xff);
		color->g = (BYTE)((num >> 16) & 0xff);
		color->b = (BYTE)((num >> 8) & 0xff);
		color->a = (BYTE)(num & 0xff);
	}

	GDB_DEBUG(wlog, (slog, L"RGBA color: %02x,%02x,%02x,%02x\n",
				color->r, color->g, color->b, color->a));
	return true;
}

