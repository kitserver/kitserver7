// log.cpp
#define UNICODE
#include <windows.h>
#include <stdio.h>
#include "shared.h"
#include "log.h"
#include "utf8.h"

static HANDLE mylog = INVALID_HANDLE_VALUE;

//////////////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////////////


// Creates log file
void OpenLog(const wchar_t* logName)
{
		mylog = CreateFile(logName,                    // file to create 
					 GENERIC_WRITE,                    // open for writing 
					 FILE_SHARE_READ | FILE_SHARE_DELETE,   // do not share 
					 NULL,                             // default security 
					 CREATE_ALWAYS,                    // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,            // normal file 
					 NULL);                            // no attr. template 
					 
		if (mylog == INVALID_HANDLE_VALUE) return;
		DWORD wbytes;
		BYTE buf[3] = {0xef, 0xbb, 0xbf};		// UTF8
		WriteFile(mylog, (LPVOID)buf, 3,(LPDWORD)&wbytes, NULL);
}

// Closes log file
void CloseLog()
{
	if (mylog != INVALID_HANDLE_VALUE) CloseHandle(mylog);
	mylog = INVALID_HANDLE_VALUE;
}

// Simple logger
KEXPORT void _Log(KMOD *caller, const wchar_t *msg)
{
	if (!caller || mylog == INVALID_HANDLE_VALUE) return;
	/*#ifdef MYDLL_RELEASE_BUILD
	if (caller->debug < 1) return;
	#endif*/
	
	DWORD wbytes;
	wchar_t buf[BUFLEN];
	ZeroMemory(buf, WBUFLEN);
	
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		swprintf(buf, L"{%s} %s\r\n", caller->nameShort, msg);
		BYTE* utf8Buf = Utf8::unicodeToUtf8(buf);
		WriteFile(mylog,(LPVOID)utf8Buf, Utf8::byteLength(utf8Buf),(LPDWORD)&wbytes, NULL);
		Utf8::free(utf8Buf);
	}
}

// Simple logger 2
KEXPORT void _LogWithNumber(KMOD *caller, const wchar_t *msg, DWORD number)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, number);
		_Log(caller, buf);
	}
}

// Simple logger
KEXPORT void _LogWithTwoNumbers(KMOD *caller, const wchar_t *msg, DWORD a, DWORD b)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, a, b);
		_Log(caller, buf);
	}
}

// Simple debugging logger
KEXPORT void _LogWithThreeNumbers(KMOD *caller, const wchar_t *msg, DWORD a, DWORD b, DWORD c)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, a, b, c);
		_Log(caller, buf);
	}
}

// Simple debugging logger
KEXPORT void _LogWithFourNumbers(KMOD *caller, const wchar_t *msg, DWORD a, DWORD b, DWORD c, DWORD d)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, a, b, c, d);
		_Log(caller, buf);
	}
}

// Simple logger 4
KEXPORT void _LogWithString(KMOD *caller, const wchar_t *msg, const wchar_t* str)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, str);
		_Log(caller, buf);
	}
}

// Simple logger 5
KEXPORT void _LogWithTwoStrings(KMOD *caller, const wchar_t *msg, const wchar_t* a, const wchar_t* b)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, a, b);
		_Log(caller, buf);
	}
}

// Simple logger 6
KEXPORT void _LogWithNumberAndString(KMOD *caller, const wchar_t *msg, DWORD a, const wchar_t* b)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, a, b);
		_Log(caller, buf);
	}
}

// Simple logger 7
KEXPORT void _LogWithStringAndNumber(KMOD *caller, const wchar_t *msg, const wchar_t* a, DWORD b)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, a, b);
		_Log(caller, buf);
	}
}

// Simple logger 2
KEXPORT void _LogWithFloat(KMOD *caller, const wchar_t *msg, float number)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, number);
		_Log(caller, buf);
	}
}

// Simple debugging logger
KEXPORT void _LogWithFourFloats(KMOD *caller, const wchar_t *msg, float a, float b, float c, float d)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, a, b, c, d);
		_Log(caller, buf);
	}
}

// Simple logger 3
KEXPORT void _LogWithDouble(KMOD *caller, const wchar_t *msg, double number)
{
	wchar_t buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, WBUFLEN);
		swprintf(buf, msg, number);
		_Log(caller, buf);
	}
}
