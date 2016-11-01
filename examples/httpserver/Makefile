
.PHONY: all clean
all: bin/httpserver

bin/httpserver:  SrvMain.cpp SrvMain.h ../../libjsonrpcserver.a ../../../lightspeed/liblightspeed.a
	mkdir -p bin
	g++ -std=c++11 -o bin/httpserver  -L../.. -L ../../../lightspeed SrvMain.cpp -I../../ -I../../../lightspeed/src -ljsonrpcserver -llightspeed -lpthread  -limmujson 

../../libjsonrpcserver.a: 
	$(MAKE) -C ../.. all

../../../lightspeed/liblightspeed.a: 
	$(MAKE) -C ../../../lightspeed all

clean:
	rm bin/httpserver
	rm ../../libjsonrpcserver.a
	rm ../../../lightspeed/liblightspeed.a

