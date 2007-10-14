#define UNICODE

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "utf8.h"
#include "lang.h"

#include <string>
#include <hash_map>

bool langLoaded = false;
hash_map<string, wstring> g_transl;
hash_map<string, wstring>::iterator g_translIt;
const wchar_t noTransl[] = L"(Translation missing)";

void setStandardTransl() {
	//
}

void clearTransl() {
	g_transl.clear();
	setStandardTransl();
}

const wchar_t* getTransl(char* section, char* key) {
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, "[%s]%s", section, key);
	string sKey = buf;

	if (!MAP_CONTAINS(g_transl, sKey)) return noTransl;
	return g_transl[sKey].c_str();
}

void readLangFile() {
	wchar_t langName[] = L"eng";
	wchar_t langFile[BUFLEN];
	ZeroMemory(langFile, WBUFLEN);
	swprintf(langFile, L".\\lang_%s.txt", langName);

	FILE* f = _wfopen(langFile, L"rb");
	
	if (!f) return;
		
	clearTransl();
	
	DWORD firstDWORD;
	bool unicodeFile = false;
	fgets((char*)&firstDWORD, 4, f);
	if ((firstDWORD & 0xffffff) == 0xbfbbef) {
		// UTF8
		fseek(f, 3, SEEK_SET);
	} else if ((firstDWORD & 0xffff) == 0xfeff) {
		// Unicode Little Endian
		unicodeFile = true;
		fseek(f, 2, SEEK_SET);
	} else {
		// no supported BOM detected, asume UTF8
		fseek(f, 0, SEEK_SET);
	}
	
	wchar_t currSection[256] = {'\0'};;

	char a_str[BUFLEN];
	wchar_t str[BUFLEN];
	wchar_t buf[BUFLEN];
	wchar_t val[BUFLEN];
	wchar_t *pFirst = NULL, *pSecond = NULL, *comment = NULL, *pSpace = NULL, *pEq = NULL;
	while (!feof(f))
	{
		if (!unicodeFile) {
			ZeroMemory(str, WBUFLEN);
			ZeroMemory(a_str, BUFLEN);
			fgets(a_str, BUFLEN-1, f);
			Utf8::fUtf8ToUnicode(str, a_str);
		} else {
			ZeroMemory(str, WBUFLEN);
			fgetws(str, BUFLEN-1, f);
		}
		ZeroMemory(val, WBUFLEN);
		
		pFirst = pSecond = NULL;
		
		if (str[0] == '[') {
			pFirst = str + 1;
			pSecond = wcsstr(pFirst, L"]");
			if (!pSecond) continue;
			*pSecond = '\0';
			
			wcscpy(currSection, pFirst);												
			continue;
		}
		
		pFirst = wcsstr(str, L"$$");
		if (!pFirst) continue;
		pFirst += 2;

		pSpace = wcsstr(str, L" ");
		pEq = wcsstr(str, L" ");
		if (!pSpace || pEq < pSpace)  pSpace = pEq;
		if (pSpace > pFirst)
			continue;
		else
			*pSpace = '\0';
			
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, L"[%s]%s", currSection, str);
		char* buf2 = Utf8::unicodeToAnsi(buf);
		string sKey = buf2;
		Utf8::free(buf2);
			
		pSecond = wcsstr(pFirst, L"$$");
		while (!pSecond && !feof(f))
		{
			wcscat(val, pFirst);
			
			if (!unicodeFile) {
				ZeroMemory(str, WBUFLEN);
				ZeroMemory(a_str, BUFLEN);
				fgets(a_str, BUFLEN-1, f);
				Utf8::fUtf8ToUnicode(str, a_str);
			} else {
				ZeroMemory(str, WBUFLEN);
				fgetws(str, BUFLEN-1, f);
			}
			
			pFirst = str;
			pSecond = wcsstr(pFirst, L"$$");
		}
		*pSecond = '\0';
		wcscat(val, pFirst);
		
		wstring sVal = val;
		g_transl[sKey] = sVal;
	}
	fclose(f);
	
	return;
}