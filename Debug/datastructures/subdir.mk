################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../datastructures/rbtree.c 

OBJS += \
./datastructures/rbtree.o 

C_DEPS += \
./datastructures/rbtree.d 


# Each subdirectory must supply rules for building sources it contributes
datastructures/%.o: ../datastructures/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


