
.PHONY: all clean
all: bin/rpcserver

bin/rpcserver:  SrvMain.cpp SrvMain.h ../../libjsonrpcserver.a ../../../lightspeed/liblightspeed.a
	mkdir -p bin
	g++ -o bin/rpcserver -lrt -L../../Debug -L ../../../lightspeed/Debug SrvMain.cpp -I../../ -I../../../lightspeed/src -ljsonrpcserver -llightspeed -lpthread

../../libjsonrpcserver.a: 
	$(MAKE) -C ../.. all

../../../lightspeed/liblightspeed.a: 
	$(MAKE) -C ../../../lightspeed all

clean:
	rm bin/httpserver
	rm ../../libjsonrpcserver.a
	rm ../../../lightspeed/liblightspeed.a

