
.PHONY: all clean
all: bin/simplechat

bin/simplechat:  serverMain.cpp serverMain.h ../../libjsonrpcserver.a ../../../lightspeed/liblightspeed.a
	mkdir -p bin
	g++ -o bin/simplechat -L../.. -L ../../../lightspeed serverMain.cpp -I../../ -I../../../lightspeed/src -ljsonrpcserver -llightspeed -lpthread

../../libjsonrpcserver.a: 
	$(MAKE) -C ../.. all

../../../lightspeed/liblightspeed.a: 
	$(MAKE) -C ../../../lightspeed all

clean:
	rm bin/simplechat
	rm ../../libjsonrpcserver.a
	rm ../../../lightspeed/liblightspeed.a

