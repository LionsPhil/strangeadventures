/* ScaledVideo SDL surfacing scaling routines.
 * Copyright 2009-2016 Philip Boulain. Licensed under the terms of the GNU GPL. */

#ifndef SCALEDVIDEO_HPP_
#define SCALEDVIDEO_HPP_

#include <string>

#include <SDL.h>

/** Abstract class for scalers between virtual and true surfaces. Use the
  * get_scaled_video() factory function to get one. SDL's video subsystem
  * must have been initialized. Does not provide non-square scaling (e.g.
  * 320x200) or downscaling. The factory function will select an implementation
  * which is tuned to the scaling function needed. */
class ScaledVideo {
protected:
	SDL_Surface* m_virtual_surface;
	SDL_Surface* m_true_surface;
	SDL_Rect m_virtual_resolution;
	SDL_Rect m_true_resolution;
	SDL_Rect m_virtual_dirty;

	// Utility to clip virtual point and rectangle
	void clipPoint(Sint16 raw_x, Sint16 raw_y,
		Sint16* clip_x, Sint16* clip_y);
	SDL_Rect clipRect(const SDL_Rect& rect);
	// Implement this to do the scaling
	virtual void updateScale() = 0;
public:
	/** This is only of use to subclasses. Use the factory function.
	  * If you try to use this plain, you won't get any video output. */
	ScaledVideo(
		SDL_Surface* virtual_surface,
		SDL_Surface* true_surface,
		SDL_Rect& virtual_resolution,
		SDL_Rect& true_resolution);
	virtual ~ScaledVideo();

	/** Return a caller-owned string describing the scaler in use. */
	virtual std::string describe() = 0;

	/** Register a (virtual) region as changed and in need of a repaint.
	  * Automatically clips offscreen rectangles. */
	void dirtyRect(const SDL_Rect& rect);

	/** Redraw everything dirty to the true surface.
	  * Pass true to also update that surface to the screen.
	  * Only meaningful if it is the SDL video surface. */
	virtual void update(bool to_screen = false);

	/** Map a rectangle from virtual to true co-ordinates.
	  * External uses for this are somewhat obscure. */
	virtual void mapVirtualToTrue(Sint16 virtual_x, Sint16 virtual_y,
		Sint16* true_x, Sint16* true_y) = 0;

	/** Map a point from true to virtual co-ordinates.
	  * This is the one to use for mapping e.g. mouse clicks.
	  * Clips to the virtual surface's co-ordinates. */
	virtual void mapTrueToVirtual(Sint16 true_x, Sint16 true_y,
		Sint16* virtual_x, Sint16* virtual_y) = 0;
};

/** Set the SDL video mode to the given true resolution (and flags), and get
  * a ScaledVideo instance that maps from the specified virtual surface to it.
  * Can throw a runtime_error if SDL won't allow switching video mode.
  * high_quality enables a fancier arbitrary-factor scaler.
  * Slower, worse at low upscale factors, better at high.
  * Caller owns the returned object. */
ScaledVideo* get_scaled_video(
	SDL_Surface* virtual_surface,
	int true_w,
	int true_h,
	int true_bpp = 0,
	Uint32 flags = SDL_SWSURFACE | SDL_ANYFORMAT,
	bool high_quality = false);

#endif
