/* Kitserver module */

#include <windows.h>
#include "kserv.h"
#include "kload_exp.h"

#include "d3dx9tex.h"

#include <map>
#include <hash_map>







// VARIABLES
bool allQualities = true;
hash_map<DWORD, bool> savedTextures;
map<DWORD*, DWORD> g_replacedHeaders;
map<DWORD*, DWORD>::iterator g_replacedHeadersIt = NULL;
hash_map<DWORD, TexPlayerInfo*> g_tpi;
hash_map<DWORD, TexPlayerInfo*>::iterator g_tpiIt;
hash_map<DWORD, BYTE> g_tpiNum;

const wchar_t refereeName[] = L"Schiri\0";


// FUNCTIONS
void hookKloadFuncs();
void kloadBeginRenderPlayerMain();
void createTpiMap();
void clearTpiMap();













void hookKloadFuncs() {
	DWORD protection = 0;
	DWORD newProtection = PAGE_EXECUTE_READWRITE;
	BYTE* bptr = NULL;
	
	if (code[C_CONTROLLERFIX1] != 0 && code[C_CONTROLLERFIX2] != 0)	{
		for (int i=0; i<2; i++) {
			bptr = (BYTE*)code[C_CONTROLLERFIX1 + i];
	    if (VirtualProtect(bptr, 7, newProtection, &protection)) {
	    	memset(bptr, 0x90, 7);
			}
		}
	}
	
	if (allQualities && code[C_QUALITYCHECK1] != 0 && code[C_QUALITYCHECK2] != 0)	{
		for (int i=0; i<2; i++) {
			bptr = (BYTE*)code[C_QUALITYCHECK1 + i];
	    if (VirtualProtect(bptr, 2, newProtection, &protection)) {
	    	/* NOP */ bptr[0] = 0x90;
	    	/* Jmp... */ bptr[1] = 0xe9;
			}
		}
	}
		
	if (code[C_BEGINRENDERPLAYERS] != 0 && code[C_BEGINRENDERPLAYERS_CS] != 0)	{
			bptr = (BYTE*)code[C_BEGINRENDERPLAYERS_CS];
	    if (VirtualProtect(bptr, 5, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(code[C_BEGINRENDERPLAYERS_CS] + 1);
	        ptr[0] = (DWORD)kloadBeginRenderPlayer1 - (DWORD)(code[C_BEGINRENDERPLAYERS_CS] + 5);
	    }
		}
		
		if (code[C_BEGINRENDERPLAYERS2] != 0 && code[C_BEGINRENDERPLAYERS2_CS] != 0)	{
			bptr = (BYTE*)code[C_BEGINRENDERPLAYERS2_CS];
	    if (VirtualProtect(bptr, 10, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(code[C_BEGINRENDERPLAYERS2_CS] + 1);
	        ptr[0] = (DWORD)kloadBeginRenderPlayer2 - (DWORD)(code[C_BEGINRENDERPLAYERS2_CS] + 5);
	        // restore the jump
	        bptr[5] = 0xe9;
	        ptr = (DWORD*)(code[C_BEGINRENDERPLAYERS2_CS] + 6);
	        ptr[0] = code[C_BEGINRENDERPLAYERS2] - (DWORD)(code[C_BEGINRENDERPLAYERS2_CS] + 10);
	    }
		}

		if (code[C_ENDRENDERPLAYERS] != 0 && code[C_ENDRENDERPLAYERS_CS] != 0)	{
			bptr = (BYTE*)code[C_ENDRENDERPLAYERS_CS];
	    if (VirtualProtect(bptr, 5, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(code[C_ENDRENDERPLAYERS_CS] + 1);
	        ptr[0] = (DWORD)kloadEndRenderPlayers - (DWORD)(code[C_ENDRENDERPLAYERS_CS] + 5);
	    }
		}
}




void DumpTexture(IDirect3DTexture9* const ptexture, DWORD a) 
{
    wchar_t buf[BUFLEN];
    swprintf(buf, L"kitserver\\tex-%08x-%08x.bmp", a, (DWORD)ptexture);
    D3DXSaveTextureToFile(buf, D3DXIFF_BMP, ptexture, NULL);
}



void setTextureHeaderAddr(DWORD* p1, IDirect3DTexture9* tex) {
	if (!p1) return;
	
	// save the value
	g_replacedHeadersIt = g_replacedHeaders.find(p1);
	// only save if this header hasn't been replaced yet
	if (g_replacedHeadersIt ==  g_replacedHeaders.end()) {
		g_replacedHeaders[p1] = *p1;
	}
	
	// create a buffer and set it as the new value
	DWORD value = (DWORD)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x24);
	*(DWORD*)(value + 8) = (DWORD)tex;
	*p1 = value;
	return;
}





void kloadBeginRenderPlayerMain(DWORD coll) {
	
	createTpiMap();
	
	TexPlayerInfo* tpi = MAP_FIND(&g_tpi, coll);
	if (tpi) {
		if (tpi->referee == 0) {
			//MessageBox(0, tpi->playerName, "\0", 0);
		} else {
			//MessageBox(0, refereeName, "\0", 0);
			
	int abc = 5;
	
	DWORD playerColl = *(DWORD*)(coll + 0x444);
	if (playerColl != 0) {
		DWORD playerColl1 = *(DWORD*)(playerColl + 0x164);
		DWORD num = *(DWORD*)(playerColl + 0x15c);
		if (playerColl1 != 0) {

			for (int i=0; i<num; i++) {
				DWORD playerColl2 = playerColl1 + i * 4;
				DWORD playerColl3 = *(DWORD*)playerColl2;
				if (playerColl3 != 0) {
					IDirect3DTexture9* tex = (IDirect3DTexture9*)(*(DWORD*)(playerColl3 + 0x20));
					if (tex && !MAP_CONTAINS(savedTextures, playerColl^(DWORD)tex)) {
							DumpTexture(tex, playerColl);
							savedTextures[playerColl^(DWORD)tex] = true;
							//*(DWORD*)(playerColl3 + 0x20) = 0;
					}
				}
				
				setTextureHeaderAddr((DWORD*)playerColl2, NULL);
			}
/*
			if (abc < num) {
				DWORD playerColl2 = playerColl1 + abc * 4;
				DWORD playerColl3 = *(DWORD*)playerColl2;

				if (playerColl3 != 0) {
					IDirect3DTexture9* tex = (IDirect3DTexture9*)(*(DWORD*)(playerColl3 + 0x20));
					if (tex && !MAP_CONTAINS(savedTextures, playerColl^(DWORD)tex)) {
							D3DSURFACE_DESC desc;
							tex->GetLevelDesc(0, &desc);
							
							DumpTexture(tex, desc.Width);
							savedTextures[playerColl^(DWORD)tex] = true;
					}
				}

				setTextureHeaderAddr((DWORD*)playerColl2, NULL);
			}	
*/
		}
	}
			
			
			
		}
	}

	/*
	DWORD* playersArr = *(DWORD**)0xd6b0f4;
	if (playersArr != NULL) {
		 playersArr += 0x737; // 0x737 = 0x1cdc / 4
		DWORD temp = playersArr[0];
		if (temp != 0) {
			temp = *(DWORD*)(temp + 0x2c);
			temp = *(DWORD*)(temp + 0x140);
			temp = *(DWORD*)(temp + 0x64);
			temp = *(DWORD*)(temp + 0x454);
			DWORD playerColl = *(DWORD*)(temp + 0x444);
			if (playerColl != 0) {
				DWORD playerColl1 = *(DWORD*)(playerColl + 0x164);
				DWORD num = *(DWORD*)(playerColl + 0x15c);
				if (playerColl1 != 0) {
					if (1 < num) {
						DWORD playerColl2 = playerColl1 + 1 * 4;
						DWORD playerColl3 = *(DWORD*)playerColl2;
						if (playerColl3 != 0) {
							IDirect3DTexture9* tex = (IDirect3DTexture9*)(*(DWORD*)(playerColl3 + 0x20));
							if (tex && !MAP_CONTAINS(savedTextures, playerColl^(DWORD)tex)) {
									//D3DSURFACE_DESC desc;
									//tex->GetLevelDesc(0, &desc);
									
									DumpTexture(tex, playerColl);
									savedTextures[playerColl^(DWORD)tex] = true;
							}
						}
					}
				}
			}
		}
	}
	*/
	return;
}

void kloadBeginRenderPlayer1() {
	DWORD _ECX;
	__asm mov _ECX, ecx
	
	kloadBeginRenderPlayerMain(_ECX);
	
	BEGINRENDERPLAYER1 beginRenderPlayers = (BEGINRENDERPLAYER1)code[C_BEGINRENDERPLAYERS];
	
	__asm mov ecx, _ECX
	beginRenderPlayers();
	
	return;
}

void kloadBeginRenderPlayer2() {
	DWORD _ECX;
	__asm mov _ECX, ecx
	
	kloadBeginRenderPlayerMain(_ECX);
	
	// esi is overwritten somehow during the function, so we take ebp
	__asm mov ecx, _ECX
	return;
}

void kloadEndRenderPlayers() {
	DWORD _EDI;
	__asm mov _EDI, edi
	
		// free all replaced buffers and reset them to the original ones
	for (g_replacedHeadersIt = g_replacedHeaders.begin(); 
			g_replacedHeadersIt != g_replacedHeaders.end();
			g_replacedHeadersIt++)
		{
			HeapFree(GetProcessHeap(), 0, (VOID*)*(g_replacedHeadersIt->first));
			*(g_replacedHeadersIt->first) = g_replacedHeadersIt->second;
		}
	g_replacedHeaders.clear();
	
	// clear the texture-player-info-map
	clearTpiMap();
	
	ENDRENDERPLAYERS endRenderPlayers = (ENDRENDERPLAYERS)code[C_ENDRENDERPLAYERS];
	__asm mov edi, _EDI
	endRenderPlayers();
	
	return;
}




void createTpiMap() {
	DWORD* startVal = *(DWORD**)(0xd6dcd4);
	DWORD* nextVal = (DWORD*)*startVal;
	nextVal = (DWORD*)*nextVal;
	
	while (nextVal != startVal) {
		DWORD temp = *(nextVal + 2);
		nextVal = (DWORD*)*nextVal;
		
		if (!temp) continue;
		
		DWORD temp2 = *(DWORD*)(temp + 0x64);
		if (!temp2) continue;
		DWORD* texArr = *(DWORD**)(temp2 + 0x454);
		if (!texArr) continue;
			
		// create a structure with the player data on first use
		TexPlayerInfo* tpi = (TexPlayerInfo*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TexPlayerInfo));
		
		bool isReferee = (*(DWORD*)temp == 0xa86470);
		if (!isReferee) {
			DWORD playerName = *(DWORD*)(temp + 0x534);
			DWORD playerInfo = playerName - 0x11c;

			tpi->playerName = (char*)playerName;
		}
			
		tpi->dummy = 123;
		tpi->referee = isReferee?1:0;
		//

		for (int i = 0; i < 9; i++)  {
			DWORD temp3 = texArr[i];
			if (!temp3) continue;
			// temp3 is finally a pointer to the structure used in kloadBeginRenderPlayerMain
			
			// save it in the map to connect the structure to the player data there
			g_tpi[temp3] = tpi;
			g_tpiNum[temp3] = i;
		}
	}
}

void clearTpiMap() {
	for (g_tpiIt = g_tpi.begin(); 
			g_tpiIt != g_tpi.end();
			g_tpiIt++)
		{
			TexPlayerInfo* tpi = g_tpiIt->second;
			HeapFree(GetProcessHeap(), 0, tpi);
		}
		
	g_tpi.clear();
	g_tpiNum.clear();
}