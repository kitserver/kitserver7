// lodmixer.h

#define MODID 101
#define NAMELONG L"LOD Mixer 7.1.0"
#define NAMESHORT L"LODMIXER"
#define DEFAULT_DEBUG 0

#define BUFLEN 4096

typedef struct _DIMENSIONS {
    int width;
    int height;
    float aspectRatio;
} DIMENSIONS;

typedef struct _LOD {
    float switch1;
    float switch2;
} LOD;

typedef struct _LMCONFIG {
    DIMENSIONS screen;
    LOD lod;
    bool aspectRatioCorrectionEnabled;
    bool controllerCheckEnabled;
} LMCONFIG;

#define DEFAULT_WIDTH 0
#define DEFAULT_HEIGHT 0
#define DEFAULT_ASPECT_RATIO 0.0f
#define DEFAULT_ASPECT_RATIO_CORRECTION_ENABLED true
#define DEFAULT_LOD_SWITCH1 0.0f
#define DEFAULT_LOD_SWITCH2 0.0f
#define GAME_DEFAULT_LOD_SWITCH1 0.095f
#define GAME_DEFAULT_LOD_SWITCH2 0.074f
#define DEFAULT_CONTROLLER_CHECK_ENABLED false

