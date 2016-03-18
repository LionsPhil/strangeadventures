/* ScaledVideo SDL surfacing scaling routines.
 * Copyright 2009-2016 Philip Boulain. Licensed under the terms of the GNU GPL. */
#ifndef SCALEDVIDEO_HPP_
#define SCALEDVIDEO_HPP_
#include <vector>
#include <SDL.h>

/** A class which provides a virtual framebuffer as an SDL_Surface which is
  * automatically scaled up to a supported native resolution. SDL's video
  * subsystem must have been initialised. Does not currently provide reverse
  * mapping e.g. for mouse position, non-square scaling (e.g. 320x200), or
  * downscaling. Does NOT do format conversion; use the SDL colour-mapping
  * functions et. al. when pixel-banging the virtual framebuffer.
  * Get ScaledVideos using the factory functions below, as the implementation
  * will be tuned to the kind of scaling operation needed. The functions 
  * return NULL if the resolution is bad. Note that they DO affect global
  * state, as they'll change the current video mode. */
class ScaledVideo {
protected:
	SDL_Surface* virtualfb;
	SDL_Rect virtres;
	SDL_Rect trueres;
	std::vector<SDL_Rect> dirtyrects;
public:
	/** Thrown by constructor if specified resolution cannot be used, or if
	  * automatic detection cannot find anything suitable. */
	class BadResolution {};

	/** This is only of use to subclasses. Use the factory functions.
	  * If you try to use this plain, you won't get any video output.
	  * opengl: initialise OpenGL context and get format for textures
	  * force32: force 32-bit graphics depth
	  * Otherwise virtual framebuffer will match screen format. */
	ScaledVideo(SDL_Rect& virtres, SDL_Rect& trueres, bool opengl,
		bool force32, bool fullscreen);
	virtual ~ScaledVideo();

	/** Get access to the virtual framebuffer. Lock it, clone it, poke it,
	  * but don't free it. */
	SDL_Surface* fb();
	/** Register a (virtual) region as changed and in need of a repaint.
	  * Superclass version queues these in dirtyrects. If using it, you
	  * must clear the vector in update()! */
	virtual void dirtyRect(Sint16 x, Sint16 y, Uint16 w, Uint16 h);
	/// Don't override this one; override the separate values version.
	void dirtyRect(const SDL_Rect& rect);
	/** Redraw everything dirty to the real screen. */
	virtual void update();
};

/** Manually define a resolution. If not fullscreen, should always succeed.
  * 'hardware' option selects use of OpenGL scaler, if needed.
  * Will assert() if you try to force downscaling. */
ScaledVideo* getScaledVideoManual(bool hardware, SDL_Rect virtres,
	 SDL_Rect trueres, bool fullscreen);

#endif

