# Strange Adventures in Infinite Space
This is an **unofficial** fork of *Strange Adventures in Infinite Space* by Digital Eel, based on their GPL release:

http://digital-eel.com/sais/source.htm

This is only the game code; the game data is under a non-Free license, but they did [release it for free for Windows](http://digital-eel.com/sais/buy.htm).
That release seems to use an older codebase, though, that is not very happy under modern Windows (I get horrible palette errors despite forcing 256-color mode, etc.).
This fork uses their SDL version of the codebase, and works with the free full release data.

## Building
### Linux
Run `./autoeverything && ./configure && make`. You'll need autotools and SDL-1.2 development files installed, including SDL\_mixer.

If you get copy in the `gamedata`, `graphics`, `mods`, and `sounds` folders from a Windows install, you should then be able to run `src/strangelp`.

### Windows
I suspect the Visual Studio project files don't work any more. They're as-is from the original GPL release.

Cross-compiling from Linux is a bit slapdash at the moment, but works:

1. Set up a mingw32 SDL cross-compile environment by following [Dana Olsen's guide](https://icculus.org/~dolson/sdl/). Note that on a modern system you probably need to invoke the script with `bash`, not `sh`, and you probably need to edit it so the patch file it generates patches `sdl-config` directly, rather than the symlink to it else it will error out.
2. Go into `include/SDL` in that environment and `ln -s ../SDL_mixer.h`, because it puts the extra library headers outside the main `SDL` include directory, unlike the host Linux system. (You may also want to do this for `SDL_image.h` etc. for completeness.)
3. `export ac_cv_func_malloc_0_nonnull=yes` [because autoconf](http://wiki.buici.com/xwiki/bin/view/Programing+C+and+C%2B%2B/Autoconf+and+RPL_MALLOC)
4. `./autoeverything && PATH=/wherever/you/put/sdl-win32/bin:$PATH ./configure --enable-windows --target=i586-mingw32msvc --host=i586-mingw32msvc --build=i586-linux && make`

Note that `--enable-windows` just turns on the `WINDOWS` define in the code so it talks Win32 API at points; you still need to use the cross-compiler!

To run this, copy the contents of `runtime` from the SDL cross-compile environment and `src/strangelp.exe` into a folder along with `gamedata` et. al.

## Other contributors
Original game by Digital Eel (Richard Carlson, Iikka Keranen and William Sears; see [the original readme.txt](https://github.com/LionsPhil/strangeadventures/blob/2e61e6274d76e96c9f517aa71434a89ce9b5f58a/readme.txt)).
Thanks to [Chris Collins for initial Linux porting work](http://nekohako.xware.cx/sais/index.html) upon which this builds.
