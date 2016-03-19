/* ScaledVideo SDL surfacing scaling routines.
 * Copyright 2009-2016 Philip Boulain. Licensed under the terms of the GNU GPL. */

#include <cassert>
#include <cmath>
#include <cstring>
#include <new>
#include <sstream>
#include <stdexcept>

#include "scaledvideo.hpp"

/* Returns true if scale factor is integer, and sets the scale factor in
 * the double out parameter. Allows for translations. */
static bool scale_needed(SDL_Rect& source, SDL_Rect& target, double* scale) {
	double scalew = ((double) target.w) / source.w;
	double scaleh = ((double) target.h) / source.h;
	*scale = (scalew > scaleh) ? scaleh : scalew;
	return ((source.w * (unsigned int) std::floor(*scale)) == target.w)
	    || ((source.h * (unsigned int) std::floor(*scale)) == target.h);
}

ScaledVideo::ScaledVideo(
	SDL_Surface* virtual_surface,
	SDL_Surface* true_surface,
	SDL_Rect& virtual_resolution,
	SDL_Rect& true_resolution)
	:
	m_virtual_surface(virtual_surface),
	m_true_surface(true_surface),
	m_virtual_resolution(virtual_resolution),
	m_true_resolution(true_resolution) {

	m_virtual_dirty.x = -1;
	m_virtual_dirty.y = 0;
	m_virtual_dirty.w = 0;
	m_virtual_dirty.h = 0;
}

ScaledVideo::~ScaledVideo() {}

void ScaledVideo::clipPoint(Sint16 raw_x, Sint16 raw_y,
	Sint16* clip_x, Sint16* clip_y) {

	*clip_x = raw_x < 0 ? 0
		: (raw_x >= m_virtual_resolution.w ? m_virtual_resolution.w - 1
		 	: raw_x);
	*clip_y = raw_y < 0 ? 0
		: (raw_y >= m_virtual_resolution.h ? m_virtual_resolution.h - 1
		 	: raw_y);
}

SDL_Rect ScaledVideo::clipRect(const SDL_Rect& rect) {
	SDL_Rect out;

	clipPoint(rect.x, rect.y, &out.x, &out.y);

	// w and h are unsigned, cannot be < 0
	if(rect.x + rect.w > m_virtual_resolution.w) {
		out.w = m_virtual_resolution.w - out.x;
	} else {
		out.w = (rect.x + rect.w) - out.x;
	}

	if(rect.y + rect.h > m_virtual_resolution.h) {
		out.h = m_virtual_resolution.h - out.y;
	} else {
		out.h = (rect.y + rect.h) - out.y;
	}

	return out;
}

void ScaledVideo::dirtyRect(const SDL_Rect& rect) {
	SDL_Rect clipped = clipRect(rect);

	if(m_virtual_dirty.x < 0) {
		// No existing dirty region
		m_virtual_dirty = clipped;
		return;
	}

	Sint32 r_x2 =    rect.x +    rect.w;
	Sint32 r_y2 =    rect.y +    rect.h;
	Sint32 c_x2 = clipped.x + clipped.w;
	Sint32 c_y2 = clipped.y + clipped.h;
	if(clipped.x < m_virtual_dirty.x) { m_virtual_dirty.x = clipped.x; }
	if(clipped.y < m_virtual_dirty.y) { m_virtual_dirty.y = clipped.y; }
	if(c_x2 > r_x2) { m_virtual_dirty.w = c_x2 - m_virtual_dirty.x; }
	if(c_y2 > r_y2) { m_virtual_dirty.h = c_y2 - m_virtual_dirty.y; }
}

void ScaledVideo::update(bool to_screen) {
	// Nothing to do?
	if(m_virtual_dirty.x < 0) { return; }

	// Do the actual scaling
	updateScale();

	// If the target is palettized, copy the palette too
	if(m_true_surface->format->BitsPerPixel == 8) {
		SDL_SetPalette(m_true_surface, SDL_LOGPAL|SDL_PHYSPAL,
			m_virtual_surface->format->palette->colors, 0,
			m_virtual_surface->format->palette->ncolors);
	}

	// Render to the screen, if requested
	if(to_screen) {
		SDL_Rect true_dirty;
		Sint16 x2, y2;

		mapVirtualToTrue(
			m_virtual_dirty.x, m_virtual_dirty.y,
			&true_dirty.x, &true_dirty.y);
		mapVirtualToTrue(
			m_virtual_dirty.x + m_virtual_dirty.w,
			m_virtual_dirty.y + m_virtual_dirty.h,
			&x2, &y2);
		true_dirty.w = x2 - true_dirty.x;
		true_dirty.h = y2 - true_dirty.y;

		SDL_UpdateRects(m_true_surface, 1, &true_dirty);

		// XXX DEBUG
		/* fprintf(stderr, "[%d,%d,%d,%d]-%d,%d->[%d,%d,%d,%d]\n",
			m_virtual_dirty.x, m_virtual_dirty.y,
			m_virtual_dirty.w, m_virtual_dirty.h,
			x2, y2,
			true_dirty.x, true_dirty.y,
			true_dirty.w, true_dirty.h); */
	}

	// Mark that we have no dirt left
	m_virtual_dirty.x = -1;
}

// No scaling, just translate.
class ScaledVideoTranslateOnly : public ScaledVideo {
	SDL_Rect m_offset;
public:
	ScaledVideoTranslateOnly(
		SDL_Surface* virtual_surface,
		SDL_Surface* true_surface,
		SDL_Rect& virtual_resolution,
		SDL_Rect& true_resolution)
		:
		ScaledVideo(
			virtual_surface, true_surface,
			virtual_resolution, true_resolution) {

		m_offset.x = (m_true_resolution.w - m_virtual_resolution.w) / 2;
		m_offset.y = (m_true_resolution.h - m_virtual_resolution.h) / 2;
	}

	virtual std::string describe() {
		std::ostringstream oss;
		oss << "translate by " << m_offset.x << ", " << m_offset.y;
		return oss.str();
	}

	virtual void updateScale() {
		// Just blit it with the offset.
		SDL_Rect destination;
		destination.x = m_virtual_dirty.x + m_offset.x;
		destination.y = m_virtual_dirty.y + m_offset.y;
		SDL_BlitSurface(m_virtual_surface, &m_virtual_dirty,
			m_true_surface, &destination);
	}

	virtual void mapVirtualToTrue(Sint16 virtual_x, Sint16 virtual_y,
		Sint16* true_x, Sint16* true_y) {

		*true_x = virtual_x + m_offset.x;
		*true_y = virtual_y + m_offset.y;
	}

	virtual void mapTrueToVirtual(Sint16 true_x, Sint16 true_y,
		Sint16* virtual_x, Sint16* virtual_y) {

		clipPoint(
			true_x - m_offset.x,
			true_y - m_offset.y,
			virtual_x, virtual_y);
	}
};

// Integer software scaler
class ScaledVideoInteger : public ScaledVideo {
	SDL_Rect m_offset;
	unsigned int m_scale;
public:
	ScaledVideoInteger(
		SDL_Surface* virtual_surface,
		SDL_Surface* true_surface,
		SDL_Rect& virtual_resolution,
		SDL_Rect& true_resolution)
		:
		ScaledVideo(
			virtual_surface, true_surface,
			virtual_resolution, true_resolution) {
			
		double scaletmp;
		if(scale_needed(virtual_resolution,true_resolution,&scaletmp)) {
			m_scale = std::floor(scaletmp);
		} else {
			throw std::logic_error(
				"Integer scaler asked to perform non-int "
				"scaling");
		}

		m_offset.x =
			(m_true_resolution.w-(m_virtual_resolution.w*m_scale))
				/ 2;
		m_offset.y =
			(m_true_resolution.h-(m_virtual_resolution.h*m_scale))
				/ 2;
	}

	virtual std::string describe() {
		std::ostringstream oss;
		oss << "translate by " << m_offset.x << ", " << m_offset.y;
		oss << " and integer scale by " << m_scale << "x";
		return oss.str();
	}

	/* This is not wonderously efficient; it makes no attempt at batch or
	 * parallel processing, so iterates over every single output pixel.
	 * Demands that the pixel formats match (enforced by factory).
	 * TODO Benchmark, see if Arbitrary approach is always faster; if so,
	 * improve or ditch this. */
	virtual void updateScale() {
		Sint16 x = m_virtual_dirty.x;
		Sint16 y = m_virtual_dirty.y;
		Uint16 w = m_virtual_dirty.w;
		Uint16 h = m_virtual_dirty.h;

		// Check within bounds, else this can scramble memory
		assert(x >= 0); assert (y >= 0);
		assert(x + w <= m_virtual_resolution.w);
		assert(y + h <= m_virtual_resolution.h);

		// Copy and upscale data
		SDL_LockSurface(m_virtual_surface);
		SDL_LockSurface(m_true_surface);
		Uint8 bypp = m_virtual_surface->format->BytesPerPixel;
		assert(bypp == m_true_surface->format->BytesPerPixel);
		// (we demanded same format from SDL, remember)
		char* srcline = (char*) m_virtual_surface->pixels
			+ (y * m_virtual_surface->pitch);
		char* dstline = (char*) m_true_surface->pixels
			+ (((y*m_scale)+m_offset.y) * m_true_surface->pitch);
		for(Uint16 lines = h; lines; --lines) {
			for(unsigned int ydup = m_scale; ydup; --ydup) {
				char* srcpix = srcline + (x * bypp);
				char* dstpix = dstline +
					(((x*m_scale)+m_offset.x) * bypp);

				for(Uint16 cols = w; cols; --cols) {
					// TODO memcpy m_scale*bypp at once?
					for(unsigned int xdup = m_scale; xdup;
						--xdup) {

						memcpy(dstpix, srcpix, bypp);
						dstpix += bypp;
					}
					srcpix += bypp;
				}
				dstline += m_true_surface->pitch;
			}
			srcline += m_virtual_surface->pitch;
		}
		SDL_UnlockSurface(m_true_surface);
		SDL_UnlockSurface(m_virtual_surface);
	}

	virtual void mapVirtualToTrue(Sint16 virtual_x, Sint16 virtual_y,
		Sint16* true_x, Sint16* true_y) {

		*true_x = (virtual_x * m_scale) + m_offset.x;
		*true_y = (virtual_y * m_scale) + m_offset.y;
	}

	virtual void mapTrueToVirtual(Sint16 true_x, Sint16 true_y,
		Sint16* virtual_x, Sint16* virtual_y) {

		clipPoint(
			(true_x / m_scale) - m_offset.x,
			(true_y / m_scale) - m_offset.y,
			virtual_x, virtual_y);
	}
};

// Arbitrary software scaler, copying
class ScaledVideoArbitrary : public ScaledVideo {
protected:
	// In this case, we use not only the x and y offset, but also the w
	// and h to record the effective width and height of our true area.
	SDL_Rect m_offset;

	/* This gets called A LOT. Avoid floating point.
	 * (This is also why it has a protected, nonvirtual version.) */
	void mapTrueToVirtualInternal(Sint16 true_x, Sint16 true_y,
		Sint16* virtual_x, Sint16* virtual_y) {

		*virtual_x =
			((true_x - m_offset.x) * m_virtual_resolution.w)
				/ m_offset.w;
		*virtual_y =
			((true_y - m_offset.y) * m_virtual_resolution.h)
				/ m_offset.h;
	}

public:
	ScaledVideoArbitrary(
		SDL_Surface* virtual_surface,
		SDL_Surface* true_surface,
		SDL_Rect& virtual_resolution,
		SDL_Rect& true_resolution)
		:
		ScaledVideo(
			virtual_surface, true_surface,
			virtual_resolution, true_resolution) {

		// Calculate target resolution at constant aspect ratio
		// Time for some integer maths! Rearrange these equalities:
		// vw/vh = tw/th  ->  tw = (th*vw)/vh
		// vh/vw = th/tw  ->  th = (tw*vh)/vw
		Sint16 tw_full_height =
			(m_true_resolution.h * m_virtual_resolution.w)
				/ m_virtual_resolution.h;
		Sint16 th_full_width  =
			(m_true_resolution.w * m_virtual_resolution.h)
				/ m_virtual_resolution.w;
		if(tw_full_height > m_true_resolution.w) {
			// Check, else maths fail
			assert(th_full_width <= m_true_resolution.h);
			// Using the full height makes us too wide.
			// So use the full width.
			m_offset.w = m_true_resolution.w;
			m_offset.h = th_full_width;
		} else { // Use the full height.
			m_offset.w = tw_full_height;
			m_offset.h = m_true_resolution.h;
		}
		// And calculate the actual offset part on top of this
		m_offset.x = (m_true_resolution.w - m_offset.w) / 2;
		m_offset.y = (m_true_resolution.h - m_offset.h) / 2;
		assert(m_offset.x == 0 || m_offset.y == 0);
	}

	virtual std::string describe() {
		std::ostringstream oss;
		oss << "translate by " << m_offset.x << ", " << m_offset.y;
		oss << " and arbitrary-scale to "
			<< m_offset.w << "x" << m_offset.h;
		return oss.str();
	}

	virtual void updateScale() {
		Sint16 x = m_virtual_dirty.x;
		Sint16 y = m_virtual_dirty.y;
		Uint16 w = m_virtual_dirty.w;
		Uint16 h = m_virtual_dirty.h;

		// Work out the true bounding rectangle
		Sint16 truex1, truex2, truey1, truey2;
		mapVirtualToTrue(x,   y,   &truex1, &truey1);
		mapVirtualToTrue(x+w, y+h, &truex2, &truey2);

		// For each display pixel in the area, source a virtual pixel
		Sint16 virtx, virty;
		SDL_LockSurface(m_virtual_surface);
		SDL_LockSurface(m_true_surface);
		Uint8 bypp = m_virtual_surface->format->BytesPerPixel;
		assert(bypp == m_true_surface->format->BytesPerPixel);
		char* dstline = (char*) m_true_surface->pixels
			+ (truey1*m_true_surface->pitch);
		for(Sint16 ty = truey1; ty < truey2; ++ty) {
			char* dstpix = dstline + (truex1 * bypp);
			for(Sint16 tx = truex1; tx < truex2; ++tx) {
				mapTrueToVirtualInternal(tx,ty, &virtx,&virty);
				char* srcpix = (char*) m_virtual_surface->pixels
					+ (virty * m_virtual_surface->pitch)
					+ (virtx * bypp);
				memcpy(dstpix, srcpix, bypp);

				dstpix += bypp;
			}
			dstline += m_true_surface->pitch;
		}
		SDL_UnlockSurface(m_true_surface);
		SDL_UnlockSurface(m_virtual_surface);
	}

	virtual void mapVirtualToTrue(Sint16 virtual_x, Sint16 virtual_y,
		Sint16* true_x, Sint16* true_y) {

		*true_x = ((virtual_x * m_offset.w) / m_virtual_resolution.w)
			+ m_offset.x;
		*true_y = ((virtual_y * m_offset.h) / m_virtual_resolution.h)
			+ m_offset.y;
	}

	virtual void mapTrueToVirtual(Sint16 true_x, Sint16 true_y,
		Sint16* virtual_x, Sint16* virtual_y) {

		Sint16 unclipped_x, unclipped_y;
		mapTrueToVirtualInternal(true_x, true_y,
			&unclipped_x, &unclipped_y);
		clipPoint(unclipped_x, unclipped_y,
			virtual_x, virtual_y);
	}
};

/* Arbitrary software scaler, converting.
 * This is the almost-ultimate fallback: handles any scaling, any 32-bit RGB out
 * format. */
class ScaledVideoArbitraryConvertingPaletted : public ScaledVideoArbitrary {
public:
	ScaledVideoArbitraryConvertingPaletted(
		SDL_Surface* virtual_surface,
		SDL_Surface* true_surface,
		SDL_Rect& virtual_resolution,
		SDL_Rect& true_resolution)
		:
		ScaledVideoArbitrary(
			virtual_surface, true_surface,
			virtual_resolution, true_resolution) {

		assert(virtual_surface->format->BitsPerPixel == 8);
		assert(   true_surface->format->BitsPerPixel == 32);
	}

	virtual std::string describe() {
		std::ostringstream oss;
		oss << ScaledVideoArbitrary::describe();
		oss << " (converting from 8bpp)";
		return oss.str();
	}

	virtual void updateScale() {
		Sint16 x = m_virtual_dirty.x;
		Sint16 y = m_virtual_dirty.y;
		Uint16 w = m_virtual_dirty.w;
		Uint16 h = m_virtual_dirty.h;

		// Work out the true bounding rectangle
		Sint16 truex1, truex2, truey1, truey2;
		mapVirtualToTrue(x,   y,   &truex1, &truey1);
		mapVirtualToTrue(x+w, y+h, &truex2, &truey2);

		// Transform the palette into target format
		SDL_Palette* palette = m_virtual_surface->format->palette;
		assert(palette->ncolors <= 256);
		Uint32 colors[256];
		for(int c = 0; c < palette->ncolors; ++c) {
			colors[c] = SDL_MapRGB(m_true_surface->format,
				palette->colors[c].r,
				palette->colors[c].g,
				palette->colors[c].b);
		}

		// For each display pixel in the area, source a virtual pixel
		Sint16 virtx, virty;
		SDL_LockSurface(m_virtual_surface);
		SDL_LockSurface(m_true_surface);
		Uint8 virtual_bypp = m_virtual_surface->format->BytesPerPixel;
		Uint8 true_bypp    =    m_true_surface->format->BytesPerPixel;
		char* dstline = (char*) m_true_surface->pixels
			+ (truey1*m_true_surface->pitch);
		for(Sint16 ty = truey1; ty < truey2; ++ty) {
			char* dstpix = dstline + (truex1 * true_bypp);
			for(Sint16 tx = truex1; tx < truex2; ++tx) {
				mapTrueToVirtualInternal(tx,ty, &virtx,&virty);
				unsigned char* srcpix = (unsigned char*)
					m_virtual_surface->pixels
					+ (virty * m_virtual_surface->pitch)
					+ (virtx * virtual_bypp);

				*((Uint32*) dstpix) = colors[*srcpix];

				dstpix += true_bypp;
			}
			dstline += m_true_surface->pitch;
		}
		SDL_UnlockSurface(m_true_surface);
		SDL_UnlockSurface(m_virtual_surface);
	}
};

/* SAIS doesn't need to handle non-8bpp input, but the other half of the
 * ultimate fallback would be a ScaledVideoArbitraryConvertingRGB that
 * converts pixel data a la:
 * red1 =one & fmt->Rmask; red1 >>=fmt->Rshift; red1 <<=fmt->Rloss;
 * gre1 =one & fmt->Gmask; gre1 >>=fmt->Gshift; gre1 <<=fmt->Gloss;
 * blu1 =one & fmt->Bmask; blu1 >>=fmt->Bshift; blu1 <<=fmt->Bloss;
 * and does the SDL_MapRGB. Not gonna be the fastest thing...
 * Bonus points: template it to handle 16-bit output too.
 * None of the scalers will handle RGB -> paletted output.
 */

// Factory function ///////////////////////////////////////////////////////////

ScaledVideo* get_scaled_video(
	SDL_Surface* virtual_surface,
	int true_w,
	int true_h,
	int true_bpp,
	Uint32 flags) {

	// Sanity-check the desired resolution
	if((true_w < virtual_surface->w) || (true_h < virtual_surface->h)) {
		throw std::runtime_error("can't downscale");
	}

	// Switch to the requested video mode (or bail)
	SDL_Surface* screen = SDL_SetVideoMode(
		true_w, true_h, true_bpp, flags);
	if(!screen) {
		throw std::runtime_error(SDL_GetError());
	}

	/* Blank the new true surface, since there may be borders we will
	 * never draw to again. */
	SDL_FillRect(screen, 0, SDL_MapRGB(screen->format, 0, 0, 0));

	/* Find the pixel format SDL has chosen for us and compare it to the
	 * virtual surface. If they're the same, just copying memory is an
	 * option, so long as we update the palette. */
	SDL_PixelFormat* true_format    = screen->format;
	SDL_PixelFormat* virtual_format = virtual_surface->format;
	int  actual_bpp =   true_format->BitsPerPixel; // might be != true_bpp
	int virtual_bpp = virtual_format->BitsPerPixel;
	bool can_just_copy =
		(actual_bpp == 8 && virtual_bpp == 8) || (
			(actual_bpp         == virtual_bpp          ) &&
			(true_format->Rmask == virtual_format->Rmask) &&
			(true_format->Gmask == virtual_format->Gmask) &&
			(true_format->Bmask == virtual_format->Bmask) &&
			(true_format->Amask == virtual_format->Amask));

	// XXX DEBUG
	if(can_just_copy) {
		fprintf(stderr, "Scaling by copying is possible\n");
	} else {
		fprintf(stderr, "Can't scale by copying: "
			"bpp(%d,%d) rm(%x,%x)\n",
			actual_bpp, virtual_bpp,
			true_format->Rmask, virtual_format->Rmask);
	}
	
	// Get the resolutions packed into SDL_Rects
	SDL_Rect virtual_resolution;
	virtual_resolution.x = virtual_resolution.y = 0;
	virtual_resolution.w = virtual_surface->w;
	virtual_resolution.h = virtual_surface->h;
	SDL_Rect true_resolution;
	true_resolution.x = true_resolution.y = 0;
	true_resolution.w = true_w;
	true_resolution.h = true_h;

	/* Translate is simple and should be fast, but SDL_BlitSurface seems
	 * to choke on 8bpp-to-8bpp blits(!) with "Blit combination not
	 * supported". Let that case fall down to the other scalers. */
	if((actual_bpp != 8) && (
		(virtual_resolution.w == true_resolution.w) ||
		(virtual_resolution.h == true_resolution.h))) {

		return new ScaledVideoTranslateOnly(
			virtual_surface, screen,
			virtual_resolution, true_resolution);
	}

	double upscale;
	bool integer = scale_needed(
		virtual_resolution, true_resolution, &upscale);

	if(integer && can_just_copy) {
		return new ScaledVideoInteger(
			virtual_surface, screen,
			virtual_resolution, true_resolution);
	} else {
		if(can_just_copy) {
			return new ScaledVideoArbitrary(
				virtual_surface, screen,
				virtual_resolution, true_resolution);
		} else if(virtual_bpp == 8 && actual_bpp == 32) {
			return new ScaledVideoArbitraryConvertingPaletted(
				virtual_surface, screen,
				virtual_resolution, true_resolution);
		} else if(actual_bpp == 32) {
			// SAIS doesn't need this
			throw std::logic_error(
				"how did we get a non-8bpp input?");
			/* return new ScaledVideoArbitraryConvertingRGB(
				virtual_surface, screen,
				virtual_resolution, true_resolution); */
		} else {
			// We just don't support 16 or 24 bit output
			throw std::runtime_error(
				"no supported scaler for output");
		}
	}
}
