CC=cl
LINK=link
RC=rc

CFLAGS=/nologo /GX $(EXTRA_CFLAGS) /DUSE_HASHMAPS
LFLAGS=/NOLOGO /NODEFAULTLIB:libcmt.lib
LIBS=user32.lib gdi32.lib advapi32.lib
LIBSDLL=pngdib.obj libpng.lib zdll.lib $(LIBS)

INCPNGDIB=soft\pngdib
LPPNGDIB=soft\pngdib 
LPLIBPNG=soft\libpng-1.2.8-lib\lib
LPZLIB=soft\zlib123-dll\lib

all: dlls apps
	if exist output (rd /S /Q output)
	mkdir output
	copy *.dll output
	copy setup.exe output
	copy lang_*.txt output
	
all_dlls: dlls
	if exist output (rd /S /Q output)
	mkdir output
	copy *.dll output
	copy lang_*.txt output
	
all_apps: apps
	if exist output (rd /S /Q output)
	mkdir output
	copy setup.exe output
	copy lang_*.txt output

dlls: kload.dll kset.dll
apps: setup.exe

release:
	$(MAKE) EXTRA_CFLAGS=/DMYDLL_RELEASE_BUILD

setup: setup.exe
kset: kset.dll
kload: kload.dll

imageutil.obj: imageutil.cpp imageutil.h
detect.obj: detect.cpp detect.h
lang.obj: lang.cpp lang.h
configs.obj: configs.cpp configs.h


kload.lib: kload.obj
kload.dll: kload.lib detect.obj imageutil.obj lang.obj log.obj configs.obj kload.res
	$(LINK) $(LFLAGS) /out:kload.dll /DLL kload.obj detect.obj imageutil.obj lang.obj log.obj configs.obj kload.res $(LIBS) d3dx9.lib
kload.obj: kload.cpp kload.h

kload.res: 
	$(RC) -r -fo kload.res kload.rc
	

kset.lib: kset.obj
kset.dll: kset.lib detect.obj imageutil.obj configs.obj lang.obj kset.res
	$(LINK) $(LFLAGS) /out:kset.dll /DLL kset.obj detect.obj imageutil.obj configs.obj lang.obj kset.res $(LIBS)
kset.obj: kset.cpp kset.h

kset.res: 
	$(RC) -r -fo kset.res kset.rc


setupgui.obj: setupgui.cpp setupgui.h
setup.obj: setup.cpp setup.h setupgui.h
setup.exe: setup.obj detect.obj setupgui.obj imageutil.obj lang.obj configs.obj setup.res
	$(LINK) $(LFLAGS) /out:setup.exe setup.obj detect.obj setupgui.obj imageutil.obj lang.obj configs.obj setup.res $(LIBS) shell32.lib
	
setup.res: 
	$(RC) -r -fo setup.res setup.rc
	
.cpp.obj:
	$(CC) $(CFLAGS) -c $(INC) /I $(INCPNGDIB) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe
	(cd soft\pngdib && $(MAKE) clean)
	if exist output (rd /S /Q output)

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe *.log *~
	(cd soft\pngdib && $(MAKE) clean-all)
	if exist output (rd /S /Q output)
	
