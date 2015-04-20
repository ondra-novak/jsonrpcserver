################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../client/client.cpp \
../client/clientAdapter.cpp \
../client/simpleHttpClient.cpp 

OBJS += \
./client/client.o \
./client/clientAdapter.o \
./client/simpleHttpClient.o 

CPP_DEPS += \
./client/client.d \
./client/clientAdapter.d \
./client/simpleHttpClient.d 


# Each subdirectory must supply rules for building sources it contributes
client/%.o: ../client/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	$(GPP) -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


