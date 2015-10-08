
all: rpcjs.c wsrpcjs.c



.INTERMEDIATE :  bin2c

bin2c: bin2c.c
	@echo Building resources
	$(CC) -o $@ bin2c.c

rpcjs.c: rpc.js bin2c
	@./bin2c rpc.js jsonrpcserver_rpc_js > $@
	
wsrpcjs.c: wsrpc.js bin2c	
	@./bin2c wsrpc.js jsonrpcserver_wsrpc_js > $@

# prevent to apply default rule js.c to js as make thinks that js is executable
%.js : %.js.c
	@ echo
	
	
	 	
clean:
	rm -f rpcjs.c
	rm -f wsrpcjs.c
	