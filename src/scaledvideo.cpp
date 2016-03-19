/* ScaledVideo SDL surfacing scaling routines.
 * Copyright 2009-2016 Philip Boulain. Licensed under the terms of the GNU GPL. */

#include <cassert>
#include <cmath>
#include <cstring>
#include <map>
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
	return ((source.w * std::floor(*scale + 0.5)) == target.w)
	    || ((source.h * std::floor(*scale + 0.5)) == target.h);
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

	if(rect.w < 0) {
		out.w = 0;
	} else if(rect.x + rect.w > m_virtual_resolution.w) {
		out.w = m_virtual_resolution.w - out.x;
	} else {
		out.w = (rect.x + rect.w) - out.x;
	}

	if(rect.h < 0) {
		out.h = 0;
	} else if(rect.y + rect.h > m_virtual_resolution.h) {
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
			m_scale = (unsigned int) scaletmp;
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

#if 0
// Arbitrary software scaler
class ScaledVideoArbitrary : public ScaledVideo {
protected:
	// In this case, we use not only the x and y offset, but also the w
	// and h to record the effective width and height of our true area.
	SDL_Rect offset;

	void mapVirtToTrue(Sint16 virtx, Sint16 virty,
		Sint16* truex, Sint16* truey) {

		*truex = ((virtx * offset.w) / virtres.w) + offset.x;
		*truey = ((virty * offset.h) / virtres.h) + offset.y;
	}

	// This gets called A LOT. Avoid floating point.
	void mapTrueToVirt(Sint16 truex, Sint16 truey,
		Sint16* virtx, Sint16* virty) {

		*virtx = ((truex - offset.x) * virtres.w) / offset.w;
		*virty = ((truey - offset.y) * virtres.h) / offset.h;
	}

public:
	// Force32 should be false for Arbitrary scaler itself
	ScaledVideoArbitrary(SDL_Rect virtres, SDL_Rect trueres,
		bool fullscreen, bool force32 = false)
		: ScaledVideo(virtres, trueres, false, force32, fullscreen) {

		// Calculate target resolution at constant aspect ratio
		// Time for some integer maths! Rearrange these equalities:
		// vw/vh = tw/th  ->  tw = (th*vw)/vh
		// vh/vw = th/tw  ->  th = (tw*vh)/vw
		Sint16 tw_full_height = (trueres.h * virtres.w) / virtres.h;
		Sint16 th_full_width  = (trueres.w * virtres.h) / virtres.w;
		if(tw_full_height > trueres.w) {
			assert(th_full_width <= trueres.h); // else maths fail
			// Using the full height makes us too wide.
			// So use the full width.
			offset.w = trueres.w;
			offset.h = th_full_width;
		} else { // Use the full height.
			offset.w = tw_full_height;
			offset.h = trueres.h;
		}
		// And calculate the actual offset part on top of this
		offset.x = (trueres.w - offset.w) / 2;
		offset.y = (trueres.h - offset.h) / 2;
		assert(offset.x == 0 || offset.y == 0);

		trace("\tusing arbitrary scaler to %dx%d", offset.w, offset.h);
	}

	virtual void dirtyRect(Sint16 x, Sint16 y, Uint16 w, Uint16 h) {
		// Work out the true bounding rectangle
		Sint16 truex1, truex2, truey1, truey2;
		mapVirtToTrue(x,   y,   &truex1, &truey1);
		mapVirtToTrue(x+w, y+h, &truex2, &truey2);

		// For each display pixel in the area, source a virtual pixel
		Sint16 virtx, virty;
		SDL_Surface* screen = SDL_GetVideoSurface();
		SDL_LockSurface(virtualfb);
		SDL_LockSurface(screen);
		Uint8 bypp = virtualfb->format->BytesPerPixel;
		assert(bypp == screen->format->BytesPerPixel);
		char* dstline = (char*) screen->pixels + (truey1*screen->pitch);
		for(Sint16 ty = truey1; ty < truey2; ++ty) {
			char* dstpix = dstline + (truex1 * bypp);
			for(Sint16 tx = truex1; tx < truex2; ++tx) {
				mapTrueToVirt(tx, ty, &virtx, &virty);
				char* srcpix = (char*) virtualfb->pixels
					+ (virty * virtualfb->pitch)
					+ (virtx * bypp);
				memcpy(dstpix, srcpix, bypp);

				dstpix += bypp;
			}
			dstline += screen->pitch;
		}
		SDL_UnlockSurface(screen);
		SDL_UnlockSurface(virtualfb);

		// Record the effective area changed
		ScaledVideo::dirtyRect(truex1, truey1,
			truex2 - truex1, truey2 - truey1);
	}
};

/* The arbitrary scaler is a "fast" nearest-pixel map, and we want crisp pixels
 * rather than bilinear filtering, but this is not visually optimal. We really
 * want something smarter that antialises misaligned pixel boundaries, but
 * still scales them up as squares, not points defining a complex gradient
 * fill. This would seem to require tracking the exact edge positions of each
 * virtual pixel to get the correct blend ratios, and the worst case is a true
 * pixel which contains a virtual corner (four-way blend). It's also full of
 * floating point, so will be slower. */
/* FIXME At low scale factors, like 320x240 -> 400x300, the blends between
 * alternating pixels make things worse. I have a suspicion there's an off-by-
 * one in here bleeding colours across. */
class ScaledVideoArbitraryHQ : public ScaledVideoArbitrary {
	/* Blending two pixels is actually a pretty involved task once you've
	 * spent all day picking apart the PixelFormat properly and doing
	 * gamma correction to allow for nonlinear colours (sigh). This is
	 * still not quite strictly right for sRGB, as that uses gamma 1.0 for
	 * very low colour values. */
	class BlendCache {
		struct Entry {
			Uint32 one; Uint32 two; float frac;
			Entry(Uint32 one, Uint32 two, float frac) :
				one(one), two(two), frac(frac) {}
			bool operator<(const Entry& other) const {
				return (one == other.one) ?
					(two == other.two) ?
						(frac < other.frac) :
						(two < other.two) :
					(one < other.one);
			}
		};

		SDL_PixelFormat* fmt;
		float gamma, invgamma;
		std::map<Entry, Uint32> cache;
		// Avoid unbounded cache growth. Inelegant.
		size_t max_cache;

		Uint32 doBlend(Uint32 one, Uint32 two, float frac) {
			Uint32 red1, gre1, blu1;
			Uint32 red2, gre2, blu2;
			float  red3, gre3, blu3;
			float  red4, gre4, blu4;
			float  redM, greM, bluM;
			float inv = 1.0 - frac;
		// Pick it all apart (weep)
		red1 =one & fmt->Rmask; red1 >>=fmt->Rshift; red1 <<=fmt->Rloss;
		gre1 =one & fmt->Gmask; gre1 >>=fmt->Gshift; gre1 <<=fmt->Gloss;
		blu1 =one & fmt->Bmask; blu1 >>=fmt->Bshift; blu1 <<=fmt->Bloss;
		red2 =two & fmt->Rmask; red2 >>=fmt->Rshift; red2 <<=fmt->Rloss;
		gre2 =two & fmt->Gmask; gre2 >>=fmt->Gshift; gre2 <<=fmt->Gloss;
		blu2 =two & fmt->Bmask; blu2 >>=fmt->Bshift; blu2 <<=fmt->Bloss;		
			// Convert to linear colourspace
			red3 = red1 / 255.0; red4 = red2 / 255.0;
			gre3 = gre1 / 255.0; gre4 = gre2 / 255.0;
			blu3 = blu1 / 255.0; blu4 = blu2 / 255.0;
			red3 = powf(red3, gamma); red4 = powf(red4, gamma);
			gre3 = powf(gre3, gamma); gre4 = powf(gre4, gamma);
			blu3 = powf(blu3, gamma); blu4 = powf(blu4, gamma);
		
			// Blend in linear colourspace
			redM = (red3 * inv) + (red4 * frac);
			greM = (gre3 * inv) + (gre4 * frac);
			bluM = (blu3 * inv) + (blu4 * frac);

			// Convert back to expotential
			red1 = (Uint32)(powf(redM, invgamma) * 255);
			gre1 = (Uint32)(powf(greM, invgamma) * 255);
			blu1 = (Uint32)(powf(bluM, invgamma) * 255);

			// Repack and return
			return SDL_MapRGB(fmt, red1, gre1, blu1);
		}
#if 0 /* This is the old, gamma-ignorant, blender. */
		redM = (Uint8)((((float)red1) * inv) + (((float)red2) * frac));
		greM = (Uint8)((((float)gre1) * inv) + (((float)gre2) * frac));
		bluM = (Uint8)((((float)blu1) * inv) + (((float)blu2) * frac));
#endif

	public:
		BlendCache(SDL_PixelFormat* fmt, float gamma = 2.2) : fmt(fmt),
			gamma(gamma), invgamma(1.0 / gamma), max_cache(65536) {}

		~BlendCache()
			{ trace("Blend cache reached size %d", cache.size()); }

		Uint32 blend(Uint32 one, Uint32 two, float frac) {
			// Don't waste cache entries on non-blends
			if(one == two) { return one; }
			if(frac == 0.0) { return one; }
			if(frac == 1.0) { return two; }

			std::map<Entry, Uint32>::iterator e
				= cache.find(Entry(one, two, frac));
			if(e == cache.end()) { // Cache miss
				Uint32 result = doBlend(one, two, frac);
				if(cache.size() < max_cache) {
					cache[Entry(one, two, frac)] = result;
				} else {
#ifndef NDEBUG
					if(max_cache) { max_cache = 0;
					trace("Blend cache has hit max size!");
					}
#endif
				}
				return result;
			} else {
				return e->second;
			}
		}
	};
	BlendCache blendcache;

	/* For true pixel column/row blend[x], what proportion is the
	 * linear-nearest-fit pixel (remainder will be next along)?
	 * Base mapTrueToVirt will, on a "blend" true pixel, always give this
	 * left/top pixel due to integer truncation; e.g. for middle pixel of
	 * true fb [012], with virtual fb [01], it gives (1 * 2) / 3 = 0.
	 * The range of these should also therefore be 0 <= blend < 1. */
	std::vector<float> blendhoriz;
	std::vector<float> blendvert;
	
	/* Populates a (expected 0-init'd!) blend set based on a given pair of
	 * dimensions (e.g. the virtual width and the true width).
	 * 'no' = 'no offset', i.e. the active area. */
	void calcBlends(const Uint16 virtdim, const Uint16 truenodim,
		const Sint16 offset, std::vector<float>& blends) {
		
		assert(offset >= 0);
		float truelow;
		float truehigh = 0;
		for(Uint16 virtv = 0; virtv < virtdim; ++virtv) {
			truelow = truehigh;
			truehigh =
				(((float) virtv + 1) * truenodim) / virtdim;
			/* In integer terms, we're now dealing with
			 * truelow <= x < truehigh
			 * (truehigh will == truedim in the final iteration).
			 * We only blend in a 'right/below' direction, and want
			 * to do this for a single true row/column, to keep
			 * crisp squares. Everything else is left as zero.
			 * TRUE | 4 | 5 | 6 |  <-- Say that truelow = 5.7 for
			 * VIRT    2   |   3                    a virtv of 3.
			 * mapTrueToVirt(5) will give 2, and we want true pixel
			 * 5 to be a 0.7 blend of that, and 0.3 of virtual 3.
			 * The blend factor is "how far through the true pixel
			 * was the left edge of the virtual one?", i.e. the
			 * fractional part. */
			assert(truelow < truenodim);
			blends[(int)truelow + offset] = truelow - (int)truelow;
		}
	}

	inline Uint32 blendPix(Uint32 one, Uint32 two, float frac) {
		return blendcache.blend(one, two, frac);
	}
	
public:
	ScaledVideoArbitraryHQ(SDL_Rect virtres, SDL_Rect trueres,
		bool fullscreen) // Note init of Arbitrary with force32 on
		: ScaledVideoArbitrary(virtres, trueres, fullscreen, true),
		blendcache(virtualfb->format),
		blendhoriz(trueres.w, 0.0), blendvert(trueres.h, 0.0) {

		/* Pixel blending is inner-loop code, and this constraint avoids
		 * the need to faff with awkward pixel alignments (hello,
		 * 24-bit). It's already nightmarish enough as it is.
		 * The force32 ScaledVideo constructor argument *should*
		 * make SDL give us a compatable shadow surface if needed. */
		if(virtualfb->format->BytesPerPixel != 4) {
			warn("High-quality scaler requires 32-bit graphics; "
				"currently have %u (%u-bit storage)",
				virtualfb->format->BitsPerPixel,
				virtualfb->format->BytesPerPixel * 8);
			throw ScaledVideo::BadResolution();
		}
		// Base base class should ensure that virtualfb is screen fmt.
		assert(SDL_GetVideoSurface()->format->BytesPerPixel == 4);

		// Base class constructor sorts out offset for us
		// Work out horizontal and vertical blending
		// Note that this takes the offset w/h (i.e. active true res)
		calcBlends(virtres.w, offset.w, offset.x, blendhoriz);
		calcBlends(virtres.h, offset.h, offset.y, blendvert);

		trace("\t(high quality)");
	}

	// TODO Refactor: this shares all but the loop body with base
	virtual void dirtyRect(Sint16 x, Sint16 y, Uint16 w, Uint16 h) {
		// Work out the true bounding rectangle
		/* This always maximally overshoots, so should include any
		 * blends: naughty copy-paste from base implementation. */
		Sint16 truex1, truex2, truey1, truey2;
		mapVirtToTrue(x,   y,   &truex1, &truey1);
		mapVirtToTrue(x+w, y+h, &truex2, &truey2);

		/* For each display pixel in the area, source a virtual pixel
		 * ...and blend as appropriate. */
		Sint16 virtx, virty;
		SDL_Surface* screen = SDL_GetVideoSurface();
		SDL_LockSurface(virtualfb);
		SDL_LockSurface(screen);
		Uint8 bypp = 4; // Guaranteed by constructor
		char* dstline = (char*) screen->pixels + (truey1*screen->pitch);
		for(Sint16 ty = truey1; ty < truey2; ++ty) {
			bool doblendv = (blendvert[ty] != 0.0);
			char* dstpix = dstline + (truex1 * bypp);
			for(Sint16 tx = truex1; tx < truex2; ++tx) {
				bool doblendh = (blendhoriz[tx] != 0.0);
				
				mapTrueToVirt(tx, ty, &virtx, &virty);
				char* srcpix = (char*) virtualfb->pixels
					+ (virty * virtualfb->pitch)
					+ (virtx * bypp);
				char* srcpixbelow = 0; // not always needed
				if(doblendv) { srcpixbelow =
					srcpix + virtualfb->pitch; }
				
				if(doblendh) {
					char* srcpixright = srcpix + bypp;
					if(doblendv) {
						char* srcpixcorner =
							srcpixbelow + bypp;
						// Four-way blend, oh Goddess
						*((Uint32*) dstpix) =
						 blendPix( // Both columns
						  blendPix( // Left column
						   *((Uint32*) srcpix),
						   *((Uint32*) srcpixbelow),
						   blendvert[ty]),
						  blendPix( // Right column
						   *((Uint32*) srcpixright),
						   *((Uint32*) srcpixcorner),
						   blendvert[ty]),
						  blendhoriz[tx]);
					} else {
						// Two-way horizontal
						*((Uint32*) dstpix) =
							blendPix(
							*((Uint32*) srcpix),
							*((Uint32*)srcpixright),
							blendhoriz[tx]);
					}
				} else if(doblendv) {
					// Two-way vertical
					*((Uint32*) dstpix) = blendPix(
						*((Uint32*) srcpix),
						*((Uint32*) srcpixbelow),
						blendvert[ty]);
				} else {
					// No blending; how delightedly simple!
					memcpy(dstpix, srcpix, bypp);
				}
				dstpix += bypp;
			}
			dstline += screen->pitch;
		}
		SDL_UnlockSurface(screen);
		SDL_UnlockSurface(virtualfb);

		// Record the effective area changed (same as base again)
		ScaledVideo::dirtyRect(truex1, truey1,
			truex2 - truex1, truey2 - truey1);
	}
};
#endif

// Factory function ///////////////////////////////////////////////////////////

ScaledVideo* get_scaled_video(
	SDL_Surface* virtual_surface,
	int true_w,
	int true_h,
	int true_bpp,
	Uint32 flags,
	bool high_quality) {

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
	 * supported". Let that case fall down to the integer scaler. */
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
		// XXX arb scalers temporarily disabled
		return new ScaledVideoTranslateOnly(
			virtual_surface, screen,
			virtual_resolution, true_resolution);
#if 0
		if(high_quality) {
			return new ScaledVideoArbitraryHQ(
				virtual_surface, screen,
				virtual_resolution, true_resolution);
		} else {
			return new ScaledVideoArbitrary(
				virtual_surface, screen,
				virtual_resolution, true_resolution);
		}
#endif
	}
}
