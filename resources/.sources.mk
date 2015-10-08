OBJS+=resources/rpc_js.o resources/wsrpc_js.o


.INTERMEDIATE : resources/rpc_js.c resources/wsrpc_js.c bin2c

bin2c: resources/bin2c.c
	@echo Building resources
	@$(CC) -o $@ resources/bin2c.c

resources/rpc_js.c: resources/rpc.js bin2c
	@./bin2c resources/rpc.js jsonrpcserver_rpc_js > $@
	
resources/wsrpc_js.c: resources/wsrpc.js bin2c	
	@./bin2c resources/wsrpc.js jsonrpcserver_wsrpc_js > $@

# prevent to apply default rule js.c to js as make thinks that js is executable
%.js : %.js.c
	@ echo
	
	
	 
