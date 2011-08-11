################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../localregistry/user/registry_ops.c 

OBJS += \
./localregistry/user/registry_ops.o 

C_DEPS += \
./localregistry/user/registry_ops.d 


# Each subdirectory must supply rules for building sources it contributes
localregistry/user/%.o: ../localregistry/user/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


