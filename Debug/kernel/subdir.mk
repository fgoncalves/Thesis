################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../kernel/chains.c \
../kernel/filter.c \
../kernel/hooks.c \
../kernel/interceptor_manager.c \
../kernel/klist.c \
../kernel/main.c \
../kernel/proc_entry.c \
../kernel/proc_registry.c \
../kernel/rule.c \
../kernel/rule_manager.c 

OBJS += \
./kernel/chains.o \
./kernel/filter.o \
./kernel/hooks.o \
./kernel/interceptor_manager.o \
./kernel/klist.o \
./kernel/main.o \
./kernel/proc_entry.o \
./kernel/proc_registry.o \
./kernel/rule.o \
./kernel/rule_manager.o 

C_DEPS += \
./kernel/chains.d \
./kernel/filter.d \
./kernel/hooks.d \
./kernel/interceptor_manager.d \
./kernel/klist.d \
./kernel/main.d \
./kernel/proc_entry.d \
./kernel/proc_registry.d \
./kernel/rule.d \
./kernel/rule_manager.d 


# Each subdirectory must supply rules for building sources it contributes
kernel/%.o: ../kernel/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


