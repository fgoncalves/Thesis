################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../kernel/interceptors/deaggregation/deaggregation.c \
../kernel/interceptors/deaggregation/injection_thread.c \
../kernel/interceptors/deaggregation/spin_lock_queue.c 

OBJS += \
./kernel/interceptors/deaggregation/deaggregation.o \
./kernel/interceptors/deaggregation/injection_thread.o \
./kernel/interceptors/deaggregation/spin_lock_queue.o 

C_DEPS += \
./kernel/interceptors/deaggregation/deaggregation.d \
./kernel/interceptors/deaggregation/injection_thread.d \
./kernel/interceptors/deaggregation/spin_lock_queue.d 


# Each subdirectory must supply rules for building sources it contributes
kernel/interceptors/deaggregation/%.o: ../kernel/interceptors/deaggregation/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


