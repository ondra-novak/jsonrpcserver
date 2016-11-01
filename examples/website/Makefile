
.PHONY: all clean
all: bin/website

bin/website:  SrvMain.cpp SrvMain.h ../../libjsonrpcserver.a ../../../lightspeed/liblightspeed.a
	mkdir -p bin
	g++ -std=c++11  -o bin/website -L../.. -L ../../../lightspeed SrvMain.cpp -I../../ -I../../../lightspeed/src -ljsonrpcserver -llightspeed -lpthread -limmujson

../../libjsonrpcserver.a: 
	$(MAKE) -C ../.. all

../../../lightspeed/liblightspeed.a: 
	$(MAKE) -C ../../../lightspeed all

clean:
	rm -f bin/website
	rm -f ../../libjsonrpcserver.a
	rm -f ../../../lightspeed/liblightspeed.a

