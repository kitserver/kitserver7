#ifndef _SETUP_MAIN_
#define _SETUP_MAIN_

#ifdef MYDLL_RELEASE_BUILD
#define SETUP_WINDOW_TITLE "KitServer 7 Setup"
#else
#define SETUP_WINDOW_TITLE "KitServer 7 Setup (debug build)"
#endif
#define CREDITS "About: v7.0.0.0a (09/2007) by Juce and Robbie."

#define LOG(f,x) if (f != NULL) fprintf x

#endif
