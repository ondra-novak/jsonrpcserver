
all: rpc.js.c wsrpc.js.c

.SUFFIXES:

bin2c: bin2c.c
	gcc -o bin2c bin2c.c

rpc.js.c: rpc.js bin2c
	./bin2c rpc.js jsonrpcserver_rpc_js > rpc.js.c
	
wsrpc.js.c: wsrpc.js bin2c
	./bin2c wsrpc.js jsonrpcserver_wsrpc_js > wsrpc.js.c

	
clean:
	rm -f bin2c
	rm -f rpc.js.c
	rm -f wsrpc.js.c
	