// lodmixer.h

#define MODID 101
#define NAMELONG L"LOD Mixer 7.0.0"
#define NAMESHORT L"LODMIXER"
#define DEFAULT_DEBUG 1

#define BUFLEN 4096

typedef struct _DIMENSIONS {
    int width;
    int height;
    float aspectRatio;
} DIMENSIONS;

typedef struct _LMCONFIG {
    DIMENSIONS screen;
} LMCONFIG;

#define DEFAULT_WIDTH 0
#define DEFAULT_HEIGHT 0
#define DEFAULT_ASPECT_RATIO 0.0f

