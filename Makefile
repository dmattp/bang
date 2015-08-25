-include build/site.mak

ifeq (,$(BUILD_FOR)) # if site.mak doesn't include a target (linux.mak,win32.mak) default to linux
 include build/linux.mak
endif

all:: bang$(EXT_EXE) #-- mathlib.dll
all:: mathlib$(EXT_SO)
all:: arraylib$(EXT_SO)
all:: stringlib$(EXT_SO)
all:: hashlib$(EXT_SO)
all:: iolib$(EXT_SO)

CPPOPTLEVEL ?= -O2
CPPFLAGS += --std=c++11 $(CPPOPTLEVEL)

# CPPFLAGS += -march=i686

# -fomit-frame-pointer

ifeq (1,$(USE_GC))
 LDFLAGS_GC=-L$(DIR_BOEHM_LIB) -lgc
 CPPFLAGS_GC=-D USE_GC=1 -I $(DIR_BOEHM_HDR) $(LDFLAGS_GC)
endif

libbang$(EXT_SO): bang.cpp bang.h Makefile
	$(CXX) $(CPPFLAGS) $(CPPFLAGS_GC)  $< $(LDFLAGS_GC) $(LDFLAGS_DL) -shared -o $@

bang$(EXT_EXE): bangmain.cpp bang.h Makefile libbang$(EXT_SO)
	$(CXX) $(CPPFLAGS)  $< -L . -lbang -o $@

mathlib$(EXT_SO): mathlib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -L. -lbang -o $@ $<

arraylib$(EXT_SO): arraylib.cpp arraylib.h bang.h
	$(CXX) $(CPPFLAGS) -shared -L. -lbang -o $@ $<

stringlib$(EXT_SO): stringlib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -L . -lbang -o $@ $<

hashlib$(EXT_SO): hashlib.cpp hashlib.h bang.h 
	$(CXX) $(CPPFLAGS) -shared -L . -lbang -o $@ $<

iolib$(EXT_SO): iolib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -L. -lbang -o $@ $<


ifeq (1,0)
all:: bangone$(EXT_EXE)
bangone$(EXT_EXE): bangmain.cpp bang.cpp hashlib.cpp 
	$(CXX) $(CPPFLAGS)  $? -L . -o $@
endif


#  -Wl,--out-implib,$(@:.dll=.a)

# -g0 doesnt seem to actually drop any size, maybe need linker option too?
# mingw32-g++ -O2 --std=c++11 bang.cpp -o $@

#	mingw32-g++ -O2 -Wl,--stack,33554432 --std=c++11 bang.cpp -o $@

# Default stack size seems to be 2M?
#   mingw32-g++ -Wl,--stack,16777216 -O2 --std=c++11 bang.cpp -o $@
