################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../tests/handler_creation.c \
../tests/reliable_client.c \
../tests/reliable_server.c 

OBJS += \
./tests/handler_creation.o \
./tests/reliable_client.o \
./tests/reliable_server.o 

C_DEPS += \
./tests/handler_creation.d \
./tests/reliable_client.d \
./tests/reliable_server.d 


# Each subdirectory must supply rules for building sources it contributes
tests/%.o: ../tests/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


