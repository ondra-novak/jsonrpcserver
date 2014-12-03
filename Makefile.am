
ACLOCAL_AMFLAGS = -I Build 

SUBDIRS = .


noinst_LIBRARIES = libjsonrpcserver.a

LIGHTSPEED_H = $(abs_top_srcdir)/../lightspeed/src
LIGHTMYSQL_H = $(abs_top_srcdir)/../lightmysql/src

libjsonrpcserver.a: FORCE
	$(MAKE) -C Build --file makefile all "GPP=g++ -O3  -c -fno-strict-aliasing -Wall -fPIC -I$(LIGHTSPEED_H) -I$(LIGHTMYSQL_H)"
	cp Build/libjsonrpcserver.a ./
	
clean-local: clean-src
distclean-local: clean-src
.PHONY: clean-src

clean-src: FORCE
	$(MAKE) -k -C Build --file makefile clean
	rm -f ./libjsonrpcserver.a


FORCE: