################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../configuration/config.c \
../configuration/configuration_parser.c \
../configuration/configuration_socket.c \
../configuration/transport_config.c 

OBJS += \
./configuration/config.o \
./configuration/configuration_parser.o \
./configuration/configuration_socket.o \
./configuration/transport_config.o 

C_DEPS += \
./configuration/config.d \
./configuration/configuration_parser.d \
./configuration/configuration_socket.d \
./configuration/transport_config.d 


# Each subdirectory must supply rules for building sources it contributes
configuration/%.o: ../configuration/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


