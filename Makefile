PATH=c:/mingw/bin:$PATH
bang.exe: bang.cpp Makefile
	g++ -O2 -Wl,--stack,33554432 --std=c++11 bang.cpp -o $@


# -g0 doesnt seem to actually drop any size, maybe need linker option too?
# mingw32-g++ -O2 --std=c++11 bang.cpp -o $@


#	mingw32-g++ -O2 -Wl,--stack,33554432 --std=c++11 bang.cpp -o $@


# Default stack size seems to be 2M?
#   mingw32-g++ -Wl,--stack,16777216 -O2 --std=c++11 bang.cpp -o $@
