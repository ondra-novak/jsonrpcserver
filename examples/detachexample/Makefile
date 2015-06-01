
.PHONY: all clean
all: bin/detachexample

bin/detachexample:  SrvMain.cpp SrvMain.h ../../libjsonrpcserver.a ../../../lightspeed/liblightspeed.a
	mkdir -p bin
	g++ -o bin/detachexample -lrt -L../../Debug -L ../../../lightspeed/Debug SrvMain.cpp -I../../ -I../../../lightspeed/src -ljsonrpcserver -llightspeed -lpthread

../../libjsonrpcserver.a: 
	$(MAKE) -C ../.. all

../../../lightspeed/liblightspeed.a: 
	$(MAKE) -C ../../../lightspeed all

clean:
	rm bin/detachexample
	rm ../../libjsonrpcserver.a
	rm ../../../lightspeed/liblightspeed.a

