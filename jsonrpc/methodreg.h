#pragma once


namespace jsonrpc {

using namespace LightSpeed;


///Abstract method handler
class IMethod: public RefCntObj {
public:

	///Calls the method
	/**
	 * @param request request from the client, also contains the parameters
	 * @return future response which might be resolved with a result
	 */
	virtual Future<Response> operator()(const Request &) const throw() = 0;
	virtual ~IMethod() {}


};

///Abstract exception handler
/** This exception handler is called, when method throws an exception which is unknown
 * to the dispatcher. The exception handler can transform exception to an RpcException, or
 * it can return a default response after the exception
 */
class IExceptionHandler: public RefCntObj {
public:
	///Handler is executed with reference to pointer to an exception
	/**
	 * @param request pointer to the request
	 * @param exception pointer to exception
	 * @return future response. If handler is unable to handle exception, special object
	 *  Future(null) have to be returned
	 */
	virtual Future<Response> operator()(const Request &, const PException &) const throw() = 0;
	virtual ~IExceptionHandler() {}
};




class IMethodRegister: public virtual IInterface{
public:


	///Register method handler
	/**
	 * @param upToVersion numeric specification maximum version where method is defined. It will
	 *  not appear in newest version. To declare method without version, specify naturalNull there
	 * @param method method name (and optionally, format of arguments).
	 * @param fn pointer to object, which will handle this method call
	 * @param untilVer version when this method has been removed from the api. If there
	 *  is same method with higher version, it will overwrite old method from specified version
	 *  above.
	 *
	 * @note method can contain description of arguments. Currently, only old system is implemented
	 * with following modifications
	 * @code
	 *    s - string
	 *    n - number
	 *    b - bool
	 *    x - null
	 *    d - string or number
	 *    a - array
	 *    o - object (not class)
	 *    u - undefined
	 *    * - repeated last
	 *    ? - any
	 * @endcode
	 *
	 * there can be comma , to create alternatives "ss,ssn,ssnb,ssb"
	 *
	 *
	 *
	 * @return function returns refernce to interface which allows to control various
	 * properties of currently registered method. See IMethodProperties
	 *
	 */
	virtual void regMethodHandler(natural upToVersion, ConstStrA method, IMethod *fn) = 0;


	///Unregister method (you must supply correct arguments as well)
	/**
	 * @param method method name (including arguments)
	 * @param ver version (must be equal to version used during registration)
	 */
	virtual void unregMethod(ConstStrA method) = 0;


	virtual void regExceptionHandler(ConstStrA name, IExceptionHandler *fn) = 0;
	virtual void unregExceptionHandler(ConstStrA name) = 0;


    class IMethodEnum {
    public:
    	virtual void operator()(ConstStrA prototype) const = 0;
    };

    virtual void enumMethods(const IMethodEnum &enm, natural version) const  = 0;


    ///change log object
    /**
     * @param logObject use null, to disable logging
     */
    virtual void setLogObject(ILog *logObject) = 0;

    ///Register method which calls a function
    /**
     * @param method method prototype, see regMethodHandler()
     * @param fn reference to function to call. The regMethod() function will make copy of the argument.
     *   You can use lambda function here
     */
    template<typename Fn>
    void regMethod(ConstStrA method,const Fn &fn);
    ///Register method which calls a member function on an object
    /**
     * @param method method prototype, see regMethodHandler()
     * @param objPtr pointer to the object. It can be a smart pointer, because it is managed by value
     * @param fn reference to member function to call
     */
    template<typename ObjPtr, typename Obj, typename Ret>
    void regMethod(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &));

    ///Register method which calls a member function on an object and brings a static argument
    /**
     * @param method method prototype, see regMethodHandler()
     * @param objPtr pointer to the object. It can be a smart pointer, because it is managed by value
     * @param fn reference to member function to call
     * @param arg an argument, which is passed as second argument of the member function
     */
    template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
    void regMethod(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &, const Arg &arg), const Arg &arg);


    ///Register method which calls a function limited to a specified version
    /**
     * @param upToVersion max version number. It won't be visible in newer versions
     * @param method method prototype, see regMethodHandler()
     * @param fn reference to function to call. The regMethod() function will make copy of the argument.
     *   You can use lambda function here
     */
    template<typename Fn>
    void regMethod(natural upToVersion, ConstStrA method,const Fn &fn);

    ///Register method which calls a member function on an object limited to a specified version
    /**
     * @param upToVersion max version number. It won't be visible in newer versions
     * @param method method prototype, see regMethodHandler()
     * @param objPtr pointer to the object. It can be a smart pointer, because it is managed by value
     * @param fn reference to member function to call
     */
    template<typename ObjPtr, typename Obj, typename Ret>
    void regMethod(natural upToVersion, ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &));
    ///Register method which calls a member function on an object and brings a static argument limited to a specified version
    /**
     * @param upToVersion max version number. It won't be visible in newer versions
     * @param method method prototype, see regMethodHandler()
     * @param objPtr pointer to the object. It can be a smart pointer, because it is managed by value
     * @param fn reference to member function to call
     * @param arg an argument, which is passed as second argument of the member function
     */
    template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
    void regMethod(natural upToVersion, ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &, const Arg &arg), const Arg &arg);

    template<typename Fn>
    void regException(ConstStrA method,const Fn &fn);
    template<typename ObjPtr, typename Obj, typename Ret>
    void regException(ConstStrA method, const ObjPtr &objPtr, bool (Obj::*fn)(const Request &, const PException &, Promise<Ret> ));
    template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
    void regException(ConstStrA method, const ObjPtr &objPtr, bool (Obj::*fn)(const Request &, const PException &, Promise<Ret>, const Arg &arg), const Arg &arg);


    template<typename ObjPtr, typename Obj, typename Ret>
    struct MethodCallMemberObject;
    template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
    struct MethodCallMemberObjectArg;
    template<typename ObjPtr, typename Obj, typename Ret>
    struct ExceptionCallMemberObject;
    template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
    struct ExceptionCallMemberObjectArg;


};



}

