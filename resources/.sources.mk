OBJS+=tmp/resources/rpc_js.o tmp/resources/wsrpc_js.o
clean_list+=tmp/bin2c tmp/resources/wsrpc_js.c tmp/resources/rpc_js.c


#.INTERMEDIATE : tmp/resources/rpc_js.c tmp/resources/wsrpc_js.c tmp/bin2c


tmp/bin2c: resources/bin2c.c
	@echo Building resources
	mkdir -p tmp/resources
	$(CC) -o $@ resources/bin2c.c

tmp/resources/rpc_js.c: resources/rpc.js tmp/bin2c
	@tmp/bin2c resources/rpc.js jsonrpcserver_rpc_js > $@
	
tmp/resources/wsrpc_js.c: resources/wsrpc.js tmp/bin2c	
	@tmp/bin2c resources/wsrpc.js jsonrpcserver_wsrpc_js > $@

# prevent to apply default rule js.c to js as make thinks that js is executable
%.js : %.js.c
	@ echo
	
	
	 
