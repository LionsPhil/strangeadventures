# Strange Adventures in Infinite Space
This is an **unofficial** fork of *Strange Adventures in Infinite Space* by Digital Eel, based on their GPL release:

http://digital-eel.com/sais/source.htm

This is only the game code; the game data is under a non-Free license, but they did [release it for free for Windows](http://digital-eel.com/sais/buy.htm).
That release seems to use an older codebase, though, that is not very happy under modern Windows (I get horrible palette errors despite forcing 256-color mode, etc.).
This fork uses their SDL version of the codebase, and works with the free full release data.

## Fork features

* Builds warning-clean for Linux and cross-compiles for Windows
* Resizable window, including fullscreen (toggle with F11): no more postage-stamp!

## Installing (Windows)

1. Install the [official free release](http://digital-eel.com/sais/buy.htm). You need this to get the game data.
2. Download the most recent build of this fork from [the releases page](https://github.com/LionsPhil/strangeadventures/releases).
3. Open the folder you installed the game to, and extract the ZIP from step 2 into it.
4. Run `strangelp.exe`.
5. If everything's good and you want to use my version by default, either move the original aside and rename mine, or create a new shortcut mine.

## Building
### Linux
Run `./autoeverything && ./configure && make`. You'll need autotools and SDL-1.2 development files installed, including SDL\_mixer.

If you get copy in the `gamedata`, `graphics`, `mods`, and `sounds` folders from a Windows install, you should then be able to run `src/strangelp`.

If you don't have any access to Windows, the game data from the MacOS X release should work and can be extracted under Linux:

1. Download the [official free MacOS X release](http://digital-eel.com/sais/buy.htm).
2. Go into a temporary folder and extract the DMG: `7z x sais-osx.dmg`
3. `mkdir /tmp/sais && sudo mount -o loop 2.hfs /tmp/sais`
4. Copy `/tmp/sais/mods` to where you want to keep the game data
5. Copy `/tmp/sais/Strange\ Adventures\ in\ Infinite\ Space.app/Contents/MacOS/{gamedata,graphics,sounds}` as well
6. `sudo umount /tmp/sais && sudo -k`

### Windows
Cross-compiling from Linux is a bit slapdash at the moment, but works:

1. Set up a mingw32 SDL cross-compile environment by following [Dana Olsen's guide](https://icculus.org/~dolson/sdl/). Note that on a modern system you probably need to invoke the script with `bash`, not `sh`, and you probably need to edit it so the patch file it generates patches `sdl-config` directly, rather than the symlink to it else it will error out.
2. Go into `include/SDL` in that environment and `ln -s ../SDL_mixer.h`, because it puts the extra library headers outside the main `SDL` include directory, unlike the host Linux system. (You may also want to do this for `SDL_image.h` etc. for completeness.)
3. `export ac_cv_func_malloc_0_nonnull=yes` [because autoconf](http://wiki.buici.com/xwiki/bin/view/Programing+C+and+C%2B%2B/Autoconf+and+RPL_MALLOC)
4. `./autoeverything && PATH=/wherever/you/put/sdl-win32/bin:$PATH ./configure --enable-windows --target=i586-mingw32msvc --host=i586-mingw32msvc --build=i586-linux && make`

Note that `--enable-windows` just turns on the `WINDOWS` define in the code so it talks Win32 API at points; you still need to use the cross-compiler!

To run this, copy the contents of `runtime` from the SDL cross-compile environment and `src/strangelp.exe` into a folder along with `gamedata` et. al.

I suspect the Visual Studio project files don't work any more. They're as-is from the original GPL release. At the very least they will need to have the list of source files updated.

## Other contributors
Original game by Digital Eel (Richard Carlson, Iikka Keranen and William Sears; see [the original readme.txt](https://github.com/LionsPhil/strangeadventures/blob/2e61e6274d76e96c9f517aa71434a89ce9b5f58a/readme.txt)).
Thanks to [Chris Collins for initial Linux porting work](http://nekohako.xware.cx/sais/index.html) upon which this builds.
