#ifndef __JUCE_GDB__
#define __JUCE_GDB__

#include <windows.h>
#include <map>
#include <hash_map>
#include <string>

using namespace std;
#if _CPPLIB_VER >= 503
using namespace stdext;
#endif

// attribute definition flags (bits)
#define SHIRT_NUMBER  0x01 
#define SHIRT_NAME    0x02 
#define SHORTS_NUMBER 0x04 
#define COLLAR        0x08 
#define MODEL         0x10 
#define CUFF          0x20 
#define SHORTS_NUMBER_LOCATION 0x40 
#define NUMBER_TYPE   0x80
#define NAME_TYPE     0x100
#define NAME_SHAPE    0x200
#define NUMBERS_FILE  0x400
#define NUMBERS_PALETTE_FILE 0x800
#define NAME_LOCATION 0x1000
#define LOGO_LOCATION 0x2000
#define MAIN_COLOR   0x4000
#define MASK_FILE     0x8000
#define KITDESCRIPTION   0x10000
#define SHIRT_NUMBER_LOCATION 0x20000
#define SHORTS_MAIN_COLOR 0x40000
#define SHIRT_FOLDER 0x80000
#define SHORTS_FOLDER 0x100000
#define SOCKS_FOLDER 0x200000
#define OVERLAY_FILE 0x400000

// GDB data structures
///////////////////////////////

class RGBAColor {
public:
    BYTE r;
    BYTE g;
    BYTE b;
    BYTE a;
};

class Kit {
public:
    wstring foldername;
    wstring description;
    BYTE collar;
    BYTE model;
    BYTE shirtNumberLocation;
    BYTE shortsNumberLocation;
    BYTE numberType;
    BYTE nameLocation;
    BYTE nameType;
    BYTE nameShape;
    BYTE logoLocation;
    RGBAColor mainColor;
    RGBAColor shortsFirstColor;
    //wstring maskFile;
    //wstring shirtFolder;
    //wstring shortsFolder;
    //wstring socksFolder;
    //wstring overlayFile;
    DWORD attDefined;
    bool configLoaded;

    Kit() : attDefined(0), configLoaded(false) {}
};

class KitCollection {
public:
    wstring foldername;
    map<wstring,Kit> players;
    map<wstring,Kit> goalkeepers;
    BOOL loaded;

    KitCollection(const wstring& fname) : foldername(fname), loaded(false) {}
};

class GDB {
public:
    wstring dir;
    hash_map<WORD,KitCollection> uni;
    KitCollection dummyHome;
    KitCollection dummyAway;
    bool readConfigs;

    GDB(const wstring& gdir, bool rc=true) : 
        dir(gdir), 
        dummyHome(L""), 
        dummyAway(L""), 
        readConfigs(rc)
    { load(); }
    void loadConfig(Kit& kit);

private:
    void load();
    void findKitsForTeam(WORD teamId);
    void fillKitCollection(KitCollection& col, int kitType);
};

#endif
