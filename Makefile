USE_GC=0
include build/linux.mak
#include build/win32.mak
-include build/site.mak

all:: bang$(EXT_EXE) #-- mathlib.dll
all:: mathlib$(EXT_SO)
all:: stringlib$(EXT_SO)

CPPFLAGS += --std=c++11 -O2

ifeq (1,$(USE_GC))
 CPPFLAGS_GC=-D USE_GC=1 -I $(PATH_BOEHM)\include
 LDFLAGS_GC=-L$(PATH_BOEHM) -lgc
endif

bang$(EXT_EXE): bang.cpp bang.h Makefile
	$(CXX) $(CPPFLAGS) $(CPPFLAGS_GC) -Wl,--stack,33554432 $< $(LDFLAGS_GC) -o $@

mathlib$(EXT_SO): mathlib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -o $@ $<

stringlib$(EXT_SO): stringlib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -o $@ $<

#  -Wl,--out-implib,$(@:.dll=.a)

# -g0 doesnt seem to actually drop any size, maybe need linker option too?
# mingw32-g++ -O2 --std=c++11 bang.cpp -o $@

#	mingw32-g++ -O2 -Wl,--stack,33554432 --std=c++11 bang.cpp -o $@

# Default stack size seems to be 2M?
#   mingw32-g++ -Wl,--stack,16777216 -O2 --std=c++11 bang.cpp -o $@
