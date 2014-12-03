################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../httpserver/ConnHandler.cpp \
../httpserver/HttpReqImpl.cpp \
../httpserver/JobSchedulerImpl.cpp \
../httpserver/httpServer.cpp \
../httpserver/pathMapper.cpp \
../httpserver/queryParser.cpp \
../httpserver/sha1.cpp \
../httpserver/simpleWebSite.cpp \
../httpserver/statBuffer.cpp \
../httpserver/webSockets.cpp 

OBJS += \
./httpserver/ConnHandler.o \
./httpserver/HttpReqImpl.o \
./httpserver/JobSchedulerImpl.o \
./httpserver/httpServer.o \
./httpserver/pathMapper.o \
./httpserver/queryParser.o \
./httpserver/sha1.o \
./httpserver/simpleWebSite.o \
./httpserver/statBuffer.o \
./httpserver/webSockets.o 

CPP_DEPS += \
./httpserver/ConnHandler.d \
./httpserver/HttpReqImpl.d \
./httpserver/JobSchedulerImpl.d \
./httpserver/httpServer.d \
./httpserver/pathMapper.d \
./httpserver/queryParser.d \
./httpserver/sha1.d \
./httpserver/simpleWebSite.d \
./httpserver/statBuffer.d \
./httpserver/webSockets.d 


# Each subdirectory must supply rules for building sources it contributes
httpserver/%.o: ../httpserver/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	$(GPP) -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


