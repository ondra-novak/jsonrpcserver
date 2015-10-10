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
NEEDLIBS:=$(LIBLIGHTMYSQL)
include dbhelpers/dbhelpers.mk

endif 


include building/build_lib.mk


	