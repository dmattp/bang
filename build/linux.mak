EXT_SO=.so
EXT_EXE=
BUILD_FOR=linux
USE_GC=0
DIR_BOEHM_HDR=/usr/include/gc
DIR_BOEHM_LIB=/usr/lib
CPPFLAGS += -fpermissive -fPIC 
LDFLAGS_DL = -ldl -lpthread

include $(DIR_BANG)/build/gcc.mak
