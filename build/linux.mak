EXT_SO=.so
EXT_EXE=
BUILD_FOR=linux
USE_GC=0
DIR_BOEHM_HDR=/usr/include/gc
DIR_BOEHM_LIB=/usr/lib
CPPFLAGS += -fpermissive -fPIC 
LDFLAGS_DL = -ldl -lpthread
#CPPFLAGS += -DLCFG_MT_SAFEISH=0
#CPPOPTLEVEL=-O3
