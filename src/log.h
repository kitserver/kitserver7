#ifndef _LOG_
#define _LOG_

#include "manage.h"

#define KEXPORT EXTERN_C __declspec(dllexport)

#ifndef THISMOD
#define THISMOD NULL
#endif

#define Log(x) _Log(THISMOD,x)
#define LogWithNumber(x,a) _LogWithNumber(THISMOD,x,a)
#define LogWithTwoNumbers(x,a,b) _LogWithTwoNumbers(THISMOD,x,a,b)
#define LogWithThreeNumbers(x,a,b,c) _LogWithThreeNumbers(THISMOD,x,a,b,c)
#define LogWithFourNumbers(x,a,b,c,d) _LogWithFourNumbers(THISMOD,x,a,b,c,d)
#define LogWithString(x,a) _LogWithString(THISMOD,x,a)
#define LogWithTwoStrings(x,a,b) _LogWithTwoStrings(THISMOD,x,a,b)
#define LogWithNumberAndString(x,a,b) _LogWithNumberAndString(THISMOD,x,a,b)
#define LogWithStringAndNumber(x,a,b) _LogWithStringAndNumber(THISMOD,x,a,b)
#define LogWithFloat(x,a) _LogWithFloat(THISMOD,x,a)
#define LogWithFourFloats(x,a,b,c,d) _LogWithFourFloats(THISMOD,x,a,b,c,d)
#define LogWithDouble(x,a) _LogWithDouble(THISMOD,x,a)

#define LOG(x) _Log(THISMOD,x)
#define LOG1N(x,a) _LogWithNumber(THISMOD,x,a)
#define LOG2N(x,a,b) _LogWithTwoNumbers(THISMOD,x,a,b)
#define LOG3N(x,a,b,c) _LogWithThreeNumbers(THISMOD,x,a,b,c)
#define LOG4N(x,a,b,c,d) _LogWithFourNumbers(THISMOD,x,a,b,c,d)
#define LOG1S(x,a) _LogWithString(THISMOD,x,a)
#define LOG2S(x,a,b) _LogWithTwoStrings(THISMOD,x,a,b)
#define LOG1N1S(x,a,b) _LogWithNumberAndString(THISMOD,x,a,b)
#define LOG1S1N(x,a,b) _LogWithStringAndNumber(THISMOD,x,a,b)
#define LOG1F(x,a) _LogWithFloat(THISMOD,x,a)
#define LOG4F(x,a,b,c,d) _LogWithFourFloats(THISMOD,x,a,b,c,d)
#define LOG1D(x,a) _LogWithDouble(THISMOD,x,a)

#ifndef MYDLL_RELEASE_BUILD
#define TRACE(x) Log(x)
#define TRACE1N(x,a) LogWithNumber(x,a)
#define TRACE2N(x,a,b) LogWithTwoNumbers(x,a,b)
#define TRACE3N(x,a,b,c) LogWithThreeNumbers(x,a,b,c)
#define TRACE4N(x,a,b,c,d) LogWithFourNumbers(x,a,b,c,d)
#define TRACE1S(x,a) LogWithString(x,a)
#define TRACE2S(x,a,b) LogWithTwoStrings(x,a,b)
#define TRACE1N1S(x,a,b) LogWithNumberAndString(x,a,b)
#define TRACE1S1N(x,a,b) LogWithStringAndNumber(x,a,b)
#define TRACE1F(x,a) LogWithFloat(x,a)
#define TRACE4F(x,a,b,c,d) LogWithFourFloats(x,a,b,c,d)
#define TRACE1D(x,a) LogWithDouble(x,a)
#else
#define TRACE(x)
#define TRACE1N(x,a)
#define TRACE2N(x,a,b)
#define TRACE3N(x,a,b,c)
#define TRACE4N(x,a,b,c,d)
#define TRACE1S(x,a)
#define TRACE2S(x,a,b)
#define TRACE1N1S(x,a,b)
#define TRACE1S1N(x,a,b)
#define TRACE1F(x,a)
#define TRACE4F(x,a,b,c,d)
#define TRACE1D(x,a)
#endif

KEXPORT void OpenLog(const wchar_t* logName);
KEXPORT void CloseLog();

KEXPORT void _Log(KMOD *caller, const wchar_t* msg);
KEXPORT void _LogWithNumber(KMOD *caller, const wchar_t* msg, DWORD number);
KEXPORT void _LogWithTwoNumbers(KMOD *caller, const wchar_t* msg, DWORD a, DWORD b);
KEXPORT void _LogWithThreeNumbers(KMOD *caller, const wchar_t* msg, DWORD a, DWORD b, DWORD c);
KEXPORT void _LogWithFourNumbers(KMOD *caller, const wchar_t* msg, DWORD a, DWORD b, DWORD c, DWORD d);
KEXPORT void _LogWithString(KMOD *caller, const wchar_t* msg, const wchar_t* str);
KEXPORT void _LogWithTwoStrings(KMOD *caller, const wchar_t* msg, const wchar_t* a, const wchar_t* b);
KEXPORT void _LogWithNumberAndString(KMOD *caller, const wchar_t *msg, DWORD a, const wchar_t* b);
KEXPORT void _LogWithStringAndNumber(KMOD *caller, const wchar_t *msg, const wchar_t* a, DWORD b);
KEXPORT void _LogWithFloat(KMOD *caller, const wchar_t* msg, float number);
KEXPORT void _LogWithFourFloats(KMOD *caller, const wchar_t* msg, float a, float b, float c, float d);
KEXPORT void _LogWithDouble(KMOD *caller, const wchar_t* msg, double number);

#endif
