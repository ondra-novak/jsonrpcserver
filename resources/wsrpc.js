///Initializes RPC client
/**
* @param rpc_url - url of rpc server
* @param method_list - array of methods. This list can be retrieved linking the script "RPC/methods/XXX.js" where
* @param options - optional object. Currently only one option is available "useMulticall" which can be true or false. Default is true.
*                   If useMulticall is true, then multiple requests are packed into single "Server.multicall" call.  
*                   If useMulticall is false, then multiple requests are no longer packed. They are sent separatedly each for single request.
*                   Note that object can process one request at time, other requests are queued or combined into multicall pack
* XXX is name of variable that will contain list of methods
*
* Use operator new to construct object. Object will map all methods to the object as standard javascript methods
*/
function WsRpcClient (wsrpc_url, method_list, options) {

	var lastIdent = 0;
	var idtable = {};
	var clientMethods={};
	var eventListeners={};
	var me = this;
	var opened = false;
	
	function getWsPath(relPath) {
		if (relPath.substr(0,6) == "wss://" || relPath.substr(0,5) == "ws://" ) return relPath;
		var loc = window.location, new_uri;
		if (loc.protocol === "https:") {
		    new_uri = "wss:";
		} else {
		    new_uri = "ws:";
		}
		new_uri += "//" + loc.host;
		if (relPath.substr(0,1) == "/") new_uri += relPath;
		else {
			var idx = loc.pathname.lastIndexOf('/');
			new_uri += loc.pathname.substr(0,idx+1) + relPath;
		}
		return new_uri;
	}
	
	var url = getWsPath(wsrpc_url);
	var connection = null;
	var reconnectIfNeed = false;

	var connect = function() {
		if (connection) return this;		
		connection = new WebSocket(url);
		connection.onopen = function(evt) { onOpen(evt) };
		connection.onclose = function(evt) { onClose(evt) };
	    connection.onmessage = function(evt) { onMessage(evt) };
//	    connection.onerror = function(evt) { onClose (evt) };
	    reconnectIfNeed = true;
	    return this;
	}

	var sendRequest = function(req) {
		connection.send(JSON.stringify(req));
	}
	

	
	var onClientMethod = function(method, arguments, id) {
		if (id == null) {
			var m = eventListeners[method];
			if (m) {
				m = m.slice(0); 
				for (var i = 0; i < m.length; i++) {
					m[i](method, arguments, me);
				}
			}
			m = eventListeners["*"]
			if (m) {
				m = m.slice(0);
				for (var i = 0; i < m.length; i++) {
					m[i](method, arguments, me);
				}				
			}
		} else {
			var m = clientMethods[method];
			if (m) {
				var res = m(arguments);
				if (res instanceof Promise) {
					return res.then(sendClientResponse.bind(id,true),sendClientResponse.bind(id,false));
				} else if (res.error) {
					sendClientResponse(id,false,res.error);
				} else if (res.result) {
					sendClientResponse(id,true,res.result);
				} else {
					sendClientResponse(id,true,res);
				}
			}
		}
	}
	
	var sendClientResponse = function(id,iserror,value) {
		var json = iserror?
				{id:id,result:null,error:value}:
				{id:id,result:value,error:null};
		var jsonstr = JSON.stringify(json);
		connection.send(jsonstr);
	} 
	
	var onServerResponse = function(result,error,id,context) {
		var storedpromise = idtable[id];
		if (storedpromise) {
			delete idtable[id];
			if (error) {
				storedpromise.reject(error);				
			} else {
				if (context) updateContext(context);
				storedpromise.accept(result);
			}
		}
	}
		

	var resendRequests = function() {
		for (var x in idtable) {
			var req = idtable[x];
			sendRequest(req.request);
		}
	}
	
	var cancelRequests = function() {
		for (var x in idtable) {
			var req = idtable[x];
			req.reject(me.FAILED);
		}
		idtable = {}
	}
	
	var onOpen = function(evt) {
		onClientMethod("status",["connect"],null);
		opened = true;
		resendRequests();
	}
	var onClose = function(evt) {
		onClientMethod("status",["disconnect"],null);
		if (reconnectIfNeed) {
			if ( Object.keys(idtable).length === 0) {
				cleanup();		
				connect();
			} else {
				me.onConnectionError(evt.data, function(x) {
					cleanup(x);
					connect();
				});
			}
		}
	}
	var onMessage = function(evt) {
		var jsonreq = JSON.parse(evt.data);
		if (jsonreq.method) {
			onClientMethod(jsonreq.method,jsonreq.params,jsonreq.id);
		} else {
			onServerResponse(jsonreq.result,jsonreq.error,jsonreq.id,jsonreq.context);
		}
	}
	
	var callMethod = function(method,params) {
		return new Promise(function(accept,reject) {
			var id = lastIdent++;
			var req = {method:method,params:params,id:id,context:me.context};
			
			idtable[id] = {request:req,accept:accept,reject:reject};
			if (opened) sendRequest(req);
		});
	}
	
	var cleanup = function(keeprq) {
		if (!keeprq) cancelRequests();
		connection = null;
		opened = false;
	}
	
	var disconnect = function(keeprq) {
		connection.close();
		cleanup(keeprq);
	}
	
	

  var updateContext = function(c) {
  	var changed = false;
		for (var i in c) {
			changed = true;
			if (c[i] === null) delete me.context[i];
			else me.context[i] = c[i];
		}
  }
	
  this.updateContext = function(c) {
	  updateContext(c);
  }
  
	var regMethod = function(obj, locname, remotename) {
		var k = locname.indexOf('.');
		var r = remotename;
		if (k == -1) {
			obj[locname] = function() {    				
				var args = new Array();    				
				for (var x =0; x < arguments.length;x++) {
					args.push(arguments[x]);
				}
			    return callMethod(r,args);
			}
		} else {
			var subloc = locname.substr(0,k);
			var outloc = locname.substr(k+1);
			if (!(subloc in obj)) obj[subloc] = new Object();
			regMethod(obj[subloc],outloc,remotename);
		}    		
	}
	
	
	method_list.forEach(function(name) {
			regMethod(me,name,name);
	});

	if (options) {
		if (options.hasOwnProperty("onConnectionError")) 
			this.onConnectionError = options.onConnectionError;
	}
	
	this.context = new Object();
	this.call = callMethod;
	this.connect = function() {
		connect();
	}
	this.close = function() {
		reconnectIfNeed = false;
		disconnect();
	}
	this.addEventListener = function(event, fn) {
		var e = eventListeners[event];
		if (!e) e = (eventListeners[event] = []);
		e.push(fn);
		return this;
	}
	
	this.removeEventListener = function(event, fn) {
		var e = eventListeners[event];
		if (!e) return this;
		var idx = e.indexOf(fn);
		if (idx != -1) array.splice(idx, 1);
	}
};

///Default behaviour for http-error. You can write own handler
/**
* @param status status code - read from XMLHttpRequest()
* @param request whole request object. 
* @param resolve callback function to resolve this situation. If called with true, then request is repeated.
*   if called with false, then request is failed. If called with object, then object must be formatted as
*   standard JSON response. This response is then used to fullfil or reject apropriate promise on that request.
*
* @note Note that for Server.multicall call, you should return apropriate response
* 
*/
WsRpcClient.prototype.onConnectionError = function(status,resolve) {
	setTimeout(function() {
		resolve(confirm("Can't finish operation - webSocket Error: " + status + ". Retry?"));}
	,1);
}


///canceled multicall request
WsRpcClient.CANCELED = "canceled";
WsRpcClient.FAILED = "failed";

///Extends promise - function store
/** Stores result to object under specified key once the promise is filled.  */
Promise.prototype.store = function(obj,name) {
	return this.then(function (v) {
		obj[name] = v;
		return v;
	});
}

///Extends promise - function log
/** Dumps result to console  */
Promise.prototype.log = function() {
	return this.then(function (v) {
		console.log(v)
		return v;
	},function(e) {
		console.error(e)
		return e;		
	});
}

