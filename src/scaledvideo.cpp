/* ScaledVideo SDL surfacing scaling routines.
 * Copyright 2009-2016 Philip Boulain. Licensed under the terms of the GNU GPL. */
#include <new>
#include <map>
#include <cassert>
#include <cstring>
#include <cmath>
#ifdef WITH_OPENGL
// TODO Include OpenGL header
#endif
#include "scaledvideo.hpp"

// Temporary
void warn(const char* fmt, ...) {
	va_list marker;
	va_start(marker, fmt);
	vfprintf(stderr, fmt, marker);
	fputs("\n", stderr);
	va_end(marker);
}
#define trace warn

//#define SCALE_ARB_BLEND // See below; should be runtime.

/* Future expansion: accept a optional SDL_PixelFormat* for the virtual
 * surface; if used, will need to select TranslateOnly over NoOp. */

// Returns true if scale factor is integer
static bool scale_needed(SDL_Rect& source, SDL_Rect& target, double* scale) {
	double scalew = ((double) target.w) / source.w;
	double scaleh = ((double) target.h) / source.h;
	*scale = (scalew > scaleh) ? scaleh : scalew;
	return ((source.w * ((int) *scale)) == target.w)
	    && ((source.h * ((int) *scale)) == target.h);
}

ScaledVideo::ScaledVideo(SDL_Rect& virtres, SDL_Rect& trueres, bool opengl,
	bool force32, bool fullscreen) : virtres(virtres), trueres(trueres) {

	// Set up the true surface
	SDL_Surface* screen;
	Uint32 flags = SDL_SWSURFACE|SDL_ANYFORMAT|SDL_HWPALETTE;
	if(opengl) { flags |= SDL_OPENGL|SDL_DOUBLEBUF; }
	if(fullscreen) { flags |= SDL_FULLSCREEN; }
	screen = SDL_SetVideoMode(trueres.w, trueres.h, force32?32:0, flags);
	if(!screen) {
		warn("Can't set video mode: %s", SDL_GetError());
		throw ScaledVideo::BadResolution();
	}
	SDL_FillRect(screen, 0, SDL_MapRGB(screen->format, 0, 0, 0));

	// Set up our virtual surface
	int bpp;
	Uint32 rmask, gmask, bmask, amask;
/*	if(opengl) {
		// We must use an OpenGL-compliant byte ordering.
		bpp = 32;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		rmask = 0xff000000;
		gmask = 0x00ff0000;
		bmask = 0x0000ff00;
		amask = 0x000000ff;
#else
		rmask = 0x000000ff;
		gmask = 0x0000ff00;
		bmask = 0x00ff0000;
		amask = 0xff000000;
#endif
	} else {
		// Match the screen to help SDL do faster blits.
		SDL_PixelFormat* nativefmt = SDL_GetVideoSurface()->format;
		bpp = nativefmt->BitsPerPixel;
		rmask = nativefmt->Rmask;
		gmask = nativefmt->Gmask;
		bmask = nativefmt->Bmask;
		amask = nativefmt->Amask;
	} */
	// FIXME let caller specify; SAIS needs this
	bpp = 8;
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x000000ff;
	// Generally speaking, SWSURFACE seems more appropriate for this.
	virtualfb = SDL_CreateRGBSurface(SDL_SWSURFACE, virtres.w, virtres.h,
		bpp, rmask, gmask, bmask, amask);
	if(!virtualfb) {
		warn("Can't set up virtual surface: %s", SDL_GetError());
		throw std::bad_alloc();
	}
/*	if(!opengl) {
		// Match the screen /properly/.
		SDL_Surface* better = SDL_DisplayFormat(virtualfb);
		if(better) {
			SDL_FreeSurface(virtualfb);
			virtualfb = better;
		} // else never mind
	} */
}

ScaledVideo::~ScaledVideo() { if(virtualfb) { SDL_FreeSurface(virtualfb); } }

SDL_Surface* ScaledVideo::fb() { return virtualfb; }

/* Note that the base implemenation clips against the TRUE resolution.
 * This is because it's useful to store dirty rectangles transformed. */
void ScaledVideo::dirtyRect(Sint16 x, Sint16 y, Uint16 uw, Uint16 uh) {
	Sint32 w = uw;
	Sint32 h = uh;
	Uint16 sw = trueres.w;
	Uint16 sh = trueres.h;
	SDL_Rect rect;
	//trace("got a rect %d %d %u %u", x, y, w, h); // DEBUG
	// Offscreen sprites may clip away to nothing
	if(x < 0) { w += x; x = 0; if(w <= 0) { return; }}
	rect.x = x > sw ? sw : x; sw -= rect.x;
	if(sw == 0) { return; }
	if(y < 0) { h += y; y = 0; if(h <= 0) { return; }}
	rect.y = y > sh ? sh : y; sh -= rect.y;
	if(sh == 0) { return; }
	rect.w = w > sw ? sw : w;
	rect.h = h > sh ? sh : h;
	dirtyrects.push_back(rect);
}

void ScaledVideo::dirtyRect(const SDL_Rect& rect)
	{ this->dirtyRect(rect.x, rect.y, rect.w, rect.h); }

void ScaledVideo::update() {
	/*trace("Dirtyrects:");
	for(std::vector<SDL_Rect>::const_iterator i = dirtyrects.begin();
		i != dirtyrects.end(); ++i) {

		trace("\t%dx%d + %dx%d", i->x, i->y, i->w, i->h);
	}*/ // DEBUG
	/* Theoretically, the documentation suggests attempting to avoid
	 * overdraw here. Realistically, there's not much we can do about that
	 * without subdivision, and apparently SDL ships out all the rectangles
	 * in one go to the graphics driver, so we'll let that do it if it
	 * feels so inclined. */
	if(dirtyrects.size() == 0) { return; }
	SDL_UpdateRects(SDL_GetVideoSurface(),
		dirtyrects.size(), &dirtyrects[0]);
		/* docs imply this is safe */
	dirtyrects.clear();
}

// True no-op. Actually makes virtualfb == true framebuffer.
class ScaledVideoNoOp : public ScaledVideo {
public:
	ScaledVideoNoOp(SDL_Rect virtres, SDL_Rect trueres, bool fullscreen) :
		ScaledVideo(virtres, trueres, false, false, fullscreen) {

		SDL_FreeSurface(virtualfb);
		virtualfb = SDL_GetVideoSurface();
		assert(virtres.w == trueres.w);
		assert(virtres.h == trueres.h);
		trace("\tusing no-op scaler");
	}

	~ScaledVideoNoOp() {
		// Stop the superdestructor from freeing the true video surface.
		virtualfb = 0;
	}
};

// No scaling, just translate. Not OpenGL (no need).
class ScaledVideoTranslateOnly : public ScaledVideo {
	SDL_Rect offset;
public:
	ScaledVideoTranslateOnly(SDL_Rect virtres, SDL_Rect trueres,
		bool fullscreen) :
		ScaledVideo(virtres, trueres, false, false, fullscreen) {

		offset.x = (trueres.w - virtres.w) / 2;
		offset.y = (trueres.h - virtres.h) / 2;
		trace("\tusing translate only scaler, %dx%d",
			offset.x, offset.y);
	}

	virtual void dirtyRect(Sint16 x, Sint16 y, Uint16 w, Uint16 h) {
		SDL_Rect srcrect, dstrect, dstrectcpy;
		// Blit accross to true framebuffer.
		srcrect.x = x; dstrect.x = x + offset.x;
		srcrect.y = y; dstrect.y = y + offset.y;
		srcrect.w = w; dstrect.w = w;
		srcrect.h = h; dstrect.h = h;
		dstrectcpy = dstrect; // BlitSurface is destructive
		SDL_BlitSurface(virtualfb, &srcrect,
			SDL_GetVideoSurface(), &dstrect);
		// Put the true framebuffer-relative rectangle in the dirty set
		ScaledVideo::dirtyRect(dstrectcpy.x, dstrectcpy.y, w, h);
	}
};

// Integer software scaler
class ScaledVideoInteger : public ScaledVideo {
	SDL_Rect offset;
	unsigned int scale;
public:
	ScaledVideoInteger(SDL_Rect virtres, SDL_Rect trueres, bool fullscreen)
		: ScaledVideo(virtres, trueres, false, false, fullscreen) {

		double scaletmp;
		if(scale_needed(virtres, trueres, &scaletmp)) {
			scale = (unsigned int) scaletmp;
		} else {
			warn("Integer scaler asked to perform non-int scaling");
			throw ScaledVideo::BadResolution();
		}
		offset.x = (trueres.w - (virtres.w * scale)) / 2;
		offset.y = (trueres.h - (virtres.h * scale)) / 2;

		trace("\tusing integer scaler, factor %u", scale);
	}

	// This is not wonderously efficient; it makes no attempt at batch or
	// parallel processing, so iterates over every single output pixel.
	// TODO Profile (or just benchmark), see if Arbitrary approach is
	// always faster; if so, improve or ditch this.
	virtual void dirtyRect(Sint16 x, Sint16 y, Uint16 w, Uint16 h) {
		// Check within bounds, else this can scramble memory
		assert(x >= 0); assert (y >= 0);
		assert(x + w <= virtres.w); assert(y + h <= virtres.h);
		// Copy and upscale data
		SDL_Surface* screen = SDL_GetVideoSurface();
		SDL_LockSurface(virtualfb);
		SDL_LockSurface(screen);
		Uint8 bypp = virtualfb->format->BytesPerPixel;
		assert(bypp == screen->format->BytesPerPixel);
		// (we demanded same format from SDL, remember)
		char* srcline = (char*) virtualfb->pixels
			+ (y * virtualfb->pitch);
		char* dstline = (char*) screen->pixels
			+ (((y*scale)+offset.y) * screen->pitch);
		for(Uint16 lines = h; lines; --lines) {
			for(unsigned int ydup = scale; ydup; --ydup) {
				char* srcpix = srcline + (x * bypp);
				char* dstpix = dstline +
					(((x*scale)+offset.x) * bypp);

				for(Uint16 cols = w; cols; --cols) {
					for(unsigned int xdup = scale; xdup;
						--xdup) {

						memcpy(dstpix, srcpix, bypp);
						dstpix += bypp;
					}
					srcpix += bypp;
				}
				dstline += screen->pitch;
			}
			srcline += virtualfb->pitch;
		}
		SDL_UnlockSurface(screen);
		SDL_UnlockSurface(virtualfb);

		// Adjust the update rectangle that gets stored
		ScaledVideo::dirtyRect(
			(x*scale) + offset.x, (y*scale) + offset.y,
			w * scale, h * scale);
	}
};

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

#ifdef SCALE_ARB_BLEND
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

#ifdef WITH_OPENGL
#error "OpenGL scaler not implemented."
// OpenGL scaler
/* Not yet implemented because it's quite a lot of extra faff for a scaler
 * whose quality won't exceed the arbitrary scaler, but involves a whole lot
 * more copying---so may well end up actually being slower. */
class ScaledVideoOpenGL : public ScaledVideo {
public:
	ScaledVideoOpenGL(SDL_Rect virtres, SDL_Rect trueres, bool fullscreen) :
		ScaledVideo(virtres, trueres, true, false, fullscreen) {

		warn("OpenGL upscaling unimplemented");
		throw ScaledVideo::BadResolution(); // TODO Implement

		// Construct appropriate geometry to fill the screen
		// Set up LINEAR texture scaling
		/* In theory, a large virtual screen may go over the graphics
		 * card's maximum texture dimensions. */

		trace("\tusing OpenGL scaler");
	}

	// update() always copies the whole surface. Keep dirtyrects empty.
	virtual void dirtyRect(Sint16 x, Sint16 y, Uint16 w, Uint16 h) {}

	virtual void update() {
		// TODO Copy whole surface as a texture.
		// Remember to keep it in a power-of-two square.

		SDL_GL_SwapBuffers();
	}
};
#endif

// Factory functions //////////////////////////////////////////////////////////

ScaledVideo* getScaledVideoManual(bool hardware, SDL_Rect virtres,
	 SDL_Rect trueres, bool fullscreen) {
#define DESCRIBE trueres.w, trueres.h, \
	hardware?"hard":"soft", fullscreen?"fullscreen":"windowed"
#define USE(X) try { return new X; } \
	catch(ScaledVideo::BadResolution) { \
		warn("Can't use video mode %dx%d %sware %s!", DESCRIBE); \
		return NULL; \
	}

	trace("Trying video mode %dx%d %sware %s.", DESCRIBE);
	if((virtres.w == trueres.w) || (virtres.h == trueres.h)) {
// FIXME Only if pixel format matches
//		if((virtres.w == trueres.w) && (virtres.h == trueres.h)) {
//			USE(ScaledVideoNoOp(virtres, trueres, fullscreen))
//		} else {
			USE(ScaledVideoTranslateOnly(
				virtres, trueres, fullscreen))
//		}
	}

#ifdef WITH_OPENGL
	if(hardware) {
		USE(ScaledVideoOpenGL(virtres, trueres, fullscreen))
#else
	if(hardware) { warn("Hardware accelleration support not compiled."); }
	if(0) { // And a } to appease naieve bracket-matching.
#endif
	} else {
		double upscale;
		bool integer = scale_needed(virtres, trueres, &upscale);
		if(integer) {
			USE(ScaledVideoInteger(virtres, trueres, fullscreen))
		} else {
			/* TODO Mechanism to select 'high quality' arbitrary
			 * scaler at runtime. It's slightly slower (the cache
			 * helps a lot), and currently looks worse at lower
			 * scale-up factors IMO. Once you get to about 800x600,
			 * it's probably an improvement. */
#ifdef SCALE_ARB_BLEND
			USE(ScaledVideoArbitraryHQ(virtres,trueres,fullscreen))
#else
			USE(ScaledVideoArbitrary(virtres, trueres, fullscreen))
#endif
		}
	}
#undef DESCRIBE
#undef USE
}
