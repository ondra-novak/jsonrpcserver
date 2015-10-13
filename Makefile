LIBNAME:=jsonrpcserver
LIBINCLUDES:=
with_dbhelpers ?= 0
SEARCHPATHS="./ ../ ../../ /usr/include/ /usr/local/include/"
CXX=clang++

selectpath=$(abspath $(firstword $(foreach dir,$(1),$(wildcard $(dir)$(2)))))

LIBLIGHTSPEED=$(call selectpath,$(SEARCHPATHS),lightspeed)
NEEDLIBS:=$(LIBLIGHTSPEED)
 

ifeq "$(with_dbhelpers)" "1"

LIBLIGHTMYSQL=$(call selectpath,$(SEARCHPATHS),lightmysql)
NEEDLIBS+=$(LIBLIGHTMYSQL)
CONFIG=tmp/with_dbhelpers
else
CONFIG=tmp/without_dbhelpers
endif 

$(CONFIG):
	@rm -f tmp/with_dbhelpers tmp/without_dbhelpers
	mkdir -p tmp
	@touch $(CONFIG)
	@echo $(LIBNAME): with_dbhelpers=$(with_dbhelpers)

include building/build_lib.mk


	