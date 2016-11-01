
.PHONY: all clean
all: bin/detachexample

bin/detachexample:  SrvMain.cpp SrvMain.h ../../libjsonrpcserver.a ../../../lightspeed/liblightspeed.a
	mkdir -p bin
	g++ -std=c++11 -o bin/detachexample  -L../.. -L ../../../lightspeed SrvMain.cpp -I../../ -I../../../lightspeed/src -ljsonrpcserver -llightspeed -lpthread -limmujson 

../../libjsonrpcserver.a: 
	$(MAKE) -C ../.. all

../../../lightspeed/liblightspeed.a: 
	$(MAKE) -C ../../../lightspeed all

clean:
	rm bin/detachexample
	rm ../../libjsonrpcserver.a
	rm ../../../lightspeed/liblightspeed.a

