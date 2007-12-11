#include <stdio.h>
#include "gdb.h"

#define MAXLINELEN 2048

#ifdef MYDLL_RELEASE_BUILD
#define GDB_DEBUG(f,x)
#define GDB_DEBUG_OPEN(f, dir)
#define GDB_DEBUG_CLOSE(f)
#else
#define GDB_DEBUG(f,x) if (f != NULL) fprintf x
#define GDB_DEBUG_OPEN(f, dir) {\
	char buf[MAXLINELEN]; \
	ZeroMemory(buf, MAXLINELEN); \
	lstrcpy(buf, dir); lstrcat(buf, "GDB.debug.log"); \
	f = fopen(buf, "wt"); \
}
#define GDB_DEBUG_CLOSE(f) if (f != NULL) { fclose(f); f = NULL; }
#endif

#define PLAYERS 0
#define GOALKEEPERS 1

static FILE* klog;

// functions
//////////////////////////////////////////

static void gdbFillKitCollection(GDB* gdb, KitCollection* col, int kitType);
static BOOL ParseColor(char* str, RGBAColor* color);
static BOOL ParseByte(char* str, BYTE* byte);
static int getKeyValuePair(char* buf, char* key, char* value);

/**
 * Allocate and initialize the GDB structure, read the "map.txt" file
 * but don't look for kit folders themselves.
 */
GDB* gdbLoad(char* dir)
{
	GDB_DEBUG_OPEN(klog, dir);
	GDB_DEBUG(klog, (klog, "Loading GDB...\n"));

	// initialize an empty database
	HANDLE heap = GetProcessHeap();
    GDB* gdb = (GDB*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(GDB));
    if (!gdb) {
        GDB_DEBUG(klog, (klog, "Unable to allocate memory for GDB.\n"));
        GDB_DEBUG_CLOSE(klog);
        return NULL;
    }
	strncpy(gdb->dir, dir, MAXFILENAME);
    gdb->uni = new WordKitCollectionMap();

    // process kit map file
    char mapFile[MAXFILENAME];
    ZeroMemory(mapFile,MAXFILENAME);
    sprintf(mapFile, "%sGDB\\uni\\map.txt", gdb->dir);
    FILE* map = fopen(mapFile, "rt");
    if (map == NULL) {
        GDB_DEBUG(klog, (klog, "Unable to find uni-map: %s.\n", mapFile));
        GDB_DEBUG_CLOSE(klog);
        return gdb;
    }

	// go line by line
    char buf[MAXLINELEN];
	while (!feof(map))
	{
		ZeroMemory(buf, MAXLINELEN);
		fgets(buf, MAXLINELEN, map);
		if (lstrlen(buf) == 0) break;

		// strip off comments
		char* comm = strstr(buf, "#");
		if (comm != NULL) comm[0] = '\0';

        // find team id
        WORD teamId = 0xffff;
        if (sscanf(buf, "%d", &teamId)==1) {
            GDB_DEBUG(klog, (klog, "teamId = %d\n", teamId));
            char* foldername = NULL;
            // look for comma
            char* pComma = strstr(buf,",");
            if (pComma) {
                // what follows is the filename.
                // It can be contained within double quotes, so 
                // strip those off, if found.
                char* start = NULL;
                char* end = NULL;
                start = strstr(pComma + 1,"\"");
                if (start) end = strstr(start + 1,"\"");
                if (start && end) {
                    // take what inside the quotes
                    end[0]='\0';
                    foldername = start + 1;
                } else {
                    // just take the rest of the line
                    foldername = pComma + 1;
                }

                GDB_DEBUG(klog, (klog, "foldername = {%s}\n", foldername));

                // make new kit collection
                KitCollection* kitCol = (KitCollection*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(KitCollection));
                if (!kitCol) {
                    GDB_DEBUG(klog, (klog, "Unable to allocate memory for kit collection.\n"));
                    continue;
                }
                strncpy(kitCol->foldername, foldername, MAXFILENAME);
                kitCol->loaded = FALSE;
                kitCol->players = new StringKitMap();
                kitCol->goalkeepers = new StringKitMap();

                // store in the "uni" map
                (*(gdb->uni))[teamId] = kitCol;

                // find kits for this team
                //gdbFindKitsForTeam(gdb, teamId);
            }
        }
    }
    fclose(map);
    //GDB_DEBUG_CLOSE(klog);
    return gdb;
}

/**
 * Enumerate all kits in this team folder.
 * and for each one parse the "config.txt" file.
 */
void gdbFillKitCollection(GDB* gdb, KitCollection* col, int kitType)
{
	WIN32_FIND_DATA fData;
	char pattern[MAXLINELEN];
	ZeroMemory(pattern, MAXLINELEN);

	HANDLE heap = GetProcessHeap();

	lstrcpy(pattern, gdb->dir); 
	if (kitType == PLAYERS) {
		sprintf(pattern + lstrlen(pattern), "GDB\\uni\\%s\\p*", col->foldername);
    } else if (kitType == GOALKEEPERS) {
		sprintf(pattern + lstrlen(pattern), "GDB\\uni\\%s\\g*", col->foldername);
    }

	GDB_DEBUG(klog, (klog, "pattern = {%s}\n", pattern));

	HANDLE hff = FindFirstFile(pattern, &fData);
	if (hff == INVALID_HANDLE_VALUE) 
	{
		// none found.
		return;
	}
	while(true)
	{
        GDB_DEBUG(klog, (klog, "found: {%s}\n", fData.cFileName));
        // check if this is a directory
        if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // make new Kit object
            Kit* kit = (Kit*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(Kit));
            if (!kit)
            {
                GDB_DEBUG(klog, (klog, "Unable to allocate memory for kit object."));
                break;
            }
			ZeroMemory(kit->foldername, MAXFILENAME);
            sprintf(kit->foldername, "GDB\\uni\\%s\\%s", col->foldername, fData.cFileName);

            ZeroMemory(kit->description,MAXFILENAME);

            // read and parse the config.txt
			kit->attDefined = 0;

            string key = fData.cFileName;
            gdbLoadConfig(gdb, key, kit);

            // insert kit object into KitCollection map
            StringKitMap* km = (kitType == PLAYERS) ? col->players : col->goalkeepers;
            (*km)[key] = kit;
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
void gdbFindKitsForTeam(GDB* gdb, WORD teamId)
{
    KitCollection* col = (*(gdb->uni))[teamId];
    if (col && !col->loaded) {
        // players
        gdbFillKitCollection(gdb, col, PLAYERS);
        // goalkeepers
        gdbFillKitCollection(gdb, col, GOALKEEPERS);
        // mark kit collection as loaded
        col->loaded = TRUE;
    }
}

/**
 * Read and parse the config.txt for the given kit.
 */
void gdbLoadConfig(GDB* gdb, string& mykey, Kit* kit)
{
	char cfgFileName[MAXFILENAME];
	ZeroMemory(cfgFileName, MAXFILENAME);
	sprintf(cfgFileName, "%s%s\\config.txt", gdb->dir, kit->foldername);

	FILE* cfg = fopen(cfgFileName, "rt");
	if (cfg == NULL) {
        GDB_DEBUG(klog, (klog, "Unable to find %s\n", cfgFileName));
		return;  // no config.txt file => nothing to do.
    }

	GDB_DEBUG(klog, (klog, "Found %s\n", cfgFileName));
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
            GDB_DEBUG(klog, (klog, "key: {%s} has value: {%s}\n", key, value));
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
}

/**
 * parses a RRGGBB(AA) string into RGBAColor structure
 */
BOOL ParseColor(char* str, RGBAColor* color)
{
	int len = lstrlen(str);
	if (!(len == 6 || len == 8)) 
		return FALSE;

	int num = 0;
	if (sscanf(str,"%x",&num)!=1) return FALSE;

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

	GDB_DEBUG(klog, (klog, "RGBA color: %02x,%02x,%02x,%02x\n",
				color->r, color->g, color->b, color->a));
	return TRUE;
}

// parses a decimal number string into actual BYTE value
BOOL ParseByte(char* str, BYTE* byte)
{
	int num = 0;
	if (sscanf(str,"%d",&num)!=1) return FALSE;
	*byte = (BYTE)num;
	return TRUE;
}

// parses the string into a key-value pair
int getKeyValuePair(char* buf, char* key, char* value)
{
    if (sscanf(buf, "%s", key)==1) {
        // look for comma
        char* delim = strstr(buf,"=");
        if (delim) {
            // what follows is the value.
            // It can be contained within double quotes, so 
            // strip those off, if found.
            char* start = NULL;
            char* end = NULL;
            start = strstr(delim + 1,"\"");
            if (start) end = strstr(start + 1,"\"");
            if (start && end) {
                // take what inside the quotes
                end[0]='\0';
                strncpy(value, start + 1, MAXLINELEN);
                return 2;
            } else {
                // just take the next string value
                return 1 + sscanf(delim + 1, "%s", value);
            }
        } else {
            value[0] = '\0';
            return 1;
        }
    } else {
        key[0] = '\0';
        value[0] = '\0';
    }
    return 0;
}

/**
 * Release the memory allocated for GDB objects.
 */
void gdbUnload(GDB* gdb)
{
    //TODO
    GDB_DEBUG_CLOSE(klog);
}

