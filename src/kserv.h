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












#define CODELEN 12
enum {
	C_CONTROLLERFIX1, C_CONTROLLERFIX2, C_QUALITYCHECK1, C_QUALITYCHECK2,
	C_BEGINRENDERPLAYERS_CS, C_BEGINRENDERPLAYERS,
	C_BEGINRENDERPLAYERS2_CS, C_BEGINRENDERPLAYERS2,
	C_ENDRENDERPLAYERS_CS, C_ENDRENDERPLAYERS,
};

#define NOCODEADDR {0,0,0,0,0,0,0,0,0,0,0,0},
DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
	{
		0x86dd12, 0x86dd22, 0x904b0f, 0x904bc5,
		0x9affd8, 0x88d1b0,
		0x8bfc6e, 0x88d1b0,
		0x88a852, 0x889f30,
	},
	// [Settings] PES2008 PC DEMO
  NOCODEADDR
};

#define DATALEN 1
enum {
	DUMMY,	
};

#define NODATAADDR {0},
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
	NODATAADDR
	// [Settings] PES2008 PC DEMO
	NODATAADDR
};

DWORD code[CODELEN];
DWORD data[DATALEN];
