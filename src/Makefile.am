AM_CXXFLAGS = -std=gnu++98 -Wall -Werror

bin_PROGRAMS = strangelp
strangelp_SOURCES = \
	cards.cpp \
	cards.h \
	combat.cpp \
	combat_display.cpp \
	combat.h \
	combat_init.cpp \
	combat_sim.cpp \
	combat_weapons.cpp \
	endgame.cpp \
	endgame.h \
	font.cpp \
	gfx.cpp \
	gfx.h \
	iface_globals.h \
	interface.cpp \
	interface.h \
	is_fileio.cpp \
	is_fileio.h \
	main.cpp \
	modconfig.cpp \
	resource.h \
	sais_version.h \
	scaledvideo.cpp \
	scaledvideo.hpp \
	sdl_iface.cpp \
	sdl_main.cpp \
	snd.h \
	sound.cpp \
	sprite_init.cpp \
	sprites.cpp \
	starmap.cpp \
	starmap_encounters.cpp \
	starmap.h \
	starmap_init.cpp \
	starmap_inventory.cpp \
	startgame.cpp \
	startgame.h \
	textstr.cpp \
	textstr.h \
	typedefs.h \
	w32_gfx.cpp \
	w32_sound.cpp

# If we're doing a Windows build, get the icon in there too
if WINDOWS
strangelp_LDADD = windows_resources.o

windows_resources.o: windows_resources.rc ../winicon.ico
	i586-mingw32msvc-windres -v -o $@ $<
endif
