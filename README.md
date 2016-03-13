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

I plan to cross-compile a nice drop-in replacement Windows binary from Linux instead.

## Other contributors
Original game by Digital Eel (Richard Carlson, Iikka Keranen and William Sears; see [the original readme.txt](https://github.com/LionsPhil/strangeadventures/blob/2e61e6274d76e96c9f517aa71434a89ce9b5f58a/readme.txt)).
There are some build fixes by Chris Collins that have been passed about as tarballs; I'm afraid any better attribution is lost.
