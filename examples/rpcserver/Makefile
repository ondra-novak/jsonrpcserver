
.PHONY: all clean
all: bin/rpcserver

bin/rpcserver:  SrvMain.cpp SrvMain.h ../../libjsonrpcserver.a ../../../lightspeed/liblightspeed.a
	mkdir -p bin
	$(CXX) -o bin/rpcserver -lrt -L../.. -L ../../../lightspeed SrvMain.cpp -I../../ -I../../../lightspeed/src -ljsonrpcserver -llightspeed -lpthread

../../libjsonrpcserver.a: 
	$(MAKE) -C ../.. all

../../../lightspeed/liblightspeed.a: 
	$(MAKE) -C ../../../lightspeed all

clean:
	rm -f bin/rpcserver
	rm -f ../../libjsonrpcserver.a
	rm -f ../../../lightspeed/liblightspeed.a

