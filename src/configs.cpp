#define UNICODE

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "utf8.h"
#include "configs.h"

#include <map>
#include <string>

multimap<string, wstring> g_knownConfigs;
multimap<string, wstring>::iterator g_knownConfigsIt;

bool readConfig(wchar_t* cfgFile)
{
	FILE* f = _wfopen(cfgFile, L"rb");
	if (!f) return false;

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
	
	char currSection[64] = {'\0'};
	
	char a_str[BUFLEN];
	wchar_t str[BUFLEN];
	char name[BUFLEN];
	wchar_t buf[BUFLEN];

	wchar_t *pComment = NULL, *pName = NULL, *pValue = NULL, *pSpace = NULL, *pEq = NULL, *pTemp = NULL;

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
		
		if (str[0] == '[') {
			wchar_t* pFirst = str + 1;
			wchar_t* pSecond = wcsstr(pFirst, L"]");
			if (!pSecond) continue;
			*pSecond = '\0';
			
			Utf8::fUnicodeToAnsi(currSection, pFirst);
			continue;
		}
		
		// skip comments
		pComment = wcsstr(str, L"#");
		if (pComment != NULL) pComment[0] = '\0';
			
		// parse the line
		pName = pValue = NULL;
		pEq = wcsstr(str, L"=");
		if (pEq == NULL || pEq[1] == '\0') continue;

		pEq[0] = '\0';
		pName = str;
		pValue = pEq + 1;
		
		ZeroMemory(name, BUFLEN);
		ZeroMemory(buf, WBUFLEN);
		swscanf(pName, L"%s", buf);
		Utf8::fUnicodeToAnsi(name, buf);
		
		char buf2[BUFLEN];
		ZeroMemory(buf2, BUFLEN);
		sprintf(buf2, "[%s]%s", currSection, name);
		string sKey = buf2;
		
		while (*pValue == ' ')
			pValue++;
			
		pTemp = pValue + wcslen(pValue) - 1;
		while (*pTemp == (wchar_t)13 || *pTemp == (wchar_t)10) {
			*pTemp = 0;
			pTemp--;
		}

		wstring sVal = pValue;

		g_knownConfigs.insert(pair<string,wstring>(sKey,sVal));
	}
	
	return true;
}

void _getConfig(char* section, char* name, BYTE dataType, DWORD a, PROCESSCONFIG callback) {

	if (!section || !name || !callback) return;
		
	wchar_t *startQuote, *endQuote;
	char buf[BUFLEN];
	wchar_t buf2[BUFLEN];
	DWORD dValue;
	int iValue;
	double dbValue;
		
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, "[%s]%s", section, name);
	string sKey = buf;	
	
	pair<multimap<string, wstring>::iterator,multimap<string, wstring>::iterator> ret;
	ret = g_knownConfigs.equal_range(sKey);
	int i = g_knownConfigs.count(sKey);
	
	for (g_knownConfigsIt = ret.first; g_knownConfigsIt != ret.second; g_knownConfigsIt++, i--) {
		// the highest bit of a is an indicator whether to return all values for this setting or not
		if (((a & C_ALL) == 0)  && (i != 1)) continue;
		
		const wchar_t* value = g_knownConfigsIt->second.c_str();
	
		switch (dataType) {
			case DT_STRING:
				startQuote = wcsstr(value, L"\"");
				if (startQuote == NULL) {
					callback(buf, value, a & 0x7fffffff);
					continue;
				}
				endQuote = wcsstr(startQuote + 1, L"\"");
				if (endQuote == NULL) {
					callback(buf, value, a & 0x7fffffff);
					continue;
				}

				ZeroMemory(buf2, WBUFLEN);
				memcpy(buf2, startQuote + 1, SW*(endQuote - startQuote - 1));
				
				callback(buf, buf2, a & 0x7fffffff);
				break;
				
			case DT_DWORD:
				if (swscanf(value, L"%d", &dValue) != 1) continue;
				callback(buf, &dValue, a & 0x7fffffff);
				break;
				
			case DT_INT:
				if (swscanf(value, L"%d", &iValue) != 1) continue;
				callback(buf, &iValue, a & 0x7fffffff);
				break;
				
			case DT_DOUBLE:
				if (swscanf(value, L"%f", &dbValue) != 1) continue;
				callback(buf, &dbValue, a & 0x7fffffff);
				break;
			
			case DT_NORMAL:
			default:
				callback(buf, value, a & 0x7fffffff);
		}
			/*	
				*/
	}
	return;
}