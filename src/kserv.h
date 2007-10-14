void kloadBeginRenderPlayer1();
void kloadBeginRenderPlayer2();
void kloadEndRenderPlayers();

typedef void  (*BEGINRENDERPLAYER1)();
typedef void  (*ENDRENDERPLAYERS)();


typedef struct _TexPlayerInfo {
	DWORD dummy;
	BYTE referee;
	char* playerName;
} TexPlayerInfo;