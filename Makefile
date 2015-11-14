-include build/site.mak

ifeq (,$(BUILD_FOR)) # if site.mak doesn't include a target (linux.mak,win32.mak) default to linux
 include build/linux.mak
endif

all:: bang$(EXT_EXE) #-- mathlib.dll
all:: mathlib$(EXT_SO)
all:: stringlib$(EXT_SO)
all:: iolib$(EXT_SO)

CPPOPTLEVEL ?= -O2
CPPFLAGS += --std=c++11 $(CPPOPTLEVEL)
HAVE_BUILTIN_ARRAY=1
HAVE_BUILTIN_HASH=1


# CPPFLAGS += -march=i686

# -fomit-frame-pointer

#CPPFLAGS += -S

ifeq (1,$(USE_GC))
 LDFLAGS += -L$(DIR_BOEHM_LIB) -lgc
 CPPFLAGS += -D USE_GC=1 -I $(DIR_BOEHM_HDR) $(LDFLAGS_GC)
endif

# GNU lightning JIT
#   DIR_LIGHTNING_LIB should be set in [site.mak] with path to liblightning.so/.a
#   Also set CPPFLAGS_LIGHTNING in [site.mak] if necessary for include paths
ifeq (1,$(USE_LIGHTNING))
LDFLAGS += -L$(DIR_LIGHTNING_LIB) -llightning
CPPFLAGS += $(CPPFLAGS_LIGHTNING) -DLCFG_TRYJIT=1
endif


OBJS_LIBBANG=bang.o

ifeq (1,$(HAVE_BUILTIN_ARRAY))
   CPPFLAGS += -DHAVE_BUILTIN_ARRAY=1
   OBJS_LIBBANG += arraylib.o
else
all:: arraylib$(EXT_SO)
endif

ifeq (1,$(HAVE_BUILTIN_HASH))
   CPPFLAGS += -DHAVE_BUILTIN_HASH=1
   OBJS_LIBBANG += hashlib.o
else
all:: hashlib$(EXT_SO)
endif


bang.o: bang.cpp bang.h Makefile
	$(CXX) $(CPPFLAGS) -c $< -o $@

%.o: %.cpp bang.h Makefile
	$(CXX) $(CPPFLAGS) -c $< -o $@

libbang$(EXT_SO): $(OBJS_LIBBANG)
	$(CXX) $(OBJS_LIBBANG) $(LDFLAGS) $(LDFLAGS_DL) $(LDFLAGS_THREADLIB) -shared -o $@

bang$(EXT_EXE): bangmain.o bang.h Makefile libbang$(EXT_SO)
	$(CXX) $< -L . -lbang $(LDFLAGS_THREADLIB) -o $@

ifneq (1,$(HAVE_BUILTIN_ARRAY))
arraylib$(EXT_SO): arraylib.o
	$(CXX) $< $(LDFLAGS) $(LDFLAGS_DL) -shared -o $@
endif

ifneq (1,$(HAVE_BUILTIN_HASH))
hashlib$(EXT_SO): hashlib.cpp hashlib.h bang.h 
	$(CXX) $(CPPFLAGS) -shared -L . -lbang -o $@ $<
endif 

mathlib$(EXT_SO): mathlib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -L. -lbang -o $@ $<

stringlib$(EXT_SO): stringlib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -L . -lbang -o $@ $<

iolib$(EXT_SO): iolib.cpp bang.h
	$(CXX) $(CPPFLAGS) -shared -L. -lbang -o $@ $<


ifeq (1,0)
all:: bangone$(EXT_EXE)
bangone$(EXT_EXE): bangmain.cpp bang.cpp hashlib.cpp 
	$(CXX) $(CPPFLAGS)  $? -L . -o $@
endif

clean:
	-rm *.tlog
	-rm *.dll
	-rm *.exe
	-rm *.manifest
	-rm *.o




