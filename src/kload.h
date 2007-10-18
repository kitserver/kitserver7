/* KitServer Loader Header-File */

#define MODID 0
#define NAMELONG L"Module Loader 7.1.0"
#define NAMESHORT L"KLOAD"

#define DEFAULT_DEBUG 0
#define DEFAULT_GDB_DIR L".\\"

#define KEXPORT EXTERN_C __declspec(dllexport)

#ifndef _COMPILING_KLOAD
typedef void  (*PROCESSCONFIG)(char* pName, const void* pValue, DWORD a);
enum {DT_NORMAL, DT_STRING, DT_DWORD, DT_INT, DT_DOUBLE};
#define C_ALL 0x80000000
#endif

// exported functions
KEXPORT PESINFO* getPesInfo();
#define GETPESINFO
KEXPORT void RegisterKModule(KMOD *k);
KEXPORT void getConfig(char* section, char* name, BYTE dataType, DWORD a, PROCESSCONFIG callback);
KEXPORT const wchar_t* getTransl(char* section, char* key);

KEXPORT bool MasterHookFunction(DWORD call_site, DWORD numArgs, void* target);
KEXPORT bool MasterUnhookFunction(DWORD call_site, void* target);
DWORD MasterCallFirst(...);
KEXPORT DWORD MasterCallNext(...);
