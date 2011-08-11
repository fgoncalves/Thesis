################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../kernel/interceptors/reliable_transport/connection.c \
../kernel/interceptors/reliable_transport/hooks.c \
../kernel/interceptors/reliable_transport/packet_buffer.c 

OBJS += \
./kernel/interceptors/reliable_transport/connection.o \
./kernel/interceptors/reliable_transport/hooks.o \
./kernel/interceptors/reliable_transport/packet_buffer.o 

C_DEPS += \
./kernel/interceptors/reliable_transport/connection.d \
./kernel/interceptors/reliable_transport/hooks.d \
./kernel/interceptors/reliable_transport/packet_buffer.d 


# Each subdirectory must supply rules for building sources it contributes
kernel/interceptors/reliable_transport/%.o: ../kernel/interceptors/reliable_transport/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


