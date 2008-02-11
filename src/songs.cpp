#define UNICODE
#define THISMOD &k_afs

#include <stdio.h>
#include <string>
#include "gdb.h"
#include "configs.h"
#include "utf8.h"
#include "log.h"
#include "songs.h"

extern KMOD k_afs;

song_map_t::~song_map_t()
{
    for (hash_map<WORD,SONG_STRUCT>::iterator it = _songMap.begin(); it != _songMap.end(); it++)
    {
        Utf8::free(it->second.title);
        Utf8::free(it->second.author);
    }
}

song_map_t::song_map_t(const wstring& filename)
{
    // process kit map file
    hash_map<WORD,wstring> mapFile;
    if (!readMap(filename.c_str(), mapFile))
    {
        LOG1S(L"Unable to read songs map (%s)\n",filename.c_str());
        return;
    }

    for (hash_map<WORD,wstring>::iterator it = mapFile.begin(); it != mapFile.end(); it++)
    {
        // determine song name, author name
        wstring title;
        wstring author;

        wstring& line = it->second;
        int qt1 = line.find('"');
        int qt2 = line.find('"',qt1+1);
        int qt3 = line.find('"',qt2+1);
        int qt4 = line.find('"',qt3+1);

        if (qt1!=string::npos && qt2!=string::npos && qt3!=string::npos && qt4!=string::npos)
        {
            title = line.substr(qt1+1,qt2-qt1-1);
            author = line.substr(qt3+1,qt4-qt3-1);
        }
        else
        {
            title = line;
            author = L"Author unknown";
        }

        SONG_STRUCT ss;
        ss.binId = it->first;
        ss.title = Utf8::unicodeToAnsi(title.c_str());
        ss.author = Utf8::unicodeToAnsi(author.c_str());
        _songMap.insert(pair<WORD,SONG_STRUCT>(it->first,ss));
    }
}

