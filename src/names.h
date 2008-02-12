// names.h
#include <windows.h>
#include <hash_map>

typedef struct _SONG_STRUCT
{
    WORD binId;
    WORD unknown1;
    char* title;
    char* author;
} SONG_STRUCT;

class song_map_t
{
public:
    song_map_t(const wstring& filename);
    ~song_map_t();

    hash_map<WORD,SONG_STRUCT> _songMap;
};

typedef struct _BALL_STRUCT
{
    WORD number;
    char* name;
} BALL_STRUCT;

class ball_map_t
{
public:
    ball_map_t(const wstring& filename);
    ~ball_map_t();

    hash_map<WORD,BALL_STRUCT> _ballMap;
};

