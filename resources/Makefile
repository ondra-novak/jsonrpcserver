
all: rpc.js.c

.SUFFIXES:

bin2c: bin2c.c
	gcc -o bin2c bin2c.c

rpc.js.c: rpc.js bin2c
	./bin2c rpc.js jsonrpcserver_rpc_js > rpc.js.c
	

	
clean:
	rm bin2c
	rm rpc.js.c
	