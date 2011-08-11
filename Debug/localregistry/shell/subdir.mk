################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../localregistry/shell/argparse.c \
../localregistry/shell/cmd.c \
../localregistry/shell/main.c 

OBJS += \
./localregistry/shell/argparse.o \
./localregistry/shell/cmd.o \
./localregistry/shell/main.o 

C_DEPS += \
./localregistry/shell/argparse.d \
./localregistry/shell/cmd.d \
./localregistry/shell/main.d 


# Each subdirectory must supply rules for building sources it contributes
localregistry/shell/%.o: ../localregistry/shell/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


