/* KitServer Loader Header-File */

#define KEXPORT EXTERN_C __declspec(dllexport)

KEXPORT PESINFO* getPesInfo();
#define GETPESINFO
KEXPORT void RegisterKModule(KMOD *k);