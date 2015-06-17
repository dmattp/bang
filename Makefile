PATH=c:/mingw/bin:$PATH

USE_GC=0
PATH_BOEHM=c:\m\home\pkg\boehm-gc\build1
CPPFLAGS += --std=c++11 -O2

all:: bang.exe #-- mathlib.dll
all:: mathlib.dll
all:: stringlib.dll

bang.exe: bang.cpp bang.h Makefile
ifeq (1,$(USE_GC))
	$(CXX) $(CPPFLAGS) -D USE_GC=1 -I $(PATH_BOEHM)\include -Wl,--stack,33554432 bang.cpp -L$(PATH_BOEHM) -lgc -o $@
else
	$(CXX) $(CPPFLAGS) -Wl,--stack,33554432 $< -o $@
endif


mathlib.dll: mathlib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -o $@ $<

stringlib.dll: stringlib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -o $@ $<

#  -Wl,--out-implib,$(@:.dll=.a)

# -g0 doesnt seem to actually drop any size, maybe need linker option too?
# mingw32-g++ -O2 --std=c++11 bang.cpp -o $@

#	mingw32-g++ -O2 -Wl,--stack,33554432 --std=c++11 bang.cpp -o $@

# Default stack size seems to be 2M?
#   mingw32-g++ -Wl,--stack,16777216 -O2 --std=c++11 bang.cpp -o $@
