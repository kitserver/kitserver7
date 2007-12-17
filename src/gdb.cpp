#define UNICODE

#include <stdio.h>
#include <string>
#include "gdb.h"
#include "configs.h"

#ifdef MYDLL_RELEASE_BUILD
#define GDB_DEBUG(f,x)
#define GDB_DEBUG_OPEN(f,dir)
#define GDB_DEBUG_CLOSE(f)
#else
#define GDB_DEBUG(f,x) if (f) fwprintf x
#define GDB_DEBUG_OPEN(f,dir) {\
    f = _wfopen((dir + L"GDB.debug.log").c_str(),L"wt");\
}
#define GDB_DEBUG_CLOSE(f) if (f) fclose(f)
#endif

#define PLAYERS 0
#define GOALKEEPERS 1

FILE* wlog;

// functions
//////////////////////////////////////////

static bool ParseColor(wchar_t* str, RGBAColor* color);
static bool ParseByte(wchar_t* str, BYTE* byte);

/**
 * Allocate and initialize the GDB structure, read the "map.txt" file
 * but don't look for kit folders themselves.
 */
void GDB::load()
{
	GDB_DEBUG_OPEN(wlog,this->dir);
	GDB_DEBUG(wlog,(wlog,L"Loading GDB...\n"));

    // process kit map file
    hash_map<WORD,wstring> mapFile;
    if (!readMap((this->dir + L"GDB\\uni\\map.txt").c_str(), mapFile))
    {
        GDB_DEBUG(wlog,(wlog,L"Unable to find uni-map: %s\n",mapFile));
        return;
    }

    for (hash_map<WORD,wstring>::iterator it = mapFile.begin(); it != mapFile.end(); it++)
    {
        KitCollection kitCol(it->second);

        // strip off quotes, if present
        if (kitCol.foldername[0]=='"' || kitCol.foldername[0]=='\'')
            kitCol.foldername.erase(0,1);
        int last = kitCol.foldername.length()-1;
        if (kitCol.foldername[last]=='"' || kitCol.foldername[last]=='\'')
            kitCol.foldername.erase(last);

        GDB_DEBUG(wlog,(wlog,L"teamId = {%d}, foldername = {%s}\n",
                    it->first, kitCol.foldername.c_str()));

        // store in the "uni" map
        this->uni.insert(pair<WORD,KitCollection>(it->first,kitCol));

        // find kits for this team
        this->findKitsForTeam(it->first);
    }

	GDB_DEBUG(wlog,(wlog,L"Loading GDB complete.\n"));
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

	GDB_DEBUG(wlog,(wlog, L"pattern = {%s}\n",pattern));

	HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
	if (hff == INVALID_HANDLE_VALUE) 
	{
		// none found.
		return;
	}
	while(true)
	{
        GDB_DEBUG(wlog,(wlog,L"found: {%s}\n",fData.cFileName));
        // check if this is a directory
        if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            Kit kit;
            kit.foldername = L"GDB\\uni\\" + col.foldername + L"\\" + wstring(fData.cFileName);

            // read and parse the config.txt
			kit.attDefined = 0;

            wstring key(fData.cFileName);
            this->loadConfig(key, kit);

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
        it->second.loaded = TRUE;
    }
}

/**
 * Read and parse the config.txt for the given kit.
 */
void GDB::loadConfig(wstring& mykey, Kit& kit)
{
    /*
	char cfgFileName[MAXFILENAME];
	ZeroMemory(cfgFileName, MAXFILENAME);
	sprintf(cfgFileName, "%s%s\\config.txt", gdb->dir, kit->foldername);

	FILE* cfg = fopen(cfgFileName, "rt");
	if (cfg == NULL) {
        GDB_DEBUG(wlog, (wlog, L"Unable to find %s\n", cfgFileName));
		return;  // no config.txt file => nothing to do.
    }

	GDB_DEBUG(wlog, (wlog, L"Found %s\n", cfgFileName));
    HANDLE heap = GetProcessHeap();

	// go line by line
	while (!feof(cfg))
	{
		char buf[MAXLINELEN];
		ZeroMemory(buf, MAXLINELEN);
		fgets(buf, MAXLINELEN, cfg);
		if (lstrlen(buf) == 0) break;

		// strip off comments
		char* comm = strstr(buf, "#");
		if (comm != NULL) comm[0] = '\0';

		// look for attribute definitions
        char key[80]; ZeroMemory(key, 80);
        char value[MAXLINELEN]; ZeroMemory(value, MAXLINELEN);

        if (getKeyValuePair(buf,key,value)==2)
        {
            GDB_DEBUG(wlog, (wlog, L"key: {%s} has value: {%s}\n", key, value));
            RGBAColor color;

            if (lstrcmp(key, "model")==0)
            {
                if (ParseByte(value, &kit->model))
                    kit->attDefined |= MODEL;
            }
            else if (lstrcmp(key, "collar")==0)
            {
                kit->collar = (lstrcmp(value,"yes")==0) ? 0 : 1;
                kit->attDefined |= COLLAR;
            }
            else if (lstrcmp(key, "shirt.number.location")==0)
            {
                if (lstrcmp(value,"off")==0) {
                    kit->shirtNumberLocation = 0;
                    kit->attDefined |= SHIRT_NUMBER_LOCATION;
                }
                else if (lstrcmp(value,"center")==0) {
                    kit->shirtNumberLocation = 1;
                    kit->attDefined |= SHIRT_NUMBER_LOCATION;
                }
                else if (lstrcmp(value,"topright")==0) {
                    kit->shirtNumberLocation = 2;
                    kit->attDefined |= SHIRT_NUMBER_LOCATION;
                }
            }
            else if (lstrcmp(key, "shorts.number.location")==0)
            {
                if (lstrcmp(value,"off")==0) {
                    kit->shortsNumberLocation = 0;
                    kit->attDefined |= SHORTS_NUMBER_LOCATION;
                }
                else if (lstrcmp(value,"left")==0) {
                    kit->shortsNumberLocation = 1;
                    kit->attDefined |= SHORTS_NUMBER_LOCATION;
                }
                else if (lstrcmp(value,"right")==0) {
                    kit->shortsNumberLocation = 2;
                    kit->attDefined |= SHORTS_NUMBER_LOCATION;
                }
                else if (lstrcmp(value,"both")==0) {
                    kit->shortsNumberLocation = 3;
                    kit->attDefined |= SHORTS_NUMBER_LOCATION;
                }
            }
            else if (lstrcmp(key, "name.location")==0)
            {
                if (lstrcmp(value,"off")==0) {
                    kit->nameLocation = 0;
                    kit->attDefined |= NAME_LOCATION;
                }
                else if (lstrcmp(value,"top")==0) {
                    kit->nameLocation = 1;
                    kit->attDefined |= NAME_LOCATION;
                }
                else if (lstrcmp(value,"bottom")==0) {
                    kit->nameLocation = 2;
                    kit->attDefined |= NAME_LOCATION;
                }
            }
            else if (lstrcmp(key, "logo.location")==0)
            {
                if (lstrcmp(value,"off")==0) {
                    kit->logoLocation = 0;
                    kit->attDefined |= LOGO_LOCATION;
                }
                else if (lstrcmp(value,"top")==0) {
                    kit->logoLocation = 1;
                    kit->attDefined |= LOGO_LOCATION;
                }
                else if (lstrcmp(value,"bottom")==0) {
                    kit->logoLocation = 2;
                    kit->attDefined |= LOGO_LOCATION;
                }
            }
            else if (lstrcmp(key, "name.shape")==0)
            {
                if (lstrcmp(value, "type1")==0) {
                    kit->nameShape = 0;
                    kit->attDefined |= NAME_SHAPE;
                }
                else if (lstrcmp(value, "type2")==0) {
                    kit->nameShape = 1;
                    kit->attDefined |= NAME_SHAPE;
                }
                else if (lstrcmp(value, "type3")==0) {
                    kit->nameShape = 2;
                    kit->attDefined |= NAME_SHAPE;
                }
            }
			else if (lstrcmp(key, "radar.color")==0)
            {
                if (ParseColor(value, &kit->radarColor))
                    kit->attDefined |= RADAR_COLOR;
            }
			else if (lstrcmp(key, "shorts.color")==0)
            {
                if (ParseColor(value, &kit->shortsMainColor))
                    kit->attDefined |= SHORTS_MAIN_COLOR;
            }
            //else if (lstrcmp(key, "mask")==0)
            //{
            //    ZeroMemory(kit->maskFile,MAXFILENAME);
            //    strncpy(kit->maskFile, value, MAXFILENAME);
            //    kit->attDefined |= MASK_FILE;
            //}
			else if (lstrcmp(key, "description")==0)
            {
                ZeroMemory(kit->description,MAXFILENAME);
                strncpy(kit->description, value, MAXFILENAME);
                kit->attDefined |= KITDESCRIPTION;
            }

            // Kit-mixing attributes. Unsupported for now.
            //else if (lstrcmp(key, "shirt.folder")==0)
            //{
            //    ZeroMemory(kit->shirtFolder, 256);
            //    strncpy(kit->shirtFolder, value, 256);
            //    kit->attDefined |= SHIRT_FOLDER;
            //}
			//else if (lstrcmp(key, "shorts.folder")==0)
            //{
            //    ZeroMemory(kit->shortsFolder, 256);
            //    strncpy(kit->shortsFolder, value, 256);
            //    kit->attDefined |= SHORTS_FOLDER;
            //}
			//else if (lstrcmp(key, "socks.folder")==0)
            //{
            //    ZeroMemory(kit->socksFolder, 256);
            //    strncpy(kit->socksFolder, value, 256);
            //    kit->attDefined |= SOCKS_FOLDER;
            //}
            //else if (lstrcmp(key, "overlay")==0)
            //{
            //    ZeroMemory(kit->overlayFile, MAXFILENAME);
            //    strncpy(kit->overlayFile, value, MAXFILENAME);
            //    kit->attDefined |= OVERLAY_FILE;
            //}

            // legacy attributes. Unsupported or irrelevant
            //else if (lstrcmp(key, "cuff")==0)
            //{
            //    kit->cuff = (lstrcmp(value,"yes")==0) ? 1 : 0;
            //    kit->attDefined |= CUFF;
            //}
        }
		// go to next line
	}

	fclose(cfg);
    */
}

/**
 * parses a RRGGBB(AA) string into RGBAColor structure
 */
bool ParseColor(wchar_t* str, RGBAColor* color)
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

	GDB_DEBUG(wlog, (wlog, L"RGBA color: %02x,%02x,%02x,%02x\n",
				color->r, color->g, color->b, color->a));
	return true;
}

// parses a decimal number string into actual BYTE value
bool ParseByte(wchar_t* str, BYTE* byte)
{
	int num = 0;
	if (swscanf(str,L"%d",&num)!=1) return false;
	*byte = (BYTE)num;
	return true;
}
