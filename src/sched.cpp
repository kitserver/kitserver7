// lodmixer.cpp
#define UNICODE
#define THISMOD &k_sched

#include <windows.h>
#include <stdio.h>
#include "kload_exp.h"
#include "sched.h"
#include "sched_addr.h"
#include "dllinit.h"

#define COLOR_BLACK D3DCOLOR_RGBA(0,0,0,128)
#define COLOR_AUTO D3DCOLOR_RGBA(135,135,135,255)
#define COLOR_CHOSEN D3DCOLOR_RGBA(210,210,210,255)
#define COLOR_INFO D3DCOLOR_RGBA(0xb0,0xff,0xb0,0xff)

KMOD k_sched={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};
HINSTANCE hInst;
HHOOK g_hKeyboardHook = NULL;
bool g_showScheduleExt = false;
EXTENSION g_ext;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void initSched();
void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops, bool addRetn=false);
void HookKeyboard();
void UnhookKeyboard();
LRESULT CALLBACK KeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void schedPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
bool OkToChangeGroups(SCHEDULE_STRUCT* ss);
bool OkToChangePlayoffs(SCHEDULE_STRUCT* ss);

void schedAtCheckNumGamesCallPoint();
void schedAfterWrotePlayoffsCallPoint();
KEXPORT DWORD schedAtCheckNumGames(SCHEDULE_STRUCT* ss);
KEXPORT void schedAfterWrotePlayoffs(bool checkType=true);


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		LOG(L"Attaching dll...");
		hInst=hInstance;
		RegisterKModule(&k_sched);

		copyAdresses();
		hookFunction(hk_D3D_Create, initSched);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		LOG(L"Detaching dll...");
        unhookFunction(hk_D3D_Present, schedPresent);
        UnhookKeyboard();
	}
	return true;
}

void HookKeyboard()
{
    if (g_hKeyboardHook == NULL) 
    {
		g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, hInst, GetCurrentThreadId());
		TRACE1N(L"Installed keyboard hook: g_hKeyboardHook = %d", (DWORD)g_hKeyboardHook);
	}
}

void UnhookKeyboard()
{
	if (g_hKeyboardHook != NULL) 
    {
		UnhookWindowsHookEx(g_hKeyboardHook);
		TRACE(L"Keyboard hook uninstalled.");
		g_hKeyboardHook = NULL;
	}
}

void initSched()
{
    // skip the settings-check call
    LOG(L"Initializing Scheduler...");

    g_ext.enabled = 0;
    g_ext.numGamesInG = 3;
    g_ext.numGamesInF = 1;
    g_ext.numGamesInSF = 1;
    g_ext.numGamesInQF = 1;
    g_ext.numGamesIn16 = 1;

    // apply patches
    for (int i=0; i<DATALEN/5; i++)
    {
        PATCH_INFO patchInfo;
        memcpy(&patchInfo, &data[i*5], sizeof(PATCH_INFO));

        BYTE* bptr = (BYTE*)(patchInfo.addr + patchInfo.offset);
        DWORD protection;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) 
        {
            DWORD value = 0;
            switch (patchInfo.op)
            {
                case OP_REPLACE:
                    memcpy(bptr, &patchInfo.value, patchInfo.size);
                    break;
                case OP_ADD:
                    memcpy(&value, bptr, patchInfo.size);
                    value += patchInfo.value;
                    memcpy(bptr, &value, patchInfo.size);
                    break;
                case OP_SUB:
                    memcpy(&value, bptr, patchInfo.size);
                    value -= patchInfo.value;
                    memcpy(bptr, &value, patchInfo.size);
                    break;
            }
        }
    }

    HookCallPoint(code[C_NUM_GAMES_CHECK], schedAtCheckNumGamesCallPoint, 6, 2);
    HookCallPoint(code[C_WROTE_PLAYOFFS], schedAfterWrotePlayoffsCallPoint, 6, 0, true);

    // hook keyboard
    HookKeyboard();
    hookFunction(hk_D3D_Present, schedPresent);

    LOG(L"Initialization complete.");
    unhookFunction(hk_D3D_Create, initSched);
}

void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops, bool addRetn)
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
            if (addRetn)
                bptr[5+numNops]=0xc3;
	        TRACE2N(L"Function (%08x) HOOKED at address (%08x)", target, addr);
	    }
	}
}

int GetRounds(SCHEDULE_STRUCT* ss)
{
    if (ss->header.numTeams>16) return 4;
    else if (ss->header.numTeams>8) return 3;
    else if (ss->header.numTeams>4) return 2;
    else if (ss->header.numTeams>2) return 1;
    return 0;
}

bool OkToChangeGroups(SCHEDULE_STRUCT* ss)
{
    if (ss->header.tournamentType==7 && ss->groupStage.groups.group[0].gamesPlayed==0)
        return true;
    return false;
}

bool OkToChangePlayoffs(SCHEDULE_STRUCT* ss)
{
    if (ss->header.tournamentType==8 && ss->playoffs.params.gamesPlayed==0)
        return true;
    else if (ss->header.tournamentType==7 && ss->playoffs.params.gamesPlayed==0)
        return true;
    return false;
}

void schedAtCheckNumGamesCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        mov byte ptr ds:[edx+eax*4],bl  // execute replaced code
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push edi // parameter: pointer to cup-struct
        call schedAtCheckNumGames
        add esp,0x04     // pop parameters
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

void schedAfterWrotePlayoffsCallPoint()
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
        mov edi,1  // parameter: type-check
        push edi
        call schedAfterWrotePlayoffs
        add esp,4 // pop params
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

/**
 * returns: number of group games to play.
 */
KEXPORT DWORD schedAtCheckNumGames(SCHEDULE_STRUCT* ss)
{
    /*
    LOG1N(L"Tournament type: %d",ss->header.tournamentType);
    LOG1N(L"Home/Away: %d",ss->params.homeAway);
    LOG1N(L"Groups: %d",ss->groupStage.numGroups);
    for (int i=0; i<ss->groupStage.numGroups; i++)
    {
        LOG4N(L"Group %d: teams=%d, matches=%d, matchesTotal=%d", i,
                ss->groupStage.groups.group[i].numTeams,
                ss->groupStage.groups.group[i].numGames,
                ss->groupStage.groups.group[i].numGamesTotal);
    }
    if (ss->playoffs.hasPlayoffs)
    {
        int i = 0;
        BYTE *m = ss->playoffs.matches;
        do
        {
            LOG1N(L"Playoff match #d: %d", m[i]);
        } while (m[i++]!=0);
    }
    if (ss->playoffs.params.numMatchups > 0)
    {
        LOG2N(L"Playoffs: matchups=%d, matchesTotal=%d",
                ss->playoffs.params.numMatchups,
                ss->playoffs.params.numGamesTotal);
        for (int i=0; i<ss->playoffs.params.numMatchups; i++)
        {
            LOG2N(L"Matchup: %d vs. %d", 
                    ss->playoffs.teamPairs[i].home,
                    ss->playoffs.teamPairs[i].away);
        }
    }
    */

    // read extended schedule info
    memcpy(&g_ext, &ss->ext, sizeof(EXTENSION));

    // check of schedule extension is enabled
    if (ss->ext.enabled)
    {
        // return extended number of games 
        return ss->ext.numGamesInG;
    }

    // default behaviour
    return max(ss->groupStage.groups.group[0].numTeams-1, 3);
}

KEXPORT void schedAfterWrotePlayoffs(bool checkType)
{
    LOG(L"Writing playoffs data");

    SCHEDULE_STRUCT* ss = *(SCHEDULE_STRUCT**)code[D_SCHEDULE_STRUCT_PTR];
    if (ss) 
    {
        LOG1N(L"checkType = %d",checkType);
        LOG1N(L"ss->header.tournamentType = %d",ss->header.tournamentType);

        if (checkType && (ss->header.tournamentType!=7 && ss->header.tournamentType!=8))
            return; // don't change normal knock-out competitions, unless explicitly told so.

        LOG(L"Applying schedule extensions.");

        // step 1: make sure we restore out extension data
        memcpy(&ss->ext, &g_ext, sizeof(EXTENSION));

        // step 2: adjust the playoff settings
        int newNumGames = ss->ext.numGamesInF;
        if (ss->header.numTeams>4) newNumGames += ss->ext.numGamesInSF*2;
        if (ss->header.numTeams>8) newNumGames += ss->ext.numGamesInQF*4;
        if (ss->header.numTeams>16) newNumGames += ss->ext.numGamesIn16*8;

        int rounds = GetRounds(ss);

        // 2a: bracket
        int m = 0;
        while (ss->playoffs.bracket.entries[m].round != 0)
        {
            //LOG1N(L"roundCode = %02x", ss->playoffs.bracket.entries[m].roundCode);
            //LOG1N(L"idx = %d", rounds - ss->playoffs.bracket.entries[m].round);
            switch (rounds - ss->playoffs.bracket.entries[m].round)
            {
                case 0: // final
                    if (g_ext.numGamesInF==2)
                        ss->playoffs.bracket.entries[m].roundCode |= ROUND_MASK_2LEGS;
                    else
                        ss->playoffs.bracket.entries[m].roundCode &= ROUND_MASK_SINGLE;
                    break;
                case 1: // semi-final
                    if (g_ext.numGamesInSF==2)
                        ss->playoffs.bracket.entries[m].roundCode |= ROUND_MASK_2LEGS;
                    else
                        ss->playoffs.bracket.entries[m].roundCode &= ROUND_MASK_SINGLE;
                    break;
                case 2: // quater-final
                    if (g_ext.numGamesInQF==2)
                        ss->playoffs.bracket.entries[m].roundCode |= ROUND_MASK_2LEGS;
                    else
                        ss->playoffs.bracket.entries[m].roundCode &= ROUND_MASK_SINGLE;
                    break;
                case 3: // round-of-16
                    if (g_ext.numGamesIn16==2)
                        ss->playoffs.bracket.entries[m].roundCode |= ROUND_MASK_2LEGS;
                    else
                        ss->playoffs.bracket.entries[m].roundCode &= ROUND_MASK_SINGLE;
                    break;
            }
            m++;
        }

        // 2b: schedule
        ZeroMemory(ss->playoffs.matches,64);
        int k = 0;
        for (int i=1; i<rounds+1; i++)
        {
            int loops = 1;
            switch (rounds - i)
            {
                case 0: // final
                    loops = g_ext.numGamesInF;
                    break;
                case 1: // semi-final
                    loops = g_ext.numGamesInSF;
                    break;
                case 2: // quater-final
                    loops = g_ext.numGamesInQF;
                    break;
                case 3: // round-of-16
                    loops = g_ext.numGamesIn16;
                    break;
            }
            for (int n=0; n<loops; n++)
                for (int m=0; m<ss->playoffs.params.numMatchups; m++)
                {
                    if (ss->playoffs.bracket.entries[m].round != i)
                        continue;

                    ss->playoffs.matches[k++] = m;
                }
        }

        // 2c: number of games
        ss->playoffs.params.numGamesTotal = newNumGames;
    }
}

void schedPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused)
{
    if (g_showScheduleExt)
    {
        SCHEDULE_STRUCT* ss = *(SCHEDULE_STRUCT**)code[D_SCHEDULE_STRUCT_PTR];
        if (ss && (ss->header.tournamentType==7 || ss->header.tournamentType==8)
                && (ss->header.unknown1[0]!=0))
        {
            wchar_t* patterns[] = {L"F: %d", L"SF: %d", L"QF: %d", L"1/8: %d"};
            int rounds = GetRounds(ss);
            if (rounds>1)
            {
                wchar_t wbuf[12] = {0};
                if (ss->header.tournamentType==7 || ss->header.tournamentType==8)
                {
                    if (ss->ext.numGamesInG>0)
                        swprintf(wbuf, L"G: %d", ss->ext.numGamesInG);
                    else
                        swprintf(wbuf, L"G: 3");
                    KDrawText(wbuf, 7, 307, COLOR_BLACK, 26.0f);
                    if (OkToChangeGroups(ss))
                        KDrawText(wbuf, 5, 305, COLOR_INFO, 26.0f);
                    else
                        KDrawText(wbuf, 5, 305, COLOR_AUTO, 26.0f);
                }

                BYTE* num = (BYTE*)&ss->ext.numGamesInF;
                for (int i=rounds-1, j=1; i>=0; i--,j++)
                {
                    if (num[i]>0)
                        swprintf(wbuf, patterns[i], num[i]);
                    else
                        swprintf(wbuf, patterns[i], 1);
                    KDrawText(wbuf, 7, 307+j*20, COLOR_BLACK, 26.0f);
                    if (OkToChangePlayoffs(ss))
                        KDrawText(wbuf, 5, 305+j*20, COLOR_INFO, 26.0f);
                    else
                        KDrawText(wbuf, 5, 305+j*20, COLOR_AUTO, 26.0f);
                }
            }
        } // end if (ss)
    }
}

LRESULT CALLBACK KeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	if (code1 >= 0 && code1==HC_ACTION && lParam & 0x80000000) 
    {
		if (wParam == 0x70) 
        { // trigger schedule extensions
            LOG(L"F1 pressed.");
            SCHEDULE_STRUCT* ss = *(SCHEDULE_STRUCT**)code[D_SCHEDULE_STRUCT_PTR];
            if (ss)
            {
                g_showScheduleExt = !g_showScheduleExt;
            }
        }
        else if (wParam == 0x71)
        {
            LOG(L"F2 pressed.");
            SCHEDULE_STRUCT* ss = *(SCHEDULE_STRUCT**)code[D_SCHEDULE_STRUCT_PTR];
            if (g_showScheduleExt && ss && (OkToChangeGroups(ss) || OkToChangePlayoffs(ss)))
            {
                // toggle 1-3-1-1-1-1 and 1-6-1-2-2-2 configurations
                g_ext.enabled = 1;
                if (OkToChangeGroups(ss))
                {
                    g_ext.numGamesInG = (ss->ext.numGamesInG==6)?3:6;

                    // apply group league settings
                    // step 1: num of matches
                    for (int i=0; i<ss->groupStage.numGroups; i++)
                    {
                        ss->groupStage.groups.group[i].numGames = g_ext.numGamesInG;
                        ss->groupStage.groups.group[i].numGamesTotal = g_ext.numGamesInG;
                    }

                    // step 2: schedule: extended/plain
                    if (g_ext.numGamesInG == 3)
                    {
                        // blank-out fixtures 4-6
                        for (int i=6; i<12; i++)
                        {
                            ss->groupStage.matches.match[i].home = 0xff;
                            ss->groupStage.matches.match[i].away = 0xff;
                        }
                    }
                    else if (g_ext.numGamesInG == 6)
                    {
                        // add fixtures 4-6
                        ss->groupStage.matches.match[6].home = ss->groupStage.matches.match[4].away;
                        ss->groupStage.matches.match[6].away = ss->groupStage.matches.match[4].home;
                        ss->groupStage.matches.match[7].home = ss->groupStage.matches.match[5].away;
                        ss->groupStage.matches.match[7].away = ss->groupStage.matches.match[5].home;
                        ss->groupStage.matches.match[8].home = ss->groupStage.matches.match[0].away;
                        ss->groupStage.matches.match[8].away = ss->groupStage.matches.match[0].home;
                        ss->groupStage.matches.match[9].home = ss->groupStage.matches.match[1].away;
                        ss->groupStage.matches.match[9].away = ss->groupStage.matches.match[1].home;
                        ss->groupStage.matches.match[10].home = ss->groupStage.matches.match[2].away;
                        ss->groupStage.matches.match[10].away = ss->groupStage.matches.match[2].home;
                        ss->groupStage.matches.match[11].home = ss->groupStage.matches.match[3].away;
                        ss->groupStage.matches.match[11].away = ss->groupStage.matches.match[3].home;
                    }
                }
                if (OkToChangePlayoffs(ss))
                {
                    g_ext.numGamesInF = (ss->ext.numGamesInF==1)?1:1;
                    g_ext.numGamesInSF = (ss->ext.numGamesInSF==2)?1:2;
                    g_ext.numGamesInQF = (ss->ext.numGamesInQF==2)?1:2;
                    g_ext.numGamesIn16 = (ss->ext.numGamesIn16==2)?1:2;

                    // apply playoff settings
                    schedAfterWrotePlayoffs(false);
                }

                memcpy(&ss->ext, &g_ext, sizeof(EXTENSION));
            }
        }
    }	

	return CallNextHookEx(g_hKeyboardHook, code1, wParam, lParam);
}

