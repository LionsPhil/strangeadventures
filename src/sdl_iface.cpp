/*
code to handle all/most of the interaction with the win32 system

- event handling
- kb and mouse input
*/

// INCLUDES ///////////////////////////////////////////////
#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>   // include important windows stuff
#include <windowsx.h>
#include <mmsystem.h>
#include <conio.h>
#include <malloc.h>
#include <io.h>

#endif

#include <stdexcept>

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <SDL.h>

#include "typedefs.h"
#include "iface_globals.h"
#include "gfx.h"
#include "snd.h"
#include "scaledvideo.hpp"

// DEFINES ////////////////////////////////////////////////

// MACROS /////////////////////////////////////////////////

// these read the keyboard asynchronously
#define KEY_DOWN(vk_code) ((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)
#define KEY_UP(vk_code)   ((GetAsyncKeyState(vk_code) & 0x8000) ? 0 : 1)

// TYPES //////////////////////////////////////////////////

typedef unsigned short USHORT;
typedef unsigned short WORD;
typedef unsigned char  UCHAR;
typedef unsigned char  BYTE;

typedef struct {
	int32 start, freq;
} t_ik_timer;

// PROTOTYPES /////////////////////////////////////////////

#ifdef MOVIE
extern int movrecord;
#endif

// GLOBALS ////////////////////////////////////////////////

SDL_Surface *sdlsurf;
extern t_paletteentry pe[256];
SDL_Rect g_native_resolution;
SDL_Rect g_virtual_resolution;
ScaledVideo* g_scaled_video;

char buffer[80];                // used to print text
int IsMinimized = 0;
int ActiveApp = 0;
int SwitchMode = 0;

int ik_mouse_x;
int ik_mouse_y;
int ik_mouse_b;
int ik_mouse_c;
int must_quit;
int wants_screenshot;

int key_left=SDLK_LEFT;
int key_right=SDLK_RIGHT;
int key_up=SDLK_UP;
int key_down=SDLK_DOWN;
int key_f[10];
int key_fire1=SDLK_TAB;
int key_fire2=SDLK_RETURN;
int key_fire2b=SDLK_SPACE;

char ik_inchar;
uint8 *keystate;

t_ik_timer ik_timer[10];

// FUNCTIONS //////////////////////////////////////////////

void eventhandler()
{
	SDL_Event event;
	int b;
	Sint16 true_x, true_y, virtual_x, virtual_y;

	keystate = SDL_GetKeyState(NULL);

	while ( SDL_PollEvent(&event) )
	{
		switch (event.type)
		{
			case SDL_KEYDOWN:
			switch(event.key.keysym.sym){
				case SDLK_F12:
					wants_screenshot=1;
					break;
				case SDLK_F2:
				case SDLK_RCTRL:
				case SDLK_LCTRL:
					settings.opt_mousemode ^= 4;
					Play_SoundFX(WAV_LOCK,0);
					break;
				case SDLK_F11:
					gfx_fullscreen = !gfx_fullscreen;
					gfx_resize();
					break;
				case SDLK_ESCAPE :
					must_quit=1;
					break;
			default:
				break;
			}

			ik_inchar = event.key.keysym.unicode & 0xff;
			break;

			case SDL_MOUSEBUTTONDOWN:
				b = (event.button.button == SDL_BUTTON_LEFT) +
						2*(event.button.button == SDL_BUTTON_RIGHT) +
						4*(event.button.button == SDL_BUTTON_MIDDLE);
				ik_mouse_c = b;
				ik_mouse_b |= b;
			case SDL_MOUSEMOTION:
				true_x = event.motion.x;
				true_y = event.motion.y;
				g_scaled_video->mapTrueToVirtual(
					true_x, true_y,
					&virtual_x, &virtual_y);
				ik_mouse_x = virtual_x;
				ik_mouse_y = virtual_y;
				break;

			case SDL_MOUSEBUTTONUP:
				b = (event.button.button == SDL_BUTTON_LEFT) +
						2*(event.button.button == SDL_BUTTON_RIGHT) +
						4*(event.button.button == SDL_BUTTON_MIDDLE);
				ik_mouse_b &= (7-b);
				break;

			case SDL_ACTIVEEVENT:
			ActiveApp = event.active.gain;
			if (ActiveApp)
			{
				gfx_redraw = 1;
			}
			break;

			case SDL_VIDEORESIZE:
				/* DON'T change gfx_width/height; those are
				 * virtual dimensions */
				gfx_window_width  = event.resize.w;
				gfx_window_height = event.resize.h;
				gfx_resize();
				/* fallthrough */
			case SDL_VIDEOEXPOSE:
				ActiveApp = 1;
				break;

			case SDL_QUIT:
				must_quit = 1;
				break;
			default:
				break;
		}
	}
}


// WINX GAME PROGRAMMING CONSOLE FUNCTIONS ////////////////

int Game_Init(void *parms)
{
	int x;

	for (x=0;x<10;x++)
		key_f[x]=SDLK_F1+x;

	return(1);
}

int Game_Shutdown(void *parms)
{
	return(1);
}


///////////////////////////////////////////////////////////

// call eventhandler once every frame
// to check if windows is trying to kill you (or other events)
int ik_eventhandler()
{
	eventhandler();

	if (must_quit)
		return 1;

	return 0;
}

// read key
int key_pressed(int vk_code)
{
	if (keystate)
		return keystate[vk_code];
	else
		return 0;
}

int ik_inkey()
{
	char c=ik_inchar;

	ik_inchar=0;
	return c;
}

int ik_mclick()
{
	char c=ik_mouse_c&3;

	ik_mouse_c=0;
	return c;
}

// cheesy timer functions
void start_ik_timer(int n, int f)
{
	ik_timer[n].start=SDL_GetTicks();
	ik_timer[n].freq=f;
}

void set_ik_timer(int n, int v)
{
	int x;

	x=SDL_GetTicks();
	ik_timer[n].start=x-ik_timer[n].freq*v;
}

int get_ik_timer(int n)
{
	int x;

	if (ik_timer[n].freq)
	{
		x=SDL_GetTicks();
		return ((x-ik_timer[n].start)/ik_timer[n].freq);
	}

	return 0;
}

int get_ik_timer_fr(int n)
{
	int x;

	if (ik_timer[n].freq)
	{
		x=SDL_GetTicks();
		return ((x-ik_timer[n].start)*256/ik_timer[n].freq);
	}

	return 0;
}

void ik_showcursor()
{
	SDL_ShowCursor(1);
}

void ik_hidecursor()
{
	SDL_ShowCursor(0);
}

void gfx_resize() {
	int w, h;
	int flags = SDL_SWSURFACE | SDL_HWPALETTE;
	ScaledVideo* old_scaler = g_scaled_video;

	if(gfx_fullscreen) {
		w = g_native_resolution.w;
		h = g_native_resolution.h;
		// Add ANYFORMAT since we don't want to *force* 8bpp
		flags |= SDL_FULLSCREEN | SDL_ANYFORMAT;
	} else {
		w = gfx_window_width;
		h = gfx_window_height;
		if(w < gfx_width ) { w = gfx_width;  }
		if(h < gfx_height) { h = gfx_height; }
		flags |= SDL_RESIZABLE;
	}

	try {
		g_scaled_video = get_scaled_video(sdlsurf, w, h, 8, flags);

		delete old_scaler;

		std::string scaler_description = g_scaled_video->describe();
		fprintf(stderr, "Using scaling technique: %s\n", scaler_description.c_str());

	} catch (std::runtime_error& re) {
		fprintf(stderr, "Couldn't change scaler: %s!\n", re.what());
	}
}
