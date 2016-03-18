#include <SDL.h>
#include <SDL_mixer.h>


#include "typedefs.h"
#include "gfx.h"
#include "scaledvideo.hpp"

int my_main();
int sound_init();

extern SDL_Surface *sdlsurf;
extern SDL_Rect g_native_resolution;
extern SDL_Rect g_virtual_resolution;
extern ScaledVideo* g_scaled_video;

int main(int argc, char *argv[])
{
	gfx_width=640; gfx_height=480;
	gfx_fullscreen=0;

	c_minx=0;
	c_miny=0;
	c_maxx=gfx_width;
	c_maxy=gfx_height;

#ifndef WINDOWS
	fprintf(stderr, "Strange Adventures in Infinite Space\n");
	fprintf(stderr, "Unofficial fork by Philip Boulain et. al. (see README.md)\n");
#endif

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
	{
		fprintf(stderr, "Problem initialising SDL: %s\n", SDL_GetError());
		return 1;
	}
	SDL_WM_SetCaption("Strange Adventures In Infinite Space", "Strange Adventures In Infinite Space");

	// Enable UNICODE so we can emulate getch() in text input
	SDL_EnableUNICODE(1);

	// init SDL mixer
	if (Mix_OpenAudio(22050, AUDIO_S16, 2, 1024) < 0)
	{
		fprintf(stderr, "Problem initialising Audio: %s\n", SDL_GetError());
		return 1;
	}
	Mix_AllocateChannels(16);
	sound_init();

	// Must find the native resolution *before* setting the video mode
	const SDL_VideoInfo* video_info = SDL_GetVideoInfo();
	g_native_resolution.w = video_info->current_w;
	g_native_resolution.h = video_info->current_h;

	g_virtual_resolution.x = 0;
	g_virtual_resolution.y = 0;
	g_virtual_resolution.w = 640;
	g_virtual_resolution.h = 480;

	g_scaled_video = getScaledVideoManual(false, g_virtual_resolution, g_virtual_resolution, false);
	//g_scaled_video = getScaledVideoManual(false, g_virtual_resolution, &g_native_resolution, true);
	sdlsurf = g_scaled_video->fb();
	//sdlsurf = SDL_SetVideoMode(640, 480, 8, SDL_FULLSCREEN | SDL_HWPALETTE);

	my_main();

	return 0;
}
