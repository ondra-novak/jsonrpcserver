LIBNAME:=jsonrpcserver
LIBINCLUDES:=
with_dbhelpers ?= 0
SEARCHPATHS="./ ../ ../../ /usr/include/ /usr/local/include/"
CXX=clang++
CXXFLAGS=-std=c++11

selectpath=$(abspath $(firstword $(foreach dir,$(1),$(wildcard $(dir)$(2)))))

LIBLIGHTSPEED=$(call selectpath,$(SEARCHPATHS),lightspeed)
NEEDLIBS:=$(LIBLIGHTSPEED)
 

ifeq "$(with_dbhelpers)" "1"

LIBLIGHTMYSQL=$(call selectpath,$(SEARCHPATHS),lightmysql)
NEEDLIBS+=$(LIBLIGHTMYSQL)
endif 

CONFIG=config.h

include $(LIBLIGHTSPEED)/building/build_lib.mk
include $(LIBLIGHTSPEED)/building/testfns.mk


$(CONFIG): testheader Makefile 
	@echo Detecting features...
	@./testheader openssl/ssl.h HAVE_OPENSSL > $@
	


EXAMPLES += $(wildcard examples/*)

examples: $(LIBFULLNAME)
	for I in $(EXAMPLES); do $(MAKE) -C $$I& done
	
	