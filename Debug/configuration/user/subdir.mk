################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../configuration/user/configuration_user_communication.c 

OBJS += \
./configuration/user/configuration_user_communication.o 

C_DEPS += \
./configuration/user/configuration_user_communication.d 


# Each subdirectory must supply rules for building sources it contributes
configuration/user/%.o: ../configuration/user/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


