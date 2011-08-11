################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../kernel/interceptors/aggregation/aggregation.c \
../kernel/interceptors/aggregation/byte_buffer.c 

OBJS += \
./kernel/interceptors/aggregation/aggregation.o \
./kernel/interceptors/aggregation/byte_buffer.o 

C_DEPS += \
./kernel/interceptors/aggregation/aggregation.d \
./kernel/interceptors/aggregation/byte_buffer.d 


# Each subdirectory must supply rules for building sources it contributes
kernel/interceptors/aggregation/%.o: ../kernel/interceptors/aggregation/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


