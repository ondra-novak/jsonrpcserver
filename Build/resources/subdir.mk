################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../resources/rpc.js.c \
../resources/wsrpc.js.c 

OBJS += \
./resources/rpc.js.o \
./resources/wsrpc.js.o 

C_DEPS += \
./resources/rpc.js.d \
./resources/wsrpc.js.d 


# Each subdirectory must supply rules for building sources it contributes
resources/%.o: ../resources/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(GPP) -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


