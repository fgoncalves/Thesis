################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../interceptor_shell/cmd_line_parser.c \
../interceptor_shell/mkrule.c \
../interceptor_shell/operations.c \
../interceptor_shell/rmrule.c 

OBJS += \
./interceptor_shell/cmd_line_parser.o \
./interceptor_shell/mkrule.o \
./interceptor_shell/operations.o \
./interceptor_shell/rmrule.o 

C_DEPS += \
./interceptor_shell/cmd_line_parser.d \
./interceptor_shell/mkrule.d \
./interceptor_shell/operations.d \
./interceptor_shell/rmrule.d 


# Each subdirectory must supply rules for building sources it contributes
interceptor_shell/%.o: ../interceptor_shell/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


