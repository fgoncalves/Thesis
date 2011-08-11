################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../kernel/interceptors/synchronization/synchronization.c 

OBJS += \
./kernel/interceptors/synchronization/synchronization.o 

C_DEPS += \
./kernel/interceptors/synchronization/synchronization.d 


# Each subdirectory must supply rules for building sources it contributes
kernel/interceptors/synchronization/%.o: ../kernel/interceptors/synchronization/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


