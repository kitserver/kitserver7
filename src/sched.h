// sched.h

#pragma pack(4)

#define MODID 131
#define NAMELONG L"Scheduler 7.3.1.1"
#define NAMESHORT L"SCHED"
#define DEFAULT_DEBUG 0

#define ROUND_MASK_2LEGS 0x04
#define ROUND_MASK_SINGLE 0xfb

typedef struct _PATCH_INFO
{
    DWORD addr;
    DWORD offset;
    DWORD size;
    DWORD op;
    DWORD value;
} PATCH_INFO;

typedef struct _TOURNAMENT_HEADER
{
    BYTE unknown1[14];
    BYTE tournamentType;
    BYTE unknown2;
    WORD mainTeam;
    WORD numTeams;
    BYTE unknown3;
    BYTE unknown4;
    BYTE unknown5;
    BYTE numTeams2;
    BYTE unknown6[8];
    BYTE padding[0x5880];
} TOURNAMENT_HEADER;

typedef struct _TOURNAMENT_PARAMS
{
    BYTE unknown1[5];
    BYTE p1;
    BYTE p2;
    BYTE p3;
    BYTE homeAway;
    BYTE unknown2[7];
    BYTE padding[0x3b880];
} TOURNAMENT_PARAMS;

typedef struct _MATCH
{
    BYTE home;
    BYTE away;
} MATCH;

typedef struct _GROUP_PARAMS
{
    BYTE gamesPlayed;
    BYTE unknown1;
    BYTE numTeams;
    BYTE numGames;
    BYTE numGamesTotal;
    BYTE numPerDay;
    BYTE numToQualify;
    BYTE unknown2;
    BYTE unknown3;
    BYTE hasHuman;
    BYTE unknown4;
    BYTE numTeams2;
} GROUP_PARAMS;

typedef struct _MATCHES_SECTION
{
    MATCH match[1];
    BYTE padding[0x80a];
} MATCHES_SECTION;

typedef struct _GROUP_PARAMS_SECTION
{
    GROUP_PARAMS group[1];
    BYTE padding[0x1ed2];
} GROUP_PARAMS_SECTION;

typedef struct _GROUP_STAGE
{
    BYTE unknown1[14];
    WORD numGroups;
    BYTE unknown2[6];
    MATCHES_SECTION matches;
    GROUP_PARAMS_SECTION groups;
} GROUP_STAGE;

typedef struct _PLAYOFFS_PARAMS
{
    BYTE unknown1[12];
    WORD gamesPlayed;
    WORD unknown2;
    WORD unknown3;
    WORD numMatchups;
    WORD unknown4;
    WORD numGamesTotal;
    BYTE unknown5[8];
} PLAYOFFS_PARAMS;

typedef struct _BRACKET_ENTRY
{
    BYTE round;
    BYTE unknown1;
    BYTE winnerOfMatchA;
    BYTE winnerOfMatchB;
    BYTE unknown2[20];
    WORD roundCode;
} BRACKET_ENTRY;

typedef struct _BRACKET
{
    BRACKET_ENTRY entries[1];
    BYTE padding[0x12a4];
} BRACKET;

typedef struct _PLAYOFFS
{
    BYTE unknown1[4];
    WORD hasPlayoffs;
    BYTE matches[1];
    BYTE padding1[0x171];
    MATCH teamPairs[1];
    BYTE padding2[0xb8];
    BRACKET bracket;
    PLAYOFFS_PARAMS params;
} PLAYOFFS;

typedef struct _EXTENSION
{
    BYTE enabled;
    BYTE numGamesInG;
    BYTE numGamesInF;
    BYTE numGamesInSF;
    BYTE numGamesInQF;
    BYTE numGamesIn16;
} EXTENSION;

typedef struct _SCHEDULE_STRUCT
{
    TOURNAMENT_HEADER header;
    TOURNAMENT_PARAMS params;
    GROUP_STAGE groupStage;
    PLAYOFFS playoffs;
    BYTE padding[0x20];
    EXTENSION ext;
} SCHEDULE_STRUCT;

