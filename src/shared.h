#define UNICODE
#define WINDOW_TITLE L"KitServer 7"
#define KEXPORT EXTERN_C __declspec(dllexport)

using namespace std;
#define MAP_FIND(map,key) (((*(map)).find(key) != (*(map)).end()) ? (*(map))[key] : NULL)
#define MAP_CONTAINS(map,key) (map.find(key)!=map.end())

#define BUFLEN 4096
#define SW sizeof(wchar_t)
#define WBUFLEN (SW * BUFLEN)
#define NULLSTRING L"\0"