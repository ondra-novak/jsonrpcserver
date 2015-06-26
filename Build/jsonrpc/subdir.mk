################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../jsonrpc/JSONRPCMain.cpp \
../jsonrpc/jsonRpc.cpp \
../jsonrpc/jsonRpcServer.cpp \
../jsonrpc/jsonRpcWebsockets.cpp \
../jsonrpc/restHandler.cpp \
../jsonrpc/rpchandler.cpp 

OBJS += \
./jsonrpc/JSONRPCMain.o \
./jsonrpc/jsonRpc.o \
./jsonrpc/jsonRpcServer.o \
./jsonrpc/jsonRpcWebsockets.o \
./jsonrpc/restHandler.o \
./jsonrpc/rpchandler.o 

CPP_DEPS += \
./jsonrpc/JSONRPCMain.d \
./jsonrpc/jsonRpc.d \
./jsonrpc/jsonRpcServer.d \
./jsonrpc/jsonRpcWebsockets.d \
./jsonrpc/restHandler.d \
./jsonrpc/rpchandler.d 


# Each subdirectory must supply rules for building sources it contributes
jsonrpc/%.o: ../jsonrpc/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	$(GPP) -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


